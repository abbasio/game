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
static win_32_offscreen_buffer GlobalBackBuffer;
static LPDIRECTSOUNDBUFFER GlobalSecondaryBuffer;

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
            BufferDescription.dwFlags = 0;
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
static void Win32ResizeDIBSection(win_32_offscreen_buffer *Buffer, int Width, int Height)
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

static void Win32DisplayBufferInWindow(win_32_offscreen_buffer *Buffer, 
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
        case WM_SIZE: 
        {
            break;
        } 
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

int CALLBACK WinMain(
        HINSTANCE hInstance, 
        HINSTANCE hPrevInstance, 
        LPSTR lpCmdLine, 
        int nCmdShow) 
{
    LARGE_INTEGER PerfCountFrequencyResult;
    QueryPerformanceFrequency(&PerfCountFrequencyResult);
    int64_t PerfCountFrequency = PerfCountFrequencyResult.QuadPart;
    
    Win32LoadXInput();

    WNDCLASSA WindowClass = {};
    
    Win32ResizeDIBSection(&GlobalBackBuffer, 1280, 720);
    
    WindowClass.style = CS_HREDRAW|CS_VREDRAW|CS_OWNDC;
    WindowClass.lpfnWndProc = Win32MainWindowCallback;
    WindowClass.hInstance = hInstance;
    //WindowClass.hIcon;
    WindowClass.lpszClassName = "GameWindowClass";


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
            SoundOutput.LatencySampleCount = SoundOutput.SamplesPerSecond / 15;

            Win32InitDSound(Window, SoundOutput.SamplesPerSecond, SoundOutput.SecondaryBufferSize);
            Win32ClearSoundBuffer(&SoundOutput);
            GlobalSecondaryBuffer->Play(0, 0, DSBPLAY_LOOPING);
            
            GlobalRunning = true;
            
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

                LARGE_INTEGER LastCounter;
                QueryPerformanceCounter(&LastCounter);
                uint64_t LastCycleCount = __rdtsc();
                while(GlobalRunning)
                { 
                    MSG Message;

                    game_controller_input *KeyboardController = &NewInput->Controllers[0];
                    // TODO: zeroing macro
                    game_controller_input ZeroController = {};
                    *KeyboardController = ZeroController;
                    
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
                                    if(VKCode == VK_UP)
                                    {
                                        Win32ProcessKeyboardMessage(&KeyboardController->Up, IsDown);
                                    } 
                                    else if(VKCode == VK_DOWN)
                                    {
                                        Win32ProcessKeyboardMessage(&KeyboardController->Down, IsDown);
                                    }
                                    else if(VKCode == VK_LEFT)
                                    {
                                        Win32ProcessKeyboardMessage(&KeyboardController->Left, IsDown);
                                    }
                                    else if(VKCode == VK_RIGHT)
                                    {
                                        Win32ProcessKeyboardMessage(&KeyboardController->Right, IsDown);
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
                    
                    
                    int MaxControllerCount = XUSER_MAX_COUNT;
                    if(MaxControllerCount > ArrayCount(NewInput->Controllers))
                    {
                        MaxControllerCount = ArrayCount(NewInput->Controllers);
                    }
                    for (DWORD ControllerIndex = 0; ControllerIndex < XUSER_MAX_COUNT; ++ControllerIndex)
                    {
                        game_controller_input *OldController = &OldInput->Controllers[ControllerIndex];
                        game_controller_input *NewController = &NewInput->Controllers[ControllerIndex];

                        XINPUT_STATE ControllerState;
                        if(XInputGetState(ControllerIndex, &ControllerState) == ERROR_SUCCESS)
                        {
                            // controller is plugged in
                            //ControllerState.dwPacketNumber
                            XINPUT_GAMEPAD *Pad = &ControllerState.Gamepad;

                            bool DPadUp = (Pad->wButtons & XINPUT_GAMEPAD_DPAD_UP);
                            bool DPadDown = (Pad->wButtons & XINPUT_GAMEPAD_DPAD_DOWN);
                            bool DPadLeft = (Pad->wButtons & XINPUT_GAMEPAD_DPAD_LEFT);
                            bool DPadRight = (Pad->wButtons & XINPUT_GAMEPAD_DPAD_RIGHT);
                        
                            NewController->IsAnalog = true;
                            NewController->StartX = OldController->EndX;
                            NewController->StartY = OldController->EndY;
                            
                            // Normalize stick value
                            float X;
                            if(Pad->sThumbLX < 0)
                            {
                                X = (float)Pad->sThumbLX / 32768.0f;
                            }
                            else
                            {
                                X = (float)Pad->sThumbLX / 32767.0f;
                            }
                            
                            NewController->MinX = NewController->MaxX = NewController->EndX = X;

                            float Y;
                            if(Pad->sThumbLY < 0)
                            {
                                Y = (float)Pad->sThumbLY / 32768.0f;
                            }
                            else
                            {
                                Y = (float)Pad->sThumbLY / 32767.0f;
                            }

                            NewController->MinY = NewController->MaxY = NewController->EndY = Y;
                        
                            //int16_t LeftStickX = (float)Pad->sThumbLX / ;
                            //int16_t LeftStickY = (float)Pad->sThumbLY;
                            
                            Win32ProcessXInputDigitalButton(Pad->wButtons, &OldController->Down, XINPUT_GAMEPAD_A, &NewController->Down);
                            Win32ProcessXInputDigitalButton(Pad->wButtons, &OldController->Right, XINPUT_GAMEPAD_B, &NewController->Right);
                            Win32ProcessXInputDigitalButton(Pad->wButtons, &OldController->Left, XINPUT_GAMEPAD_X, &NewController->Left);
                            Win32ProcessXInputDigitalButton(Pad->wButtons, &OldController->Up, XINPUT_GAMEPAD_Y, &NewController->Up);
                            Win32ProcessXInputDigitalButton(Pad->wButtons, &OldController->LeftShoulder, XINPUT_GAMEPAD_LEFT_SHOULDER, &NewController->LeftShoulder);
                            Win32ProcessXInputDigitalButton(Pad->wButtons, &OldController->RightShoulder, XINPUT_GAMEPAD_RIGHT_SHOULDER, &NewController->RightShoulder);
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
                        }
                    }

                    // DirectSound output test
                    // The following uses a ring buffer
                    // Write cursor must stay ahead of the play cursor
                    DWORD ByteToLock = 0;
                    DWORD TargetCursor;
                    DWORD BytesToWrite = 0;
                    DWORD PlayCursor;
                    DWORD WriteCursor;
                    bool SoundIsValid = false;
                    if(SUCCEEDED(GlobalSecondaryBuffer->GetCurrentPosition(&PlayCursor, &WriteCursor)))
                    {   
                        ByteToLock = (SoundOutput.RunningSampleIndex*SoundOutput.BytesPerSample) % SoundOutput.SecondaryBufferSize;
                        TargetCursor = ((PlayCursor + (SoundOutput.LatencySampleCount*SoundOutput.BytesPerSample)) % SoundOutput.SecondaryBufferSize);
                        // ring buffer - if write byte is behind play cursor, write up to it
                        // if write byte is ahead of play cursor, write to end of buffer and loop around 
                        // continue up to the play buffer
                        if(ByteToLock > TargetCursor)
                        {
                            BytesToWrite = (SoundOutput.SecondaryBufferSize - ByteToLock);
                            BytesToWrite += TargetCursor;
                        }
                        else
                        {
                            BytesToWrite = TargetCursor - ByteToLock;
                        }
                        
                        SoundIsValid = true;
                    }
                    
                    game_sound_output_buffer SoundBuffer = {};
                    SoundBuffer.SamplesPerSecond = SoundOutput.SamplesPerSecond;
                    SoundBuffer.SampleCount  = BytesToWrite / SoundOutput.BytesPerSample;
                    SoundBuffer.Samples = Samples; 
                    
                    game_offscreen_buffer Buffer = {};
                    Buffer.Memory = GlobalBackBuffer.Memory;
                    Buffer.Width = GlobalBackBuffer.Width;
                    Buffer.Height = GlobalBackBuffer.Height;
                    Buffer.Pitch = GlobalBackBuffer.Pitch;
                    GameUpdateAndRender(&GameMemory, NewInput,  &Buffer, &SoundBuffer); 
                    
                    if(SoundIsValid)
                    {
                        Win32FillSoundBuffer(&SoundOutput, ByteToLock, BytesToWrite, &SoundBuffer);
                    }
                    win32_window_dimension Dimension = Win32GetWindowDimension(Window);
                    Win32DisplayBufferInWindow(&GlobalBackBuffer, DeviceContext, Dimension.Width, Dimension.Height);
                    
                    uint64_t EndCycleCount = __rdtsc();
                    
                    LARGE_INTEGER EndCounter;
                    QueryPerformanceCounter(&EndCounter);
                    
                    uint64_t CyclesElapsed = EndCycleCount - LastCycleCount;
                    int64_t CounterElapsed = EndCounter.QuadPart - LastCounter.QuadPart;
                    int64_t MSPerFrame = ((1000*CounterElapsed) / PerfCountFrequency);
                    int64_t FPS = PerfCountFrequency / CounterElapsed;
                    int32_t MegaCyclesPerFrame = (int32_t)(CyclesElapsed / (1000 * 1000));

                    /* char Buffer[256]; 
                    wsprintf(Buffer, "%d ms/frame, %d fps, %d megacycles/frame\n", MSPerFrame, FPS, MegaCyclesPerFrame);
                    OutputDebugStringA(Buffer);
                    */
                    LastCounter = EndCounter;
                    LastCycleCount = EndCycleCount;

                    game_input *Temp = NewInput;
                    NewInput = OldInput;
                    OldInput = Temp;
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
