@echo off

set CompilerFlags=-MT -nologo -Gm- -GR- -EHa- -Oi -W4 -WX -wd4201 -wd4100 -wd4101 -wd4189 -wd4477 -FC -Z7
set LinkerFlags=-opt:ref user32.lib gdi32.lib winmm.lib

IF NOT EXIST ..\build mkdir ..\build

pushd ..\build
cl %CompilerFlags% -DGAME_WIN32=1 -DGAME_DEBUG=1 -DGAME_INTERNAL=1 -Fmgame.map ..\code\game.cpp /link  /DLL /EXPORT:GameGetSoundSamples /EXPORT:GameUpdateAndRender
cl %CompilerFlags% -DGAME_WIN32=1 -DGAME_DEBUG=1 -DGAME_INTERNAL=1 -Fmwin32_game.map ..\code\win32_game.cpp  /link %LinkerFlags%
popd