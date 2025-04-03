#include "game.h"

static void GameOutputSound(game_sound_output_buffer *SoundBuffer, int ToneHz)
{
    static float tSine;
    int16_t ToneVolume = 3000;
    int WavePeriod = SoundBuffer -> SamplesPerSecond/ToneHz;
    
    int16_t *SampleOut = SoundBuffer->Samples;
    for(int SampleIndex = 0; SampleIndex < SoundBuffer->SampleCount; ++SampleIndex)
    {
        float SineValue = sinf(tSine);
        int16_t SampleValue = (int16_t)(SineValue * ToneVolume);
        *SampleOut++ = SampleValue;
        *SampleOut++ = SampleValue;

        tSine += 2.0f*Pi32*1.0f / (float)WavePeriod;
    }
}

static void RenderWeirdGradient(game_offscreen_buffer *Buffer, int BlueOffset, int GreenOffset)
{
    uint8_t *Row = (uint8_t *)Buffer->Memory;
    for(int Y = 0; Y < Buffer->Height; ++Y)
    {
        uint32_t *Pixel = (uint32_t *)Row;
        for(int X = 0; X < Buffer->Width; ++X)
        {
         /*
          Pixel in memory: BB GG RR xx
          Little Endian Architecture
          0x xxRRGGBB
         */
            uint8_t Blue = (X + BlueOffset);
            uint8_t Green = (Y + GreenOffset);

            // write a value to the location of the pixel pointer,
            // then increment
            *Pixel++ = ((Green << 8) | Blue);
        }

        Row += Buffer->Pitch;
    }
}
static void GameUpdateAndRender(game_offscreen_buffer *Buffer, int BlueOffset, int GreenOffset,
                                game_sound_output_buffer *SoundBuffer, int ToneHz)
{
    GameOutputSound(SoundBuffer, ToneHz);
    RenderWeirdGradient(Buffer, BlueOffset, GreenOffset);
}