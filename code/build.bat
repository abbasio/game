@echo off

set CompilerFlags=-MT -nologo -Gm- -GR- -EHa- -Oi -W4 -WX -wd4201 -wd4100 -wd4101 -wd4189 -wd4477 -FC -Z7 -DGAME_WIN32=1 -DGAME_DEBUG=1 -DGAME_INTERNAL=1
set LinkerFlags=-incremental:no -opt:ref user32.lib gdi32.lib winmm.lib

IF NOT EXIST ..\build mkdir ..\build

pushd ..\build
del *.pdb > NUL 2 > NUL
cl %CompilerFlags% ..\code\game.cpp -Fmgame.map -LD /link -incremental:no -PDB:handmade_%date:~-4,4%%date:~-10,2%%date:~-7,2%_%time:~0,2%%time:~3,2%%time:~6,2%.pdb /EXPORT:GameGetSoundSamples /EXPORT:GameUpdateAndRender
cl %CompilerFlags% ..\code\win32_game.cpp -Fmwin32_game.map /link %LinkerFlags%
popd