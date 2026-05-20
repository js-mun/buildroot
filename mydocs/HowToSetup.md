## 설치
```
sudo apt update
sudo apt install -y qemu-system-x86 qemu-utils libsdl2-dev build-essential libncurses-dev bison flex git rsync bc libssl-dev

git clone https://github.com/buildroot/buildroot.git --depth=1
cd buildroot
```

## 빌드
모델 기본 설정(defconfig)
```
make qemu_x86_64_defconfig
```

Buildroot config
- make menuconfig = Buildroot 시스템(파일 시스템, 패키지) 설정
- make linux-menuconfig = 리눅스 커널(드라이버, 하드웨어 기능) 설정
```
make menuconfig

Target packages > Libraries > Graphics
[*] libdrm

Target packages > Libraries > Graphics > libdrm
[*]   Install test programs

Target packages > Debugging, profiling and benchmark
[*] strace

System configuration
[ ] Enable root login with password
```

Linux config
```
make linux-menuconfig

# 아래 3개 검색하여 y로
NET_9P
NET_9P_VIRTIO
```

빌드
```
make -j$(nproc)
```

Linux config 변경 시 커널 빌드
```
make linux-rebuild -j$(nproc)
make -j$(nproc)
```


빌드가 완료되면 output/images/ 디렉토리에 두 개의 파일 생성
- bzImage: 가상머신용 리눅스 커널 소스
- rootfs.ext2: modetest가 포함된 루트 파일시스템 이미지

## 실행

이제 빌드된 이미지들을 QEMU로 실행. virtio-gpu를 사용하고, 호스트의 sdl과 연결
- buildroot login: root
```
qemu-system-x86_64 \
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
추가 설정
```
drm.debug 로그 출력
  -append "root=/dev/sda console=ttyS0 drm.debug=0x1ff log_buf_len=4M" \

root로 진입(init을 sh로 변경하여 login 건너뜀 -> rcS 같은 일반적인 부팅 초기화도 건너뜀)
  -append "root=/dev/sda console=ttyS0 init=/bin/sh" \
```



Host share folder 마운트
```
mkdir -p /mnt/share && mount -t 9p -o trans=virtio host_share /mnt/share
```

## strace to perfetto trace
```
python3 mydocs/strace_to_perfetto.py modetest.strace -o modetest.perfetto.json
```


## 테스트
```
modetest
```

아래 명령어 입력 시, 화면 출력
```
# modetest -M virtio_gpu -s <Connector_ID>@<CRTC_ID>:<해상도_이름>
modetest -M virtio_gpu -s 38@37:1024x768
```


strace
```
strace -f -tt -T -o /mnt/share/modetest.strace modetest -M virtio_gpu -s 38@37:1024x768
```


## libdrm module build
rebuild 안붙이면, 스탬프 확인 후 빌드 스킵. 리빌드 이후에 make해야 패키징
```
make -j8 libdrm-rebuild
make -j8
```
