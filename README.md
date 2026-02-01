# project-qemu

Custom ARM64 ELF loader to mess with `libg.so`.  
Loads dependencies, handles relocations, and observes runtime-initialized data under emulation.

## Background

In 2025, I wanted to better understand how a real-world ARM64 Android library initializes and computes data at runtime.

My love and aim for automation (and ELF exploration :p) pushed me to dive into library loading on Linux, so
I made what any self-respecting engineer would do: a custom ELF loader to throw an Android library into memory,
execute its init routines under QEMU, and observe how certain values are produced.

I am open-sourcing this because I am no longer using it, and because it might be useful or interesting for anyone
playing with emulation, ELF internals, or binary analysis.

> This project is for educational and research purposes only.  
> No game assets, proprietary binaries, or confidential code are included or distributed.

## How it works

The loader is designed to run opaque ARM64 shared objects outside their original environment and inspect their
runtime behavior.

At a high level, the process is as follows:

1. Map `libg.so` into memory.
2. Resolve dependencies (`libm`, `libstdc++`).
3. Apply ELF relocations.
4. Fake missing Android NDK symbols using system libc/libc++.
5. Replace `__cxa_atexit` to prevent crashes.
6. Execute library initialization routines.
7. Locate a specific runtime function (`load64`) using a recognizable instruction pattern and invoke it to
   observe the runtime-computed data it produces.

## Requirements

- ARM64 rootfs at `/opt/arm64-rootfs`
- `qemu-aarch64-static` or native ARM64 host
- CMake

## Rootfs Setup (~500mb of disk needed)

Here are the commands to set up the `arm64` rootfs on a Debian/Ubuntu host:

```bash
sudo apt install qemu-user-static debootstrap gcc-aarch64-linux-gnu
sudo mkdir -p /opt/arm64-rootfs
sudo debootstrap --arch=arm64 --foreign stable /opt/arm64-rootfs http://deb.debian.org/debian
sudo cp /usr/bin/qemu-aarch64-static /opt/arm64-rootfs/usr/bin/
sudo chroot /opt/arm64-rootfs /debootstrap/debootstrap --second-stage
````

## Setup

1. Copy the library to the chroot:

```bash
cp libg.so /opt/arm64-rootfs/tmp/
```

2. Configure CMake (if not already done):

```bash
cmake -DCMAKE_TOOLCHAIN_FILE=aarch64-clang-toolchain.cmake -B build
```

## Run

Use `./run.sh` to build and chroot exec:

```bash
./run.sh
```

## Contacts

* Discord: `@s.b`
* Email: `mail@peterr.dev` or `me@peterr.dev`
* Discord Server: [https://discord.peterr.dev](https://discord.peterr.dev)

## give a 🌟 because why not :p
