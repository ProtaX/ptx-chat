# ptx-chat
Is a TCP chat using blocking sockets. For GUI I am using...

# NanoGUI
https://github.com/wjakob/nanogui
Because its small, free and easy to use. For logging i am using...

# spdlog
https://github.com/gabime/spdlog
Because its fast and also easy to use. In order to...

# Build a ptx-chat
* Clone:
```
git clone --recursive https://github.com/ProtaX/ptx-chat.git
```
* Install spdlog
* Install dependencies
```
sudo apt-get install cmake xorg-dev libglu1-mesa-dev
```
* Run
```
mkdir build && cd build && cmake .. && make -j4
```
* Server executable is located in build/src/server
* Client executable is located in build/src/client

# Work in progress
And bugs are fixing
