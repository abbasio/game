#if !defined(GAME_H)

/*
    Build Flags:
    GAME_INTERNAL: 0 for public release, 1 for developer build
    GAME_DEBUG: 0 for performance build, 1 for debug (slow) 
*/

#if GAME_DEBUG
#define Assert(Expression) if(!(Expression)) {*(int *)0 = 0;} // Write to null pointer - forces error, stops program
#else
#define Assert(Expression)
#endif

//--------MACROS
#define Kilobytes(Value) ((Value) * 1024)
#define Megabytes(Value) (Kilobytes(Value) * 1024)
#define Gigabytes(Value) (Megabytes(Value) * 1024)
#define Terabytes(Value) (Gigabytes(Value) * 1024)

#define ArrayCount(Array) (sizeof(Array) / sizeof((Array)[0]))

inline uint32_t SafeTruncateUInt64(uint64_t Value)
{
    // TODO: Define something for maximum values (UInt32Max)
    Assert(Value <= 0xFFFFFFFF); // Less than 32 bits
    return((uint32_t)Value);
}


//--------Services that the platform layer provides to the game.
#if GAME_INTERNAL
struct debug_read_file_result
{
    uint32_t ContentsSize;
    void *Contents;
};
static debug_read_file_result DEBUGPlatformReadEntireFile(char *FileName);
static void DEBUGPlatformFreeFileMemory(void *Memory);
static bool DEBUGPlatformWriteEntireFile(char *FileName, uint32_t MemorySize, void *Memory);
#endif
//--------Services that the game provides to the platform layer.
//Input: Timing, Controller/Keyboard input, Bitmap buffer to use, sound buffer to usestruct win_32_offscreen_buffer 
struct game_sound_output_buffer
{
    int SampleCount;
    int SamplesPerSecond;
    int16_t *Samples;
};

struct game_offscreen_buffer
{
    void *Memory;
    int Width;
    int Height;
    int Pitch;
    int BytesPerPixel;
};

struct game_button_state
{
    int HalfTransitionCount;
    bool EndedDown;
};

struct game_controller_input
{ 
    bool IsConnected;
    bool IsAnalog;

    float StickAverageX;
    float StickAverageY;

    union 
    {
        game_button_state Buttons[12];
        struct
        {
            game_button_state MoveUp;
            game_button_state MoveDown;
            game_button_state MoveLeft;
            game_button_state MoveRight;
            game_button_state ActionUp;
            game_button_state ActionDown;
            game_button_state ActionLeft;
            game_button_state ActionRight;
            
            game_button_state LeftShoulder;
            game_button_state RightShoulder; 
            
            game_button_state Start;
            game_button_state Back;
        };
    };
};

struct game_input
{
    // Todo: insert clock values here
    game_controller_input Controllers[5];
};
inline game_controller_input *GetController(game_input *Input, int ControllerIndex)
{
    Assert(ControllerIndex < ArrayCount(Input->Controllers));
    game_controller_input *Result = &Input->Controllers[ControllerIndex];
    return Result;
};

struct game_state
{
    int ToneHz;
    int GreenOffset;
    int BlueOffset;
};

struct game_memory
{
    bool IsInitialized;
    uint64_t PermanentStorageSize;
    void *PermanentStorage; // MUST be cleared to 0 at startup. Handled by default on win32
    uint64_t TransientStorageSize;
    void *TransientStorage; // Same requirement as PermanentStorage 
};


static void GameOutputSound(game_sound_output_buffer *SoundBuffer, int SampleCountToOutput);
static void GameUpdateAndRender(game_memory *Memory, game_input *Input, game_offscreen_buffer *Buffer);
// NOTE: This has to be performant or audio latency will be high
// Reduce pressure on this function by measuring
static void GameGetSoundSamples(game_memory *Memory, game_sound_output_buffer *SoundBuffer);

#define GAME_H
#endif