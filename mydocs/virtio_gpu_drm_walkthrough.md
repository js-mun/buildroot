# QEMU virtio-gpu DRM Walkthrough

이 문서는 `qemu-system-x86_64`에서 `virtio-gpu`를 붙였을 때,
Linux 게스트의 DRM/KMS 스택이 어떤 순서로 연결되는지 초보자 관점에서 정리한 노트입니다.

목표는 아래 흐름을 코드와 함께 읽는 것입니다.

```text
QEMU
  -> VirtIO GPU Device (PCI)
  -> Linux PCI Core
  -> virtio_pci
  -> virtio core
  -> virtio_gpu
  -> DRM Core
  -> libdrm
  -> modetest
```

## 큰 그림

`virtio-gpu`는 단순히 "화면 장치"가 아니라,
Linux 쪽에서는 **DRM 드라이버**로 동작합니다.

- QEMU는 PCI 장치를 흉내 냅니다.
- Linux PCI core가 이 장치를 발견합니다.
- `virtio_pci`가 PCI와 virtio 사이를 연결합니다.
- `virtio_gpu`가 DRM 드라이버로 등록됩니다.
- `modetest`는 libdrm을 통해 DRM ioctl을 직접 호출합니다.

즉, 화면에 그리는 것처럼 보여도 실제로는
`modetest -> libdrm -> DRM core -> virtio_gpu -> QEMU` 경로로
커널에 요청을 보냅니다.

## 각 계층의 역할

### 1. QEMU

QEMU에서 아래 옵션으로 장치를 붙입니다.

```bash
-device virtio-gpu-pci
```

이 장치는 게스트에게 PCI 장치로 보입니다.

### 2. Linux PCI Core

커널이 PCI 버스를 스캔하면서 `virtio-gpu-pci`를 발견합니다.
이 단계는 일반적인 PCI 장치 열거 과정입니다.

### 3. `virtio_pci`

`virtio_pci`는 PCI 장치를 virtio 프레임워크에 연결하는 드라이버입니다.
PCI 장치가 virtio 장치로 보이게 만드는 중간 계층입니다.

### 4. `virtio core`

virtio 장치 공통 계층입니다.
여기서 device/feature 협상이 일어나고, 상위 드라이버가 사용할 수 있는
virtio 기능이 정리됩니다.

### 5. `virtio_gpu`

이 드라이버가 핵심입니다.
`virtio_gpu`는 Linux DRM 드라이버로 등록되며,
커널 쪽에서 framebuffer, CRTC, connector, plane 같은 DRM 객체를 다룹니다.

이게 중요한 이유는, 사용자 공간에서 보는 `modetest`가
결국 이 DRM 객체들을 설정하기 때문입니다.

### 6. DRM Core

DRM core는 Linux 그래픽 스택의 중심입니다.
여기에는 여러 하위 기능이 있습니다.

- GEM / TTM
  - 그래픽 버퍼 메모리 관리
- KMS
  - connector / crtc / plane / framebuffer를 연결하는 모드 설정
- fbdev emulation
  - 오래된 framebuffer 인터페이스 호환
- PRIME / DMA-BUF
  - 다른 드라이버와 버퍼 공유
- DRM IOCTL
  - 사용자 공간과의 시스템 호출 인터페이스

`modetest`가 실제로 만지는 것은 대부분 이 IOCTL 계층과 KMS입니다.

### 7. `libdrm`

`libdrm`은 DRM ioctl을 직접 다루기 위한 사용자 공간 라이브러리입니다.

`modetest`는 X11이나 Wayland 없이도 이 라이브러리를 통해
커널 DRM 인터페이스를 직접 호출합니다.

### 8. `modetest`

`modetest`는 libdrm 테스트 프로그램입니다.
이 프로그램은 다음을 확인하는 데 유용합니다.

- connector 목록 확인
- CRTC 목록 확인
- plane 목록 확인
- mode setting
- framebuffer 생성
- atomic / legacy modeset

실습에서 가장 많이 쓰는 명령은 아래입니다.

```bash
modetest -M virtio_gpu -s 38@37:1024x768
```

## 코드에서 보는 흐름

### `modetest.c`

주요 진입점은 `main()`입니다.

읽기 좋은 순서는 다음과 같습니다.

1. `parse_connector()`
   - `-s 38@37:1024x768` 같은 문자열을 해석
   - connector 문자열, crtc id, mode string, format string을 분리
2. `util_open()`
   - `/dev/dri/card0`를 열고 driver 정보를 확인
3. `get_resources()`
   - DRM resources, connectors, crtcs, planes를 조회
4. `set_mode()`
   - 지정한 connector와 crtc를 묶고
   - mode를 찾고
   - framebuffer를 만들고
   - `drmModeSetCrtc()` 또는 atomic commit 수행

관련 파일:

- [output/build/libdrm-2.4.131/tests/modetest/modetest.c](/home/mj/work/study_linux_graphics/buildroot/output/build/libdrm-2.4.131/tests/modetest/modetest.c)
- [output/build/libdrm-2.4.131/tests/util/kms.c](/home/mj/work/study_linux_graphics/buildroot/output/build/libdrm-2.4.131/tests/util/kms.c)
- [output/build/libdrm-2.4.131/xf86drm.c](/home/mj/work/study_linux_graphics/buildroot/output/build/libdrm-2.4.131/xf86drm.c)

## 실습 팁

### 1. 커널 로그를 보고 싶을 때

`run.sh -d`로 아래 파라미터를 붙입니다.

```text
drm.debug=0x1ff log_buf_len=4M
```

### 2. libdrm 디버그를 보고 싶을 때

게스트 안에서:

```bash
export LIBGL_DEBUG=verbose
```

이 값은 `drmMsg()`가 조건부로 출력하는 로그에 영향을 줍니다.

### 3. `modetest` 실행 전후를 추적하고 싶을 때

`strace`를 이용합니다.

```bash
strace -f -tt -T -o /mnt/share/modetest.strace \
  modetest -M virtio_gpu -s 38@37:1024x768
```

그리고 `mydocs/strace_to_perfetto.py`로 트레이스를 변환할 수 있습니다.

## 함께 보면 좋은 다이어그램

- [mydocs/virtio_gpu_drm_stack.puml](/home/mj/work/study_linux_graphics/buildroot/mydocs/virtio_gpu_drm_stack.puml)
- [mydocs/modetest_sequence.puml](/home/mj/work/study_linux_graphics/buildroot/mydocs/modetest_sequence.puml)

