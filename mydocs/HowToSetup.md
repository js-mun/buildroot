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

config 설정
```
make menuconfig

Target packages > Libraries > Graphics
[*] libdrm

Target packages > Libraries > Graphics > libdrm
[*]   Install test programs  

Target packages > Debugging, profiling and benchmark
[*] strace
```

빌드
```
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
  -serial stdio
```

- -vga none -device virtio-gpu-pci: 구형 VGA 에뮬레이션을 끄고 현대적인 가상 GPU 드라이버 VirtIO-GPU 사용 (리눅스 커널의 virtio_gpu 드라이버와 매핑)
- -display sdl,gl=on: 가상머신의 출력을 우분투 호스트 SDL2로 띄움, OpenGL 가속을 활성화
- -serial stdio: 가상머신의 터미널 콘솔 입출력을 현재 호스트 터미널에 보여줌


## 테스트
```
modetest
```

아래 명령어 입력 시, 화면 출력 
```
# modetest -M virtio_gpu -s <Connector_ID>@<CRTC_ID>:<해상도_이름>
modetest -M virtio_gpu -s 38@37:1024x768
```