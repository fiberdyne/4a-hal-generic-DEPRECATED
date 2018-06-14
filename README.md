# DEPRECATED
The new code is located at [https://github.com/iotbzh/4a-hal-generic](https://github.com/iotbzh/4a-hal-generic)

# 4a-hal-generic

This project aims to provide a generic Hardware Abstraction Layer (HAL) for AGL's 4A (AGL Advanced Audio Architecture). The main feature of the generic HAL is that it becomes completely run-time dynamic in nature by employing the app-controller submodule.

## Configuration
Configuration is achieved through JSON in conf.d/etc. There are samples in this directory.

TODO: Add JSON schema, add config detailed instructions.

## Compile
Start by building cloning, and building 4a-alsa-core.

```
git clone --recurse-submodules https://git.automotivelinux.org/src/4a-alsa-core
cd 4a-alsa-core
mkdir -p build && cd build
cmake ..
make
```

Move back to your workspace and clone the 4a-hal-audio repo and build.

You will first require to modify the `#define ALSA_CARD_NAME` and `#define ALSA_DEVICE_ID` in the `hal-generic.c`

```
git clone --recurse-submodules https://github.com/fiberdyne/4a-hal-audio.git
cd 4a-hal-audio
mkdir -p build && cd build
cmake ..
make
```

Copy the 4a-alsa-core lib from `4a-alsa-core/build/package/lib` to `4a-hal-audio/build/package/lib` before executing afb-daemon.

## Running

Note: If running on a remote system, rsync the package folder to the device, connect and execute afb there. `rsync` is a nice command that makes this easy for you.

```
cd 4a-hal-audio/build
rsync -av package root@192.168.1.198:package
```

Finally, run afb-daemon.

```
cd 4a-hal-audio/build/package
afb-daemon --verbose --verbose --port=1234 --rootdir=./ --roothttp=./htdocs --ldpath=./lib --token=
```
