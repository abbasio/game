#include <math.h>
#include <stdint.h>
#include <windows.h>
#include <stdio.h>
#define Pi32 3.14159265359f

#include "game.h"
#include "game.cpp"

#include <xinput.h>
#include <dsound.h>

#include "win32_game.h"

// Setting up input functions up this way allows us to avoid linking dlls/libraries
// that might have incompatibilities, while maintaining the same names.

// XInputGetState
#define X_INPUT_GET_STATE(name) DWORD WINAPI name(DWORD dwUserIndex, XINPUT_STATE *pState) 
typedef X_INPUT_GET_STATE(x_input_get_state);
X_INPUT_GET_STATE(XInputGetStateStub)
{
    return(ERROR_DEVICE_NOT_CONNECTED);
}
static x_input_get_state *XInputGetState_ = XInputGetStateStub;
#define XInputGetState XInputGetState_

//XInputSetState
#define X_INPUT_SET_STATE(name) DWORD WINAPI name(DWORD dwUserIndex, XINPUT_VIBRATION *pVibration)
typedef X_INPUT_SET_STATE(x_input_set_state);
X_INPUT_SET_STATE(XInputSetStateStub)
{
    return(ERROR_DEVICE_NOT_CONNECTED);
}
static x_input_set_state *XInputSetState_ = XInputSetStateStub;
#define XInputSetState XInputSetState_

#define DIRECT_SOUND_CREATE(name) HRESULT WINAPI name(LPCGUID pcGuidDevice, LPDIRECTSOUND *ppDS, LPUNKNOWN pUnkOuter);
typedef DIRECT_SOUND_CREATE(direct_sound_create);

static bool GlobalRunning;
static win32_offscreen_buffer GlobalBackBuffer;
static LPDIRECTSOUNDBUFFER GlobalSecondaryBuffer;
static int64_t GlobalPerfCountFrequency;

static debug_read_file_result DEBUGPlatformReadEntireFile(char *FileName)
{
    debug_read_file_result Result = {};

    HANDLE FileHandle = CreateFileA(FileName, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0);
    if (FileHandle != INVALID_HANDLE_VALUE)
    {
        LARGE_INTEGER FileSize;
        if(GetFileSizeEx(FileHandle, &FileSize))
        {
            uint32_t FileSize32 = SafeTruncateUInt64(FileSize.QuadPart);
            Result.Contents = VirtualAlloc(0, FileSize.QuadPart, MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE);
            if(Result.Contents)
            {
                DWORD BytesRead;
                if (ReadFile(FileHandle, Result.Contents, FileSize32, &BytesRead, 0) && 
                    (FileSize32 == BytesRead))
                {
                    // File read successfully
                    Result.ContentsSize = FileSize32;
                }
                else
                {
                    DEBUGPlatformFreeFileMemory(Result.Contents);
                    Result.Contents = 0;
                }
            }
            else
            {
                // logging
            }
        }
        else
        {
            // logging
        }
        CloseHandle(FileHandle);
    }
    else
    {
        // logging
    }

    return(Result);
}

static void DEBUGPlatformFreeFileMemory(void *Memory)
{
    if(Memory)
    {
        VirtualFree(Memory, 0, MEM_RELEASE);
    }
}
static bool DEBUGPlatformWriteEntireFile(char *FileName, uint32_t MemorySize, void *Memory)
{
    bool Result = false;

    HANDLE FileHandle = CreateFileA(FileName, GENERIC_WRITE, 0, 0, CREATE_ALWAYS, 0, 0);
    if (FileHandle != INVALID_HANDLE_VALUE)
    {
        DWORD BytesWritten;
        if (WriteFile(FileHandle, Memory, MemorySize, &BytesWritten, 0)) 
        {
            Result = (BytesWritten == MemorySize);
        }
        else
        {
            // logging
        }
        CloseHandle(FileHandle);
    }
    else
    {
        // logging
    }

    return(Result);
}

static void Win32LoadXInput(void)
{
     
    HMODULE XInputLibrary = LoadLibraryA("xinput1_4.dll");
    if(!XInputLibrary)
    {
        XInputLibrary = LoadLibraryA("xInput9_1_0.dll");
    }

    if(!XInputLibrary)
    {
        XInputLibrary = LoadLibraryA("xinput1_3.dll");
    }
    
    if(XInputLibrary)
    {
        XInputGetState = (x_input_get_state *) GetProcAddress(XInputLibrary, "XInputGetState");
        if(!XInputGetState) {XInputGetState = XInputGetStateStub;}
        
        XInputSetState = (x_input_set_state *) GetProcAddress(XInputLibrary, "XInputSetState");
        if(!XInputSetState) {XInputSetState = XInputSetStateStub;}
    }
}

