# Linux Kernel Boot and DRM Bring-up

이 문서는 QEMU x86 + `virtio-gpu` 환경에서,
Linux 커널이 부팅된 뒤 **언제 DRM이 살아나는지**를 초보자 관점에서 정리한 노트입니다.

핵심 질문은 두 가지입니다.

1. 커널 부팅 중 어디서 `virtio-gpu` 드라이버가 붙는가?
2. `/dev/dri/card0`는 언제 생기는가?

## 먼저 결론

- `DRM`은 부팅 후반의 사용자 공간이 아니라, **커널 드라이버 초기화 단계**에서 올라옵니다.
- `virtio-gpu`는 PCI 장치로 인식된 뒤, 커널의 `virtio_pci`와 `virtio_gpu` 드라이버가 차례로 붙으면서
  DRM 장치로 등록됩니다.
- 따라서 `/dev/dri/card0`가 생기는 시점은 **PID 1이 뜨기 전후의 커널 드라이버 초기화 구간**입니다.

## 학습에 필요한 구간만 보기

아래처럼 너무 이른 단계는 건너뛰어도 됩니다.

```text
bootloader -> start_kernel()
start_kernel() -> rest_init()
kernel_init() -> do_basic_setup()
do_initcalls()
PCI subsys init
virtio_pci_probe()
virtio_gpu_probe()
drm_dev_register()
/dev/dri/card0 생성
init(/sbin/init 또는 /bin/sh)
modetest 실행
```

## 부팅 단계별 설명

### 1. `start_kernel()`

Linux 커널의 큰 시작점입니다.
아주 이른 부팅 초기화는 여기서 시작합니다.

하지만 학습 초반에는 다음 정도만 기억하면 충분합니다.

- 커널 로그 버퍼가 준비된다
- 초기화 함수들이 등록된다
- 이후 initcall 단계에서 각 서브시스템 드라이버가 올라온다

### 2. `rest_init()` / `kernel_init()`

커널은 사용자 공간을 띄우기 전에 기본 초기화를 마칩니다.
이때 PID 1이 될 init 프로세스를 준비합니다.

### 3. `do_basic_setup()` / `do_initcalls()`

이 단계에서 실제 드라이버 초기화가 많이 일어납니다.
`virtio-gpu`도 여기서 PCI 디바이스에 붙습니다.

### 4. PCI와 virtio 드라이버 등록

PCI 코어는 QEMU가 만든 virtio-gpu 장치를 발견할 준비를 합니다.
하지만 이 문서에서 중요한 건 `pci.c` 자체의 세부 함수가 아니라,
`virtio_gpu_driver_init()`가 실행되면서 virtio 드라이버가 등록되는 지점입니다.

`drivers/gpu/drm/virtio/virtgpu_drv.c`에서:

- `module_init(virtio_gpu_driver_init)`
- `register_virtio_driver(&virtio_gpu_driver)`
- `virtio_gpu_driver.probe = virtio_gpu_probe`

이 연결이 만들어집니다.

### 5. `virtio_gpu_probe()`

여기가 그래픽 실습의 핵심입니다.
`virtio_gpu_probe()`가 호출되면서 DRM 장치가 준비됩니다.

이 함수 안에서 대략 다음이 일어납니다.

- `drm_dev_alloc()`
- `virtio_gpu_init()`
- `drm_dev_register()`
- `drm_client_setup()`

`virtio_gpu_init()` 안에서는 다시 다음 단계가 있습니다.

- `virtio_gpu_modeset_init()`
  - `drmm_mode_config_init()`
  - `vgdev_output_init()` 반복

`vgdev_output_init()` 안에서는 per-scanout 단위로:

- `drm_crtc_init_with_planes()`
- `drm_connector_init()`
- `drm_simple_encoder_init()`

가 호출됩니다.

### 6. `drm_dev_register()`

DRM 디바이스가 등록됩니다.
이 시점 이후에 `/dev/dri/card0` 같은 노드가 생깁니다.

중요한 점은 `drm_client_setup()`가 이 다음에 온다는 것입니다.
즉, DRM 장치가 먼저 공개되고, 그 위에 in-kernel client(fbdev 같은)가 붙습니다.

