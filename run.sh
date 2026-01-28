set -e
# zig cc -target aarch64-linux-gnu -o loader qemu.c -ldl
cmake --build build
cp build/project_qemu /opt/arm64-rootfs/tmp/
sudo chroot /opt/arm64-rootfs /tmp/project_qemu
