<p align="center">
  <img src="betadecay.PNG" alt="Beta decay misconceptions visualization" width="800">
</p>
 
# BetaDecayViz

This is an SFML 3 based visualization written in C++ and intended as a learning tool.  
It focuses on how angular momentum conservation works in beta decay under different simplifying assumptions.

## Modes
- 1: Spin only (intentionally incomplete and oversimplified)
- 2: Spin plus particle motion (introduces helicity)
- 3: Full conservation view (adds an orbital angular momentum placeholder when spin alone is not sufficient)

## Controls
- 1/2/3: switch between modes
- Space: generate a new decay
- Up/Down: adjust the left-handed bias
- P: pause the simulation
- N: advance one step while paused
- H: toggle the help panel
- Hover dots and arrows to view tooltips

## Build (Windows, Visual Studio, vcpkg)
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 ^
  -DCMAKE_TOOLCHAIN_FILE="%USERPROFILE%\vcpkg\scripts\buildsystems\vcpkg.cmake"
cmake --build build --config Release

Run:
build\Release\BetaDecayViz.exe

---

## What problem this project solves

Many introductory explanations of beta decay describe the emitted electron and neutrino as having opposite spins and moving in opposite directions. When presented this way, it can appear that angular momentum conservation is handled entirely by spin, without any further consideration.

This project is meant to show where that explanation stops working. In a large number of cases, the spins of the outgoing particles do not add up to the original angular momentum of the system. Something else has to account for the difference, and that part is often left implicit or skipped entirely in simplified discussions.

The visualization makes that gap visible instead of assuming it away.

## Why the visualization matters

For students, angular momentum conservation in weak interactions is usually introduced through equations and selection rules. While those are correct, they do not always make it clear why certain simplified mental pictures fail.

By letting the user switch between progressively less incomplete models, the visualization shows:
- what happens when only spin is considered
- how particle direction and helicity affect the outcome
- when an additional orbital contribution is required to satisfy conservation

The goal is not to calculate physical quantities precisely, but to make it clearer what assumptions are being made at each step and what breaks when those assumptions are too restrictive.

## How the code is structured

The program is built around a single interactive loop that updates a shared simulation state and draws the corresponding representation using SFML.

Simulation logic, rendering, and input handling are kept close together so it is easy to follow how a given assumption or parameter change affects what is shown on screen. The code is intentionally straightforward rather than highly abstracted, since it is meant to be read and modified by someone trying to understand the behavior, not reused as a general physics framework.

## What I learned building this

Working on this made it clear how often explanations rely on leaving parts implicit, especially when the math works out but the intuition does not.

It also highlighted how useful it can be to explicitly show an incomplete or incorrect model before moving to a more complete one. From a programming perspective, the project reinforced the importance of keeping simulation state understandable when multiple constraints interact, as well as practical experience with SFML rendering, real-time input handling, and stepping a simulation in a controlled way.
