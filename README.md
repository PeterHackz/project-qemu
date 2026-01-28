# project-qemu

Custom ELF loader to mess with `libg.so` (arm64). Loads dependencies, handles relocations, and dumps the public key.

## Background

In 2025, I needed to find the server public key of an android game.

My love and aim for automation (and elf exploration and exploitation) pushed me to explore library loading on linux, so
I made what any self-respecting engineer would do: a custom ELF loader to throw the android library in memory, find the
key deobfuscator (load64) and run it :P

I am open sourcing this because I am no longer using it or interested in the game, and maybe someone will find it useful
or interesting.

> This project is for educational and research purposes only.  
> No game assets or proprietary code are included.

## How it works

For those familiar with the game, the key is loaded in multiple places, including the well-known `blake2b_init_param`.

That function contains a unique code sequence that XORs the deobfuscated key with constants.

This design choice is
questionable, but hey, not my code, and helped me automate finding it ;)

The process is as follows:

1. Map `libg.so` into memory.
2. Resolve dependencies (`libm`, `libstdc++`).
3. Apply ELF relocations.
4. Fake Android NDK symbols using system libc/libc++.
5. Replace `__cxa_atexit` to prevent crashes.
6. Execute library init routines.
7. Locate `load64` using the XOR constants and invoke it to dump the deobfuscated public key.

## Requirements

-   ARM64 rootfs at `/opt/arm64-rootfs`.
-   `qemu-aarch64-static` or native arm64 host.
-   CMake.

## Rootfs Setup

Here are the commands to set up the `arm64` rootfs on a Debian/Ubuntu host:

```bash
sudo apt install qemu-user-static debootstrap gcc-aarch64-linux-gnu
sudo mkdir -p /opt/arm64-rootfs
sudo debootstrap --arch=arm64 --foreign stable /opt/arm64-rootfs http://deb.debian.org/debian
sudo cp /usr/bin/qemu-aarch64-static /opt/arm64-rootfs/usr/bin/
sudo chroot /opt/arm64-rootfs /debootstrap/debootstrap --second-stage
```

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

-   Discord: `@s.b`
-   Email: `mail@peterr.dev` or `me@peterr.dev`
-   [Discord Server](https://discord.peterr.dev)

## give a 🌟 because why not :p