즉, `modetest`가 접근하는 장치가 여기서 준비됩니다.

### 7. 사용자 공간 시작

이후 `init`가 올라오고, 로그인 프롬프트나 `/bin/sh`가 실행됩니다.
그 다음에 `modetest`를 직접 실행하게 됩니다.

## 왜 이 타이밍이 중요한가

`modetest`는 DRM 장치를 사용하는 사용자 공간 프로그램입니다.
따라서:

- 커널이 먼저 `/dev/dri/card0`를 만들어야 하고
- 그 다음에 `modetest`가 열어야 합니다

순서가 반대면 당연히 실패합니다.

## 로그로 확인하기

다음 로그를 보면 흐름이 잘 보입니다.

```bash
dmesg | grep -E "virtio|drm"
ls -l /dev/dri
```

관찰 포인트:

- `virtio-gpu-pci detected`
- `Initialized virtio_gpu`
- `/dev/dri/card0`

## 코드와 연결해서 볼 파일

- [output/build/host-qemu-10.2.0/hw/display/virtio-gpu-pci.c](/home/mj/work/study_linux_graphics/buildroot/output/build/host-qemu-10.2.0/hw/display/virtio-gpu-pci.c)
- [output/build/host-qemu-10.2.0/hw/display/virtio-gpu.c](/home/mj/work/study_linux_graphics/buildroot/output/build/host-qemu-10.2.0/hw/display/virtio-gpu.c)
- [output/build/host-qemu-10.2.0/hw/virtio/virtio-pci.c](/home/mj/work/study_linux_graphics/buildroot/output/build/host-qemu-10.2.0/hw/virtio/virtio-pci.c)
- [output/build/linux-6.18.7/drivers/gpu/drm/virtio/virtgpu_drv.c](/home/mj/work/study_linux_graphics/buildroot/output/build/linux-6.18.7/drivers/gpu/drm/virtio/virtgpu_drv.c)
- [output/build/linux-6.18.7/drivers/gpu/drm/virtio/virtgpu_display.c](/home/mj/work/study_linux_graphics/buildroot/output/build/linux-6.18.7/drivers/gpu/drm/virtio/virtgpu_display.c)
- [output/build/linux-6.18.7/drivers/gpu/drm/drm_drv.c](/home/mj/work/study_linux_graphics/buildroot/output/build/linux-6.18.7/drivers/gpu/drm/drm_drv.c)
- [output/build/linux-6.18.7/drivers/gpu/drm/drm_crtc.c](/home/mj/work/study_linux_graphics/buildroot/output/build/linux-6.18.7/drivers/gpu/drm/drm_crtc.c)
- [output/build/linux-6.18.7/drivers/gpu/drm/drm_connector.c](/home/mj/work/study_linux_graphics/buildroot/output/build/linux-6.18.7/drivers/gpu/drm/drm_connector.c)
- [output/build/linux-6.18.7/drivers/gpu/drm/clients/drm_client_setup.c](/home/mj/work/study_linux_graphics/buildroot/output/build/linux-6.18.7/drivers/gpu/drm/clients/drm_client_setup.c)
- [output/build/libdrm-2.4.131/tests/util/kms.c](/home/mj/work/study_linux_graphics/buildroot/output/build/libdrm-2.4.131/tests/util/kms.c)
- [output/build/libdrm-2.4.131/tests/modetest/modetest.c](/home/mj/work/study_linux_graphics/buildroot/output/build/libdrm-2.4.131/tests/modetest/modetest.c)

## 함께 보면 좋은 UML

- [mydocs/kernel_boot_drm_sequence.puml](/home/mj/work/study_linux_graphics/buildroot/mydocs/kernel_boot_drm_sequence.puml)
- [mydocs/virtio_gpu_drm_stack.puml](/home/mj/work/study_linux_graphics/buildroot/mydocs/virtio_gpu_drm_stack.puml)
- [mydocs/modetest_sequence.puml](/home/mj/work/study_linux_graphics/buildroot/mydocs/modetest_sequence.puml)
