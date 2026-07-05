#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")"

exec qemu-system-x86_64 \
  -M pc \
  -kernel output/images/bzImage \
  -drive file=output/images/rootfs.ext2,format=raw \
  -append "root=/dev/sda console=ttyS0" \
  -vga none \
  -netdev user,id=eth0 \
  -device virtio-net-pci,netdev=eth0 \
  -device virtio-gpu-pci \
  -display sdl,gl=on \
  -serial stdio \
  -virtfs local,path=/home/mj/work/share_folder,mount_tag=host_share,security_model=none,id=host_share
