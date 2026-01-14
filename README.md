# BetaDecayViz

SFML 3 visualization designed as a learning tool.

## Modes
- 1: Spin only (deliberately oversimplified)
- 2: Spin + motion (helicity)
- 3: Full conservation (shows orbital placeholder when spins alone do not balance)

## Controls
- 1/2/3: switch modes
- Space: new decay
- Up/Down: adjust left-handed bias
- P: pause
- N: single step (when paused)
- H: toggle help panel
- Hover dots and arrows for tooltips

## Build (Windows, Visual Studio, vcpkg)
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 ^
  -DCMAKE_TOOLCHAIN_FILE="%USERPROFILE%\vcpkg\scripts\buildsystems\vcpkg.cmake"
cmake --build build --config Release

Run:
build\Release\BetaDecayViz.exe
