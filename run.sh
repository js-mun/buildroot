#!/usr/bin/env bash
set -euo pipefail

debug_kernel=0
debug_libgl=0
run_test=0
use_atomic=0

while getopts ":dvat" opt; do
  case "$opt" in
    d)
      debug_kernel=1
      ;;
    v)
      debug_libgl=1
      ;;
    a)
      use_atomic=1
      ;;
    t)
      run_test=1
      ;;
    *)
      echo "usage: $0 [-d] [-v] [-a] [-t]" >&2
      exit 1
      ;;
  esac
done

kernel_append="root=/dev/sda console=ttyS0 init=/bin/sh"
if [ "$debug_kernel" -eq 1 ]; then
  kernel_append="$kernel_append drm.debug=0x1ff log_buf_len=4M"
fi

qemu_cmd=(
  qemu-system-x86_64
  -M pc
  -kernel output/images/bzImage
  -drive file=output/images/rootfs.ext2,format=raw
  -append "$kernel_append"
  -vga none
  -netdev user,id=eth0
  -device virtio-net-pci,netdev=eth0
  -device virtio-gpu-pci
  -display sdl,gl=on
  -serial stdio
  -virtfs local,path=/home/mj/work/share_folder,mount_tag=host_share,security_model=none,id=host_share
)

qemu_spawn_cmd=""
for arg in "${qemu_cmd[@]}"; do
  qemu_spawn_cmd="$qemu_spawn_cmd {$arg}"
done

if [ "$run_test" -eq 0 ]; then
  exec "${qemu_cmd[@]}"
fi

modetest_cmd="modetest -M virtio_gpu -s 38@37:1024x768"
if [ "$use_atomic" -eq 1 ]; then
  modetest_cmd="$modetest_cmd -a -P 33@37:1024x768@XR24"
fi

exec expect -c "
set timeout 120

spawn -noecho $qemu_spawn_cmd

expect {
  -re {# } {
    sleep 0.5
    if {$debug_libgl == 1} {
      send -- \"export LIBGL_DEBUG=verbose\r\"
    } else {
      send -- \"unset LIBGL_DEBUG\r\"
    }
  }
  timeout {
    puts stderr \"timeout waiting for shell prompt\"
    exit 1
  }
}

expect {
  -re {# } {
    send -- \"$modetest_cmd\r\"
  }
  timeout {
    puts stderr \"timeout waiting before modetest\"
    exit 1
  }
}

puts \"modetest started in background; handing control to interactive shell\"
interact
"



# -F primary,secondary
# patterns: tiles, plain, smpte, gradient, noise, noise-color, black-white
# modetest -M virtio_gpu -s 38@37:1024x768 -F tiles,plain
# modetest -M virtio_gpu -s 38@37:1024x768 -F noise,black-white
# modetest -M virtio_gpu -s 38@37:1024x768 -F gradient,plain
# modetest -M virtio_gpu -s 38@37:1024x768 -F smpte,plain  (default)


