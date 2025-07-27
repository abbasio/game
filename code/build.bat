@echo off

IF NOT EXIST ..\build mkdir ..\build
pushd ..\build
cl -MT -nologo -Gm- -GR- -EHa- -Oi -W4 -WX -wd4201 -wd4100 -wd4101 -wd4189 -wd4477 -DGAME_WIN32=1 -DGAME_DEBUG=1 -DGAME_INTERNAL=1 -FC -Z7 -Fmwin32_game.map ..\code\win32_game.cpp  /link -opt:ref user32.lib gdi32.lib winmm.lib
popd