# Quake

Adapted from https://github.com/mwh204/id-quake.x11.  My personal notes are in notes.md.

I've split this into several folders:
* src - the original, Linux-only source with minor modifications
* newsrc - ported to SDL and actively under development but mostly maintaining Quake compatibility

## Installation

### newsrc
1. Install SDL2:
	* macOS - `brew install sdl2`
	* Linux - `sudo apt-get install libsdl2-dev`
1. `cd newsrc`
1. `make`

### src
1. `sudo dpkg --add-architecture i386`
1. Check that it worked: `sudo dpkg --print-foreign-architectures`
1. `sudo apt-get update`
1. `sudo apt-get install linux-libc-dev:i386 g++-multilib libx11-dev:i386 libxext-dev:i386`
1. `make`

## Execution
1. Ensure the id1 folder and/or any mod folders you want are in the root dir of this repo
1. `cd newsrc`
1. Modify the `run` rule in Makefile as desired
1. `make run`

## TODO
* port to SDL
	* input - WIP
	* sound
	* graphics - DONE
* remove unnecessary code

## Long term goals (in a new form)
* Replace QuakeC with Lua
* Transition codebase to C++ (use namespaces, classes as simple structs with methods)
