# Pluto Fall

Pluto Fall is a cooperative scrolling space shooter. This repository currently
contains an **initial playable prototype**, not the complete game described in
the original design.

## Build on Windows

Install Visual Studio 2026 with the Desktop development with C++ workload,
Windows SDK, and CMake tools. From a Developer PowerShell, generate the solution
using the Visual Studio generator installed on your machine, for example:

```powershell
cmake --help # List available generators; choose the one for your Visual Studio installation.
cmake -S . -B build -G "<Visual Studio generator>" -A x64
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
.\build\Debug\PlutoFall.exe
```

The generated Visual Studio solution uses MSBuild and C++23. The prototype uses
Direct3D 11 with Direct2D/DirectWrite for its placeholder graphics and text,
XInput for controllers, and initializes XAudio2. No external dependencies or
asset downloads are required. The platform-independent gameplay core and tests
can also be built on other platforms with CMake and a C++23 compiler.

## Controls

Use W/A/S/D or arrow keys to move; Enter starts or joins; Ctrl fires; Shift
launches a bomb or cancels a player selection; Escape returns to the title.
On an XInput controller use the left stick or D-pad to move, A to join or fire,
the right trigger to fire, B or the left trigger to bomb or cancel, Start to
begin, and Back to return to the title. Up to four players can join, including
one keyboard player. On the player-select screen, lock each selection with
Ctrl/A, then press Enter/Start to begin.

The current prototype has placeholder ships, enemies, and an endless portrait
playfield. Volume controls do not yet affect audio because sounds and music have
not been added. JSON-driven levels, dialogue and credits, localization, asset
loading and fallbacks, model rendering, additional motion/fire functions,
landscape levels, powerups, bosses, and level transitions are not implemented.