static void Win32InitDSound(HWND Window, int32_t SamplesPerSecond, int32_t BufferSize)
{
    // load library
    HMODULE DSoundLibrary = LoadLibraryA("dsound.dll");
    if(DSoundLibrary)
    {
        direct_sound_create *DirectSoundCreate = (direct_sound_create *)
            GetProcAddress(DSoundLibrary, "DirectSoundCreate");
        
        LPDIRECTSOUND DirectSound;
        if(DirectSoundCreate && SUCCEEDED(DirectSoundCreate(0, &DirectSound, 0)))
        { 
            WAVEFORMATEX WaveFormat = {};
            WaveFormat.wFormatTag = WAVE_FORMAT_PCM;
            WaveFormat.nChannels = 2;
            WaveFormat.nSamplesPerSec = SamplesPerSecond;
            WaveFormat.wBitsPerSample = 16;
            WaveFormat.nBlockAlign = (WaveFormat.nChannels*WaveFormat.wBitsPerSample) / 8;
            WaveFormat.nAvgBytesPerSec = WaveFormat.nSamplesPerSec*WaveFormat.nBlockAlign;
            WaveFormat.cbSize = 0;

            if (SUCCEEDED(DirectSound->SetCooperativeLevel(Window, DSSCL_PRIORITY)))
            {
                DSBUFFERDESC BufferDescription = {};
                BufferDescription.dwSize = sizeof(BufferDescription);
                BufferDescription.dwFlags = DSBCAPS_PRIMARYBUFFER;
                
                LPDIRECTSOUNDBUFFER PrimaryBuffer;
                if(SUCCEEDED(DirectSound->CreateSoundBuffer(&BufferDescription, &PrimaryBuffer, 0)))
                {
                   
                    if(SUCCEEDED(PrimaryBuffer->SetFormat(&WaveFormat)))
                    {
                       OutputDebugStringA("Primary buffer format was set.\n"); 
                    }
                    else
                    {
                        // log
                    }
                }
                {
                    // log
                }
            }
            else
            {
                // log
            }

            DSBUFFERDESC BufferDescription = {};
            BufferDescription.dwSize = sizeof(BufferDescription);
            BufferDescription.dwFlags = DSBCAPS_GETCURRENTPOSITION2;
            BufferDescription.dwBufferBytes = BufferSize;
            BufferDescription.lpwfxFormat = &WaveFormat;

            HRESULT Error = DirectSound->CreateSoundBuffer(&BufferDescription, &GlobalSecondaryBuffer, 0);
            
            if(SUCCEEDED(Error))
            {
                OutputDebugStringA("Secondary buffer created successfully.\n"); 
            }
            else
            {
                // log
            }
        }
        else
        {
            // log
        }
    }
    else
    {
        // log
    }
}

win32_window_dimension Win32GetWindowDimension(HWND Window)
{
    win32_window_dimension Dimensions;

    RECT ClientRect;
    GetClientRect(Window, &ClientRect);
    Dimensions.Width = ClientRect.right - ClientRect.left;
    Dimensions.Height = ClientRect.bottom - ClientRect.top;
    
    return(Dimensions);
}


// DIB = Device Independent Bitmap
static void Win32ResizeDIBSection(win32_offscreen_buffer *Buffer, int Width, int Height)
{
    if(Buffer->Memory)
    {
        VirtualFree(Buffer->Memory, 0, MEM_RELEASE);     
    }

    Buffer->Width = Width;
    Buffer->Height = Height;

    Buffer->Info.bmiHeader.biSize = sizeof(Buffer->Info.bmiHeader);
    Buffer->Info.bmiHeader.biWidth = Buffer->Width;
    Buffer->Info.bmiHeader.biHeight = -Buffer->Height; // negative value means bitmap is top-down, not bottom-up
    Buffer->Info.bmiHeader.biPlanes = 1;
    // Note: 32 bits per pixel - 8 Red, 8 Green, 8 Blue, and a padding byte (8 bits)
    // Padding is to account for alignment (24 bits is only 3 bytes, want to align w 4 byte boundaries) 
    Buffer->Info.bmiHeader.biBitCount = 32;
    Buffer->Info.bmiHeader.biCompression = BI_RGB;
    
    Buffer->BytesPerPixel = 4;
    int BitmapMemorySize = (Width*Height)*Buffer->BytesPerPixel;
    Buffer->Memory = VirtualAlloc(0, BitmapMemorySize, MEM_COMMIT|MEM_RESERVE, PAGE_READWRITE);
     Buffer->Pitch = Width*Buffer->BytesPerPixel;
}

static void Win32DisplayBufferInWindow(win32_offscreen_buffer *Buffer, 
                                HDC DeviceContext, int WindowWidth, int WindowHeight)
{
    // Rectangle to Rectangle copy
    StretchDIBits(DeviceContext, 
        0, 0, WindowWidth, WindowHeight,
        0, 0, Buffer->Width, Buffer->Height,
        Buffer->Memory,
        &Buffer->Info,
        DIB_RGB_COLORS, SRCCOPY
    );
}

LRESULT CALLBACK Win32MainWindowCallback(HWND Window, UINT Message, WPARAM wParam, LPARAM lParam)
{
    LRESULT Result = 0;

    switch(Message) 
    {
        case WM_DESTROY: 
        {
            GlobalRunning = false;
            break;
        } 
        case WM_CLOSE: 
        {
            GlobalRunning = false;
            break;
        } 
        case WM_ACTIVATEAPP: 
        {
            OutputDebugStringA("WM_ACTIVATEAPP\n");
            break;
        }
        case WM_PAINT:
        {
            PAINTSTRUCT Paint;
            HDC DeviceContext = BeginPaint(Window, &Paint);
            int X = Paint.rcPaint.left;
            int Y = Paint.rcPaint.top;
            int Width = Paint.rcPaint.right - Paint.rcPaint.left;
            int Height = Paint.rcPaint.bottom - Paint.rcPaint.top;

            win32_window_dimension Dimension = Win32GetWindowDimension(Window);
            Win32DisplayBufferInWindow(&GlobalBackBuffer, DeviceContext, Dimension.Width, Dimension.Height);
            EndPaint(Window, &Paint);
            break;  
        }
        case WM_SYSKEYDOWN:
        case WM_SYSKEYUP:
        case WM_KEYDOWN:
        case WM_KEYUP:
        {
            Assert(!"Keyboard input came through non-dispatch message"); 
            break;
        }  
        default:
        {
            //OutputDebugStringA("default\n");
            Result = DefWindowProcA(Window, Message, wParam, lParam);
            break;
        } 
    }
    return(Result);
}


static void Win32ClearSoundBuffer(win32_sound_output *SoundOutput)
{
    VOID *Region1;
    DWORD Region1Size;
    VOID *Region2;
    DWORD Region2Size;
    if(SUCCEEDED(GlobalSecondaryBuffer->Lock(0, SoundOutput->SecondaryBufferSize, &Region1, &Region1Size, &Region2, &Region2Size, 0)))
    {
        uint8_t *DestSample = (uint8_t *)Region1;
        for(DWORD ByteIndex = 0; ByteIndex < Region1Size; ++ByteIndex)
        {
            *DestSample++ = 0;
        }
        
        DestSample = (uint8_t *)Region2; 
        for(DWORD ByteIndex = 0; ByteIndex < Region2Size; ++ByteIndex)
        {
            *DestSample++ = 0;
        }
        GlobalSecondaryBuffer->Unlock(Region1, Region1Size, Region2, Region2Size);
    }
}

static void Win32FillSoundBuffer(win32_sound_output *SoundOutput, DWORD ByteToLock, DWORD BytesToWrite, game_sound_output_buffer *SourceBuffer)
{
    VOID *Region1;
    DWORD Region1Size;
    VOID *Region2;
    DWORD Region2Size;
    
    if(SUCCEEDED(GlobalSecondaryBuffer->Lock(ByteToLock, BytesToWrite, &Region1, &Region1Size, &Region2, &Region2Size, 0)))
    {
        DWORD Region1SampleCount = Region1Size/SoundOutput->BytesPerSample;
        int16_t *DestSample = (int16_t *)Region1;
        int16_t *SourceSample = SourceBuffer->Samples; 
        for(DWORD SampleIndex  = 0; SampleIndex < Region1SampleCount; ++SampleIndex)
        {
            *DestSample++ = *SourceSample++;
            *DestSample++ = *SourceSample++;
            ++SoundOutput->RunningSampleIndex;
        }

        DWORD Region2SampleCount = Region2Size/SoundOutput->BytesPerSample;
        DestSample = (int16_t *)Region2;
        for(DWORD SampleIndex = 0; SampleIndex < Region2SampleCount; ++SampleIndex)
        {
            *DestSample++ = *SourceSample++;
            *DestSample++ = *SourceSample++;
            ++SoundOutput->RunningSampleIndex;
        }
        GlobalSecondaryBuffer->Unlock(Region1, Region1Size, Region2, Region2Size);
    }
}

static void Win32ProcessKeyboardMessage(game_button_state *NewState, bool IsDown)
{
    Assert(NewState->EndedDown != IsDown); // We should only get the state if nothing changed
    NewState->EndedDown = IsDown;
    ++NewState->HalfTransitionCount;
}

static void Win32ProcessXInputDigitalButton(DWORD XInputButtonState, 
                                            game_button_state *OldState, DWORD ButtonBit, 
                                            game_button_state *NewState)
{
    NewState->EndedDown = ((XInputButtonState & ButtonBit) == ButtonBit);
    ++NewState->HalfTransitionCount = (OldState->EndedDown != NewState->EndedDown) ? 1 : 0;
}

