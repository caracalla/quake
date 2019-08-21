# Quake

My notes are in notes.md

Before installing the packages listed below, the following must be done to be able to install 32 bit (i386) packages:
1. `sudo dpkg --add-architecture i386`
1. Check that it worked: `sudo dpkg --print-foreign-architectures`
1. `sudo apt-get update`

## Original README from https://github.com/mwh204/id-quake.x11

this is a subset of id's release of quake with only the files required for the x11 build included.

some libraries I had to install to compile it on debian amd64:
linux-libc-dev:i386 g++-multilib libx11-dev:i386 libxext-dev:i386

## TODO
* get an x machine running and get it working with quake data files
* remove the i386 dependency
* remove the x11 dependency -- maybe render in framebuffer??
