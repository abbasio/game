@echo off

IF NOT EXIST ..\build mkdir ..\build
pushd ..\build
cl -DGAME_WIN32=1 -FC -Zi ..\code\win32_game.cpp user32.lib gdi32.lib
popd