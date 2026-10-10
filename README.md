# flecs-city

This is a C++ sandbox project for exploring Flecs v4 and raylib (as well as other libraries that I'll inevitably add as it grows). The long term goal is to develop a basic city simulation with vehicles. It won't necessarily be a game, but will contain game-like systems and features. I'll probably also use it to implement other things that I want to explore in the context of Flecs, such as networking.

## Requirements

* [Zig](https://ziglang.org/)
* [vcpkg](https://github.com/microsoft/vcpkg) (included as a submodule)
* Windows only: a compatible MinGW toolchain (I recommend `winget install MartinStorsjo.LLVM-MinGW.UCRT`)

## Setup

1. Bootstrap vcpkg with `./vcpkg/bootstrap-vcpkg.sh` (`./vcpkg/bootstrap-vcpkg.bat` on Windows).
2. Install dependencies with `./vcpkg/vcpkg install --triplet=x64-linux-dynamic` (`./vcpkg/vcpkg.exe install --triplet=x64-mingw-dynamic` on Windows).

## Building

Run `zig build`.

## Running

Run `zig build run`. Supported args (pass after `--`):

* -m[mode], --mode=[mode]: Mode to run in (monolith|server|client). Defaults to monolith.
* -l[listen], --listen=[listen]: Port to listen on (if mode is Server). Defaults to 6420.
* -c[connect], --connect=[connect]: Address to connect to (if mode is Client). Defaults to 127.0.0.1:6420.
