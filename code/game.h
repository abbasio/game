#if !defined(GAME_H)


#define ArrayCount(Array) (sizeof(Array) / sizeof((Array)[0]))

//--------Services that the platform layer provides to the game.

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
    bool IsAnalog;

    float StartX;
    float StartY;

    float MinX;
    float MinY;

    float MaxX;
    float MaxY;

    float EndX;
    float EndY; 
    
    union 
    {
        game_button_state Buttons[6];
        struct
        {
            game_button_state Up;
            game_button_state Down;
            game_button_state Left;
            game_button_state Right;
            game_button_state LeftShoulder;
            game_button_state RightShoulder;
        };
    };
};

struct game_input
{
    game_controller_input Controllers[4];
};

static void GameOutputSound(game_sound_output_buffer *SoundBuffer, int SampleCountToOutput);
static void GameUpdateAndRender(game_input *Input, game_offscreen_buffer *Buffer, game_sound_output_buffer *SoundBuffer);


#define GAME_H
#endif