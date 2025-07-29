#include "game.h"

void GameOutputSound(game_state *GameState, game_sound_output_buffer *SoundBuffer)
{
    int16_t ToneVolume = 3000;
    int WavePeriod = SoundBuffer -> SamplesPerSecond/GameState->ToneHz;

    int16_t *SampleOut = SoundBuffer->Samples;
    for(int SampleIndex = 0; SampleIndex < SoundBuffer->SampleCount; ++SampleIndex)
    {
        float SineValue = sinf(GameState->tSine);
        int16_t SampleValue = (int16_t)(SineValue * ToneVolume);
        *SampleOut++ = SampleValue;
        *SampleOut++ = SampleValue;

        GameState->tSine += 2.0f*Pi32*1.0f / (float)WavePeriod;
        if(GameState->tSine > (2.0f*Pi32))
        {
            GameState->tSine -= (2.0f*Pi32);
        }
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
            uint8_t Blue = (uint8_t)(X + BlueOffset);
            uint8_t Green = (uint8_t)(Y + GreenOffset);

            // write a value to the location of the pixel pointer,
            // then increment
            *Pixel++ = ((Green << 8) | Blue);
        }

        Row += Buffer->Pitch;
    }
}

extern "C" GAME_UPDATE_AND_RENDER(GameUpdateAndRender)
{
    game_state *GameState = (game_state *)Memory->PermanentStorage;

    Assert(sizeof(game_state) <= Memory->PermanentStorageSize);

    if(!Memory->IsInitialized)
    {
        char *FileName = __FILE__;
        
        debug_read_file_result File = Memory->DEBUGPlatformReadEntireFile(FileName);
        if(File.Contents)
        {
            Memory->DEBUGPlatformWriteEntireFile("test.out", File.ContentsSize, File.Contents);
            Memory->DEBUGPlatformFreeFileMemory(File.Contents);
        }
        
        GameState->ToneHz = 256;
        GameState->tSine = 0.0f;
        // might move to platform layer
        Memory->IsInitialized = true;
    }
    else
    {

    }

    for(int ControllerIndex = 0; ControllerIndex < ArrayCount(Input->Controllers); ++ControllerIndex)
    {
        game_controller_input *Controller = GetController(Input, ControllerIndex);
        if(Controller->IsAnalog)
        {
            //  Use analog movement tuning
            GameState->BlueOffset += (int)(4.0f*(Controller->StickAverageX));
            GameState->ToneHz = 256 + (int)(128.0f*(Controller->StickAverageY));
        }
        else 
        {
            // Use digital movement tuning
            if (Controller->MoveLeft.EndedDown)
            {
                GameState->BlueOffset -= 1;
            }
            if (Controller->MoveRight.EndedDown)
            {
                GameState->BlueOffset += 1;
            }
        }
        if(Controller->ActionDown.EndedDown)
        {
            GameState->GreenOffset += 1;
        }
    }

    RenderWeirdGradient(Buffer, GameState->BlueOffset, GameState->GreenOffset);
}

extern "C" GAME_GET_SOUND_SAMPLES(GameGetSoundSamples)
{
    game_state *GameState = (game_state *)Memory->PermanentStorage;
    GameOutputSound(GameState, SoundBuffer);
}

#if GAME_WIN32

#include "windows.h"

BOOL WINAPI DllMain(
    _In_ HINSTANCE hinstDLL,
    _In_ DWORD fdwReason,
    _In_ LPVOID lpvReserved)
    {
        return TRUE;
    }

    #endif