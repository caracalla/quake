# Quake

Adapted from https://github.com/mwh204/id-quake.x11.  My personal notes are in notes.md.

## Installation

### newsrc

This is my SDL

### src folder

This is the original source from https://github.com/mwh204/id-quake.x11.  

1. `sudo dpkg --add-architecture i386`
1. Check that it worked: `sudo dpkg --print-foreign-architectures`
1. `sudo apt-get update`
1. `sudo apt-get install linux-libc-dev:i386 g++-multilib libx11-dev:i386 libxext-dev:i386`
1. `make run`

## Execution
```sh
cd src
make run
```

## TODO
* port to SDL
	* input
	* sound
	* graphics - DONE(ish)
