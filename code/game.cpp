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
static void GameUpdateAndRender(game_memory *Memory, game_input *Input, game_offscreen_buffer *Buffer, game_sound_output_buffer *SoundBuffer)
{
    game_state *GameState = (game_state *)Memory->PermanentStorage;

    Assert(sizeof(game_state) <= Memory->PermanentStorageSize);

    if(!Memory->IsInitialized)
    {
        GameState->ToneHz = 256;
        // might move to platform layer
        Memory->IsInitialized = true;
    }
    else
    {

    }

    game_controller_input *Input0 = &Input->Controllers[0];
    if(Input0->IsAnalog)
    {
        //  Use analog movement tuning
        GameState->BlueOffset += (int)4.0f*(Input0->EndX);
        GameState->ToneHz = 256 + (int)(128.0f*(Input0->EndY));
    }
    else
    {
        // Use digital movement tuning
    }

    if(Input0->Down.EndedDown)
    {
        GameState->GreenOffset += 1;
    }


    GameOutputSound(SoundBuffer, GameState->ToneHz);
    RenderWeirdGradient(Buffer, GameState->BlueOffset, GameState->GreenOffset);
}