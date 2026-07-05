# CH 4

## Kernel config를 설정하는 방법

`CONFIG_MODULES`는 **Buildroot 설정이 아니라 Linux 커널 설정**입니다. 그래서 `make menuconfig`에서는 안 보입니다.

Buildroot `menuconfig`는 `BR2_LINUX_KERNEL`, `BR2_PACKAGE_LIBDRM` 같은 **BR2_** 옵션만 다룹니다.  
`CONFIG_MODULES`는 커널 `.config` 항목이라 커널 쪽에서만 설정합니다.


| **명령**                  | **대상**                             | `CONFIG_MODULES` |
| ----------------------- | ---------------------------------- | ---------------- |
| `make menuconfig`       | Buildroot (패키지, rootfs, toolchain) | **없음**           |
| `make linux-menuconfig` | Linux 커널 (드라이버, 모듈 등)              | **여기 있음**        |


`board/qemu/x86_64/linux.config` 3번째 줄:

```
CONFIG_MODULES=y

CONFIG_MODULE_UNLOAD=y
```

## **Buildroot 커널로 LKM 빌드하는 방법**

- [https://github.com/PacktPublishing/Linux-Kernel-Programming_2E.git](https://github.com/PacktPublishing/Linux-Kernel-Programming_2E.git)

### **1회성 명령**

```
cd ~/work/study_linux_graphics/Linux-Kernel-Programming_2E/ch4/helloworld_lkm

make -C ~/work/study_linux_graphics/buildroot/output/build/linux-6.18.7 \
  M=$(pwd) modules \
  ARCH=x86_64 \
  CROSS_COMPILE=~/work/study_linux_graphics/buildroot/output/host/bin/x86_64-buildroot-linux-gnu-
```

### **Makefile 수정 (권장)**

```
PWD   := $(shell pwd)
KDIR  := /home/mj/work/study_linux_graphics/buildroot/output/build/linux-6.18.7
ARCH  := x86_64
CROSS_COMPILE := /home/mj/work/study_linux_graphics/buildroot/output/host/bin/x86_64-buildroot-linux-gnu-

obj-m += helloworld_lkm.o

all:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean
```

## **QEMU 게스트에서 로드**

**공유 폴더 설정 및 실행**

```
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
```

`run.sh`의 virtfs 공유 폴더를 쓰면 편합니다.

```
# 게스트 안에서
mkdir -p /mnt/share
mount -t 9p -o trans=virtio host_share /mnt/share

# 호스트에서 빌드한 .ko를 share_folder에 복사
mj@mj:~/work/study_linux_graphics/Linux-Kernel-Programming_2E/ch4/helloworld_lkm$ cp helloworld_lkm.ko ~/work/share_folder/

# 게스트 안에서
insmod /mnt/share/helloworld_lkm.ko
dmesg | tail
rmmod helloworld_lkm
```

