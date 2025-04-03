#include <math.h>
#include <stdint.h>
#include <windows.h>

#define Pi32 3.14159265359f

#include "game.h"
#include "game.cpp"

#include <xinput.h>
#include <dsound.h>

#include <stdio.h>

struct win_32_offscreen_buffer 
{
    BITMAPINFO Info;
    void *Memory;
    int Width;
    int Height;
    int Pitch;
    int BytesPerPixel;
};


struct win32_window_dimension 
{
    int Width;
    int Height;
};
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


static void * PlatformLoadFile(char *FileName)
{
    // Implements Win32 file loading
    return(0);
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
            
            if(SUCCEEDED(ERROR))
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
            uint32_t VKCode = wParam;
            bool WasDown = ((lParam & (1 << 30)) != 0); // lParam is a bit field, 30th bit lets us know if the key was down or up before this message was triggered
            bool IsDown = ((lParam & (1 << 31)) == 0); // 31st bit is transition state (0 for key down message, 1 for key up message)
            if(WasDown != IsDown) // avoid key repeat message
            {
                if(VKCode == VK_UP)
                {   
                } 
                else if(VKCode == VK_DOWN)
                {   
                } 
                else if(VKCode == VK_LEFT)
                {   
                } 
                else if(VKCode == VK_RIGHT)
                {   
                } 
                else if(VKCode == VK_ESCAPE)
                { 
                } 
                else if(VKCode == VK_SPACE)
                {   
                } 
                
                bool AltKeyWasDown = ((lParam & (1 << 29)) != 0);
                if((VKCode == VK_F4) && AltKeyWasDown)
                {
                    GlobalRunning = false;
                }

                break;
            }
            
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

struct win32_sound_output
{
    
    int SamplesPerSecond;
    int ToneHz;
    int16_t ToneVolume;
    uint32_t RunningSampleIndex;
    int WavePeriod;
    int BytesPerSample;
    int SecondaryBufferSize;
    float tSine;
    int LatencySampleCount;
};

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
            
            int XOffset = 0;
            int YOffset = 0;
        
            win32_sound_output SoundOutput = {};
                
            SoundOutput.SamplesPerSecond = 48000;
            SoundOutput.ToneHz = 256;
            SoundOutput.ToneVolume = 3000;
            SoundOutput.RunningSampleIndex = 0;
            SoundOutput.WavePeriod = SoundOutput.SamplesPerSecond/SoundOutput.ToneHz;
            SoundOutput.BytesPerSample = sizeof(int16_t)*2;
            SoundOutput.SecondaryBufferSize = SoundOutput.SamplesPerSecond*SoundOutput.BytesPerSample;
            SoundOutput.LatencySampleCount = SoundOutput.SamplesPerSecond / 15;

            Win32InitDSound(Window, SoundOutput.SamplesPerSecond, SoundOutput.SecondaryBufferSize);
            Win32ClearSoundBuffer(&SoundOutput);
            GlobalSecondaryBuffer->Play(0, 0, DSBPLAY_LOOPING);
            
            GlobalRunning = true;
            
            int16_t *Samples = (int16_t *)VirtualAlloc(0, SoundOutput.SecondaryBufferSize, MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE);
            
            LARGE_INTEGER LastCounter;
            QueryPerformanceCounter(&LastCounter);
            uint64_t LastCycleCount = __rdtsc();
            while(GlobalRunning)
            {
                
                MSG Message;
                
                while(PeekMessage(&Message, 0, 0, 0, PM_REMOVE))
                {
                    if (Message.message == WM_QUIT)
                    {
                        GlobalRunning = false;
                    }   
                    
                    TranslateMessage(&Message);
                    DispatchMessageA(&Message);
                }
                
                for (DWORD ControllerIndex = 0; ControllerIndex < XUSER_MAX_COUNT; ++ControllerIndex)
                {
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
                        bool Start = (Pad->wButtons & XINPUT_GAMEPAD_START);
                        bool Back = (Pad->wButtons & XINPUT_GAMEPAD_BACK);
                        bool LeftShoulder = (Pad->wButtons & XINPUT_GAMEPAD_LEFT_SHOULDER);
                        bool RightShoulder = (Pad->wButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER);
                        bool AButton = (Pad->wButtons & XINPUT_GAMEPAD_A);
                        bool BButton = (Pad->wButtons & XINPUT_GAMEPAD_B);
                        bool XButton = (Pad->wButtons & XINPUT_GAMEPAD_X);
                        bool YButton = (Pad->wButtons & XINPUT_GAMEPAD_Y);

                        int16_t LeftStickX = Pad->sThumbLX;
                        int16_t LeftStickY = Pad->sThumbLY;
                        
                        int16_t RightStickX = Pad->sThumbRX;
                        int16_t RightStickY = Pad->sThumbRY;
                        
                        //XOffset += StickX >> 12; // 1000 0000 0000 0000 -> 0000 0000 0000 1000 (32768 -> 8)
                        XOffset += LeftStickX / 4096; 
                        YOffset -= LeftStickY / 4096; 

                        unsigned char LeftTrigger = Pad->bLeftTrigger;
                        unsigned char RightTrigger = Pad->bRightTrigger;

                        SoundOutput.ToneHz = 512 + (int)(256.0f*((float)RightStickY / 30000.0f));
                        SoundOutput.WavePeriod = SoundOutput.SamplesPerSecond/SoundOutput.ToneHz;
                    }
                    else 
                    {
                        // controller unavailable
                    }
                }

                // DirectSound output test
                // The following uses a ring buffer
                // Write cursor must stay ahead of the play cursor
                DWORD ByteToLock;
                DWORD TargetCursor;
                DWORD BytesToWrite;
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
                GameUpdateAndRender(&Buffer, XOffset, YOffset, &SoundBuffer, SoundOutput.ToneHz); 
                
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
                int32_t FPS = PerfCountFrequency / CounterElapsed;
                int32_t MegaCyclesPerFrame = (int32_t)(CyclesElapsed / (1000 * 1000));

                /* char Buffer[256]; 
                wsprintf(Buffer, "%d ms/frame, %d fps, %d megacycles/frame\n", MSPerFrame, FPS, MegaCyclesPerFrame);
                OutputDebugStringA(Buffer);
                */
                LastCounter = EndCounter;
                LastCycleCount = EndCycleCount;
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
