# 4a-hal-fd-dsp

Start by building cloning, and building 4a-alsa-core.

```
git clone --recurse-submodules https://git.automotivelinux.org/src/4a-alsa-core
cd 4a-alsa-core
mkdir -p build && cd build
cmake ..
make
```

Move back to your workspace and clone the 4a-hal-fd-dsp repo and build.

You will first require to modify the ```#define ALSA_CARD_NAME``` and ```#define ALSA_DEVICE_ID``` in the ```hal_fd_dsp.c``` 

```
git clone --recurse-submodules https://github.com/fiberdyne/agl-4a-hal.git
cd agl-4a-hal
mkdir -p build && cd build
cmake ..
make
```
Finally, copy the lib from 4a-alsa-core/build/package/lib to 4a-hal-fd-dsp/build/package/lib before executing afb-daemon.

```
cd 4a-hal-fd-dsp/build/package
afb-daemon --verbose --verbose --port=1234 --rootdir=./ --roothttp=./htdocs --ldpath=./lib
```