static float Win32ProcessXInputStickValue(SHORT Value, SHORT DeadZoneThreshold)
{
    float Result = 0;
    if(Value < -DeadZoneThreshold)
    {
        Result = (float)Value / 32768.0f;
    }
    else if(Value > DeadZoneThreshold)
    {
        Result = (float)Value / 32767.0f;
    }
    return Result;
}

static void Win32ProcessPendingMessages(game_controller_input *KeyboardController)
{

    MSG Message;
    while(PeekMessage(&Message, 0, 0, 0, PM_REMOVE))
    {
        switch(Message.message)
        {
            case WM_QUIT:
            {
                GlobalRunning = false;
            } break;
            
            case WM_SYSKEYDOWN:
            case WM_SYSKEYUP:
            case WM_KEYDOWN:
            case WM_KEYUP:
            {

                uint32_t VKCode = uint32_t(Message.wParam);
                bool WasDown = ((Message.lParam & (1 << 30)) != 0); // lParam is a bit field, 30th bit lets us know if the key was down or up before this message was triggered
                bool IsDown = ((Message.lParam & (1 << 31)) == 0); // 31st bit is transition state (0 for key down message, 1 for key up message)
                if(WasDown != IsDown) // avoid key repeat message
                {
                    if (VKCode == 'W')
                    {
                        Win32ProcessKeyboardMessage(&KeyboardController->MoveUp, IsDown);
                    }
                    else if (VKCode == 'A')
                    {
                        Win32ProcessKeyboardMessage(&KeyboardController->MoveLeft, IsDown);
                    }
                    else if (VKCode == 'S')
                    {
                        Win32ProcessKeyboardMessage(&KeyboardController->MoveDown, IsDown);
                    }
                    else if (VKCode == 'D')
                    {
                        Win32ProcessKeyboardMessage(&KeyboardController->MoveRight, IsDown);
                    }
                    else if(VKCode == VK_UP)
                    {
                        Win32ProcessKeyboardMessage(&KeyboardController->ActionUp, IsDown);
                    } 
                    else if(VKCode == VK_DOWN)
                    {
                        Win32ProcessKeyboardMessage(&KeyboardController->ActionDown, IsDown);
                    }
                    else if(VKCode == VK_LEFT)
                    {
                        Win32ProcessKeyboardMessage(&KeyboardController->ActionLeft, IsDown);
                    }
                    else if(VKCode == VK_RIGHT)
                    {
                        Win32ProcessKeyboardMessage(&KeyboardController->ActionRight, IsDown);
                    } 
                    else if(VKCode == VK_ESCAPE)
                    {
                        GlobalRunning = false; 
                    } 
                    else if(VKCode == VK_SPACE)
                    {   
                    }

                    bool AltKeyWasDown = ((Message.lParam & (1 << 29)) != 0);
                    if((VKCode == VK_F4) && AltKeyWasDown)
                    {
                        GlobalRunning = false;
                    }
                }
            } break;

            default:
            {
                TranslateMessage(&Message);
                DispatchMessageA(&Message);
            } break;
        }
        
    }
}

inline LARGE_INTEGER Win32GetWallClock()
{
    LARGE_INTEGER EndCounter;
    QueryPerformanceCounter(&EndCounter);
    return EndCounter;
}

inline float Win32GetSecondsElapsed(LARGE_INTEGER Start, LARGE_INTEGER End)
{
    float Result = (float)(End.QuadPart - Start.QuadPart) / (float)GlobalPerfCountFrequency;
    return Result;
}

static void Win32DebugDrawVertical(win32_offscreen_buffer *Buffer, int X, int Top, int Bottom, uint32_t Color)
{
        uint8_t *Pixel = (uint8_t *)((uint8_t *)Buffer->Memory + 
                                        X*Buffer->BytesPerPixel + 
                                        Top*Buffer->Pitch);
        for (int Y = Top; Y < Bottom; ++Y)
        {
            *(uint32_t *)Pixel = Color;
            Pixel += Buffer->Pitch;
        }
}

inline void Win32DrawSoundBufferMarker(win32_offscreen_buffer *Buffer, 
                                       win32_sound_output *SoundOutput,
                                       float C, int PadX, int Top, int Bottom, DWORD Value, uint32_t Color)
{
    Assert(Value < SoundOutput->SecondaryBufferSize);

    int X = PadX + (int)(C * (float)Value);
    Win32DebugDrawVertical(Buffer, X, Top, Bottom, Color);
}

static void Win32DebugSyncDisplay(win32_offscreen_buffer *Buffer, int MarkerCount, win32_debug_time_marker *Markers, 
                                win32_sound_output *SoundOutput, float TargetSecondsPerFrame)
{
    int PadX = 32;
    int PadY = 32;

    int Top = PadY;
    int Bottom = Buffer->Height - PadY;
    
    // Map sample buffer bytes to pixels for display
    // this looks fancy but all we're doing is making a visual representation of the sound buffer and play cursors
    float C = ((float)Buffer->Width - 2*PadX) / (float)(SoundOutput->SecondaryBufferSize);
    for(int MarkerIndex = 0; MarkerIndex < MarkerCount; ++MarkerIndex)
    {
        win32_debug_time_marker *ThisMarker = &Markers[MarkerIndex];
        Win32DrawSoundBufferMarker(Buffer, SoundOutput, C, PadX, Top, Bottom, ThisMarker->PlayCursor, 0xFFFFFFFF);
        Win32DrawSoundBufferMarker(Buffer, SoundOutput, C, PadX, Top, Bottom, ThisMarker->WriteCursor, 0xFFFF0000);
    }
}

int CALLBACK WinMain(
        HINSTANCE hInstance, 
        HINSTANCE hPrevInstance, 
        LPSTR lpCmdLine, 
        int nCmdShow) 
{
    LARGE_INTEGER PerfCountFrequencyResult;
    QueryPerformanceFrequency(&PerfCountFrequencyResult);
    GlobalPerfCountFrequency = PerfCountFrequencyResult.QuadPart;
   
    // Set Windows scheduler granularity to 1ms
    // Allows us to sleep for 1ms intervals
    UINT DesiredSchedulerMs = 1;
    boolean SleepIsGranular = (timeBeginPeriod(DesiredSchedulerMs) == TIMERR_NOERROR);    

    Win32LoadXInput();

    WNDCLASSA WindowClass = {};
    
    Win32ResizeDIBSection(&GlobalBackBuffer, 1280, 720);
    
    WindowClass.style = CS_HREDRAW|CS_VREDRAW|CS_OWNDC;
    WindowClass.lpfnWndProc = Win32MainWindowCallback;
    WindowClass.hInstance = hInstance;
    //WindowClass.hIcon;
    WindowClass.lpszClassName = "GameWindowClass";

    // TODO: Reliably query this on windows
#define MonitorRefreshHz 120
#define GameUpdateHz (MonitorRefreshHz / 2)
    float TargetSecondsPerFrame = 1.0f / (float)GameUpdateHz;

    if(RegisterClassA(&WindowClass)) 
    {
        // A = ANSI, W = unicode
        HWND Window = CreateWindowExA(
            0,
            WindowClass.lpszClassName,
            "game",
            WS_OVERLAPPEDWINDOW|WS_VISIBLE,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            0,
            0,
            hInstance,
            0);

        if (Window)
        {
            HDC DeviceContext = GetDC(Window);
            
            win32_sound_output SoundOutput = {};
                
            SoundOutput.SamplesPerSecond = 48000;
            SoundOutput.RunningSampleIndex = 0;
            SoundOutput.BytesPerSample = sizeof(int16_t)*2;
            SoundOutput.SecondaryBufferSize = SoundOutput.SamplesPerSecond*SoundOutput.BytesPerSample;
            SoundOutput.LatencySampleCount = SoundOutput.SamplesPerSecond / GameUpdateHz;
            // TODO: Compute this variance and get lowest reasonable value
            SoundOutput.SafetyBytes = ((SoundOutput.SamplesPerSecond*SoundOutput.BytesPerSample) / GameUpdateHz) / 3;

            Win32InitDSound(Window, SoundOutput.SamplesPerSecond, SoundOutput.SecondaryBufferSize);
            Win32ClearSoundBuffer(&SoundOutput);
            GlobalSecondaryBuffer->Play(0, 0, DSBPLAY_LOOPING);
            
            GlobalRunning = true;
#if 0
            // This tests the PlayCursor/WriteCursor update frequency
            // Came out to 480 samples of latency
            while(GlobalRunning)
            {
                DWORD DebugPlayCursor;
                DWORD DebugWriteCursor;
                GlobalSecondaryBuffer->GetCurrentPosition(&DebugPlayCursor, &DebugWriteCursor);

                char TextBuffer[256];
                _snprintf_s(TextBuffer, sizeof(TextBuffer),
                            "PC: %u, WC: %u\n", DebugPlayCursor, DebugWriteCursor);
                OutputDebugStringA(TextBuffer);
            }
#endif 
            int16_t *Samples = (int16_t *)VirtualAlloc(0, SoundOutput.SecondaryBufferSize, MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE);

#if GAME_INTERNAL
            LPVOID BaseAddress = (LPVOID)Terabytes((uint64_t)2); //In x64, the first 8TB of address space are reserved for the application
#else
            LPVOID BaseAddress = 0;
#endif
            game_memory GameMemory = {};
            GameMemory.PermanentStorageSize = Megabytes(64);
            GameMemory.TransientStorageSize = Gigabytes((uint64_t)1);
           
            // TODO: Handle different memory footprints
            uint64_t TotalSize = GameMemory.PermanentStorageSize + GameMemory.TransientStorageSize;

            GameMemory.PermanentStorage = VirtualAlloc(BaseAddress, TotalSize, MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE);
            GameMemory.TransientStorage = ((uint8_t *)GameMemory.PermanentStorage + GameMemory.PermanentStorageSize);


            if(Samples && GameMemory.PermanentStorage && GameMemory.TransientStorage)
            {
                game_input Input[2] = {};
                game_input *NewInput = &Input[0];
                game_input *OldInput = &Input[1];

                LARGE_INTEGER LastCounter = Win32GetWallClock();

                int DebugTimeMarkerIndex = 0;
                win32_debug_time_marker DebugTimeMarkers[GameUpdateHz / 2] = {0};

                bool SoundIsValid = false;
                DWORD AudioLatencyBytes = 0;
                float AudioLatencySeconds = 0;
                
                uint64_t LastCycleCount = __rdtsc();
                while(GlobalRunning)
                { 
                    game_controller_input *OldKeyboardController = GetController(OldInput, 0);
                    game_controller_input *NewKeyboardController = GetController(NewInput, 0);
                    // TODO: zeroing macro
                    game_controller_input ZeroController = {};
                    *NewKeyboardController = ZeroController;
                    NewKeyboardController->IsConnected = true;
                    for(int ButtonIndex = 0; ButtonIndex < ArrayCount(NewKeyboardController->Buttons); ++ButtonIndex)
                    {
                        NewKeyboardController->Buttons[ButtonIndex].EndedDown = OldKeyboardController->Buttons[ButtonIndex].EndedDown;
                    }

                    Win32ProcessPendingMessages(NewKeyboardController);
                    
                    DWORD MaxControllerCount = XUSER_MAX_COUNT;
                    if(MaxControllerCount > (ArrayCount(NewInput->Controllers) - 1))
                    {
                        MaxControllerCount = (ArrayCount(NewInput->Controllers) - 1);
                    }
                    for (DWORD ControllerIndex = 0; ControllerIndex < XUSER_MAX_COUNT; ++ControllerIndex)
                    {
                        
                        DWORD OurControllerIndex = ControllerIndex + 1; // 0 is the keyboard controller
                        game_controller_input *OldController = GetController(OldInput, OurControllerIndex);
                        game_controller_input *NewController = GetController(NewInput, OurControllerIndex);

                        XINPUT_STATE ControllerState;
                        if(XInputGetState(ControllerIndex, &ControllerState) == ERROR_SUCCESS)
                        {
                            // controller is plugged in
                            NewController->IsConnected = true;
                            //ControllerState.dwPacketNumber
                            XINPUT_GAMEPAD *Pad = &ControllerState.Gamepad;

                        
                            NewController->IsAnalog = true;
                            NewController->StickAverageX = Win32ProcessXInputStickValue(Pad->sThumbLX, XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE);
                            NewController->StickAverageY = Win32ProcessXInputStickValue(Pad->sThumbLY, XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE);
                            
                            if(Pad->wButtons & XINPUT_GAMEPAD_DPAD_UP)
                            {
                                NewController->StickAverageY = 1.0f;
                            }
                            if(Pad->wButtons & XINPUT_GAMEPAD_DPAD_DOWN)
                            {
                                NewController->StickAverageY = -1.0f;
                            }
                            if(Pad->wButtons & XINPUT_GAMEPAD_DPAD_LEFT)
                            {
                                NewController->StickAverageX = -1.0f;
                            }
                            if(Pad->wButtons & XINPUT_GAMEPAD_DPAD_RIGHT)
                            {
                                NewController->StickAverageX = 1.0f;
                            }

                            float Threshold = 0.5f;
                            Win32ProcessXInputDigitalButton(
                                NewController->StickAverageX < -Threshold ? 1 : 0,
                                &OldController->MoveLeft, 1,
                                &NewController->MoveLeft);
                            Win32ProcessXInputDigitalButton(
                                NewController->StickAverageX > Threshold ? 1 : 0,
                                &OldController->MoveRight, 1,
                                &NewController->MoveRight);
                            Win32ProcessXInputDigitalButton(
                                NewController->StickAverageY < -Threshold ? 1 : 0,
                                &OldController->MoveDown, 1,
                                &NewController->MoveDown);
                            Win32ProcessXInputDigitalButton(
                                NewController->StickAverageY > Threshold ? 1 : 0,
                                &OldController->MoveUp, 1,
                                &NewController->MoveUp);


                            Win32ProcessXInputDigitalButton(Pad->wButtons, &OldController->ActionDown, XINPUT_GAMEPAD_A, &NewController->ActionDown);
                            Win32ProcessXInputDigitalButton(Pad->wButtons, &OldController->ActionRight, XINPUT_GAMEPAD_B, &NewController->ActionRight);
                            Win32ProcessXInputDigitalButton(Pad->wButtons, &OldController->ActionLeft, XINPUT_GAMEPAD_X, &NewController->ActionLeft);
                            Win32ProcessXInputDigitalButton(Pad->wButtons, &OldController->ActionUp, XINPUT_GAMEPAD_Y, &NewController->ActionUp);
                            Win32ProcessXInputDigitalButton(Pad->wButtons, &OldController->LeftShoulder, XINPUT_GAMEPAD_LEFT_SHOULDER, &NewController->LeftShoulder);
                            Win32ProcessXInputDigitalButton(Pad->wButtons, &OldController->RightShoulder, XINPUT_GAMEPAD_RIGHT_SHOULDER, &NewController->RightShoulder);
                            Win32ProcessXInputDigitalButton(Pad->wButtons, &OldController->Start, XINPUT_GAMEPAD_START, &NewController->Start);
                            Win32ProcessXInputDigitalButton(Pad->wButtons, &OldController->Back, XINPUT_GAMEPAD_BACK, &NewController->Back);
                            //bool Start = (Pad->wButtons & XINPUT_GAMEPAD_START);
                            //bool Back = (Pad->wButtons & XINPUT_GAMEPAD_BACK);
                            
                            //int16_t RightStickX = Pad->sThumbRX;
                            //int16_t RightStickY = Pad->sThumbRY;
                            
                            //unsigned char LeftTrigger = Pad->bLeftTrigger;
                            //unsigned char RightTrigger = Pad->bRightTrigger;
                        }
                        else 
                        {
                            // controller unavailable
                            NewController->IsConnected = false;
                        }
                    }
                    
                    
                    
                    game_offscreen_buffer Buffer = {};
                    Buffer.Memory = GlobalBackBuffer.Memory;
                    Buffer.Width = GlobalBackBuffer.Width;
                    Buffer.Height = GlobalBackBuffer.Height;
                    Buffer.Pitch = GlobalBackBuffer.Pitch;
                    GameUpdateAndRender(&GameMemory, NewInput,  &Buffer); 
                    
                    DWORD PlayCursor;
                    DWORD WriteCursor;
                    if(GlobalSecondaryBuffer->GetCurrentPosition(&PlayCursor, &WriteCursor) == DS_OK)
                    {
                        /*
                            Sound output computation:

                            First, define a safety value - the number of samples we think our
                            game loop may vary by. (ex: 2ms)
                            
                            When we wake up to write audio, check the position of the play cursor.
                            Calculate where we think the play cursor will be on the next frame.

                            Check the position of the write cursor. If it is before the predicted play cursor 
                            by at least the safety value (ex: 2ms), then we will write audio up to the frame boundary, 
                            then for one additional frame. This gives us perfect audio sync for a sound card that has
                            low enough latency.

                            If the write cursor is after the safety value, then we assume we can never perfectly sync audio.
                            We will write one frame's worth of audio, plus the safety value worth of guard samples. 
                        */
                        
                        if(!SoundIsValid)
                        {
                            SoundOutput.RunningSampleIndex = WriteCursor / SoundOutput.BytesPerSample;
                            SoundIsValid = true;
                        }
                        
                        DWORD ByteToLock = (SoundOutput.RunningSampleIndex*SoundOutput.BytesPerSample) % SoundOutput.SecondaryBufferSize;
                        
                        DWORD  ExpectedSoundBytesPerFrame = 
                            (SoundOutput.SamplesPerSecond * SoundOutput.BytesPerSample) / GameUpdateHz;
                        DWORD ExpectedFrameBoundaryByte = PlayCursor + ExpectedSoundBytesPerFrame;

                        DWORD SafeWriteCursor = WriteCursor;
                        if(SafeWriteCursor < PlayCursor)
                        {
                            SafeWriteCursor += SoundOutput.SecondaryBufferSize;
                        }
                        Assert(SafeWriteCursor >= PlayCursor);
                        bool AudioCardIsLowLatency = (SafeWriteCursor < ExpectedFrameBoundaryByte);

                        DWORD TargetCursor = 0;
                        if(AudioCardIsLowLatency)
                        {
                            TargetCursor = (ExpectedFrameBoundaryByte + ExpectedSoundBytesPerFrame);
                        }
                        else
                        {
                            TargetCursor = (WriteCursor + ExpectedSoundBytesPerFrame + SoundOutput.SafetyBytes);
                        }
                        TargetCursor = (TargetCursor % SoundOutput.SecondaryBufferSize);

                        DWORD BytesToWrite = 0;
                        if(ByteToLock > TargetCursor)
                        {
                            BytesToWrite = (SoundOutput.SecondaryBufferSize - ByteToLock);
                            BytesToWrite += TargetCursor;
                        }
                        else
                        {
                            BytesToWrite = TargetCursor - ByteToLock;
                        }
                        
                        game_sound_output_buffer SoundBuffer = {};
                        SoundBuffer.SamplesPerSecond = SoundOutput.SamplesPerSecond;
                        SoundBuffer.SampleCount  = BytesToWrite / SoundOutput.BytesPerSample;
                        SoundBuffer.Samples = Samples; 
                        GameGetSoundSamples(&GameMemory, &SoundBuffer);
#if GAME_INTERNAL
                        // Audio latency in bytes: 5760 (~33ms)
                        DWORD DebugPlayCursor;
                        DWORD DebugWriteCursor;
                        GlobalSecondaryBuffer->GetCurrentPosition(&DebugPlayCursor, &DebugWriteCursor);

                        DWORD UnwrappedWriteCursor = DebugWriteCursor;
                        if(UnwrappedWriteCursor < DebugPlayCursor)
                        {
                            UnwrappedWriteCursor += SoundOutput.SecondaryBufferSize;
                        }
                        AudioLatencyBytes = UnwrappedWriteCursor - DebugPlayCursor;

                        AudioLatencySeconds = (float)(AudioLatencyBytes / SoundOutput.BytesPerSample) / (float)SoundOutput.SamplesPerSecond;

                        char TextBuffer[256];
                        _snprintf_s(TextBuffer, sizeof(TextBuffer),
                                    "PC: %u, WC: %u, DELTA: %u (%fs)\n",
                                    DebugPlayCursor, DebugWriteCursor, AudioLatencyBytes, AudioLatencySeconds);
                        OutputDebugStringA(TextBuffer);
#endif
                        Win32FillSoundBuffer(&SoundOutput, ByteToLock, BytesToWrite, &SoundBuffer);
                    }
                    else
                    {
                        SoundIsValid = false;
                    } 
                    LARGE_INTEGER WorkCounter = Win32GetWallClock();
                    float WorkSecondsElapsed = Win32GetSecondsElapsed(LastCounter, WorkCounter);

                    float SecondsElapsedForFrame = WorkSecondsElapsed;
                    if(SecondsElapsedForFrame < TargetSecondsPerFrame)
                    {
                        if(SleepIsGranular)
                        {
                            DWORD MsToSleep = (DWORD)(1000.0f * (TargetSecondsPerFrame - SecondsElapsedForFrame));
                            if(MsToSleep > 0)
                            {
                                Sleep(MsToSleep);
                            }
                        }
                        float TestSecondsElapsedForFrame = Win32GetSecondsElapsed(LastCounter, Win32GetWallClock());
                        if(TestSecondsElapsedForFrame < TargetSecondsPerFrame)
                        {
                            // Log missed sleep here
                        }
                        while(SecondsElapsedForFrame < TargetSecondsPerFrame)
                        {
                            SecondsElapsedForFrame = Win32GetSecondsElapsed(LastCounter, Win32GetWallClock());
                        }
                    } 
                    else
                    {
                        // TODO: Missed target frame rate
                        // logging
                    }
                    
                    LARGE_INTEGER EndCounter = Win32GetWallClock();
                    float MSPerFrame = 1000.0f * Win32GetSecondsElapsed(LastCounter, EndCounter);
                    LastCounter = EndCounter;   
                    
                    win32_window_dimension Dimension = Win32GetWindowDimension(Window);
#if GAME_INTERNAL
                    Win32DebugSyncDisplay(&GlobalBackBuffer, ArrayCount(DebugTimeMarkers), DebugTimeMarkers, &SoundOutput, TargetSecondsPerFrame);
#endif
                    Win32DisplayBufferInWindow(&GlobalBackBuffer, DeviceContext, Dimension.Width, Dimension.Height);

#if GAME_INTERNAL
                    {
                        Assert(DebugTimeMarkerIndex < ArrayCount(DebugTimeMarkers));
                        win32_debug_time_marker *Marker = &DebugTimeMarkers[DebugTimeMarkerIndex++];
                        if(DebugTimeMarkerIndex == ArrayCount(DebugTimeMarkers))
                        {
                            DebugTimeMarkerIndex = 0;
                        }
                        Marker->PlayCursor = PlayCursor;
                        Marker->WriteCursor = WriteCursor;
                    }
#endif
                    
                    game_input *Temp = NewInput;
                    NewInput = OldInput;
                    OldInput = Temp;
                    
                    uint64_t EndCycleCount = __rdtsc();
                    uint64_t CyclesElapsed = EndCycleCount - LastCycleCount;
                    LastCycleCount = EndCycleCount;

                    double FPS = 0.0f;
                    double MegaCyclesPerFrame = (double)(CyclesElapsed / (1000.0f * 1000.0f));
                    
                    char FPSBuffer[256]; 
                    _snprintf_s(FPSBuffer, sizeof(FPSBuffer), "%.02fms/f, %.02ff/s, %.02fmc/f\n", MSPerFrame, FPS, MegaCyclesPerFrame);
                    OutputDebugStringA(FPSBuffer);
                    
                    
                }
            }
            else
            {
                //logging
            }
        }
        else
        {
            // logging
        }
    } 
    else
    {
        // logging
    }

    return(0);
};