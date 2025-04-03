#if !defined(GAME_H)

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

static void GameOutputSound(game_sound_output_buffer *SoundBuffer, int SampleCountToOutput);
static void GameUpdateAndRender(game_offscreen_buffer *Buffer, int BlueOffset, int GreenOffset,
                                game_sound_output_buffer *SoundBuffer, int ToneHz);


#define GAME_H
#endif