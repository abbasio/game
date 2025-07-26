#if !defined(WIN32_GAME_H)

struct win32_offscreen_buffer 
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

struct win32_sound_output
{
    
    int SamplesPerSecond;
    uint32_t RunningSampleIndex;
    int BytesPerSample;
    DWORD SecondaryBufferSize;
    float tSine;
    int LatencySampleCount;
};

#define WIN32_GAME_H
#endif