# DRM Study with Buildroot and `testapp`

이 저장소는 QEMU `virtio-gpu` 환경에서 DRM/KMS 흐름을 공부하기 위한 Buildroot 작업 공간입니다.

핵심 구성은 다음과 같습니다.

- QEMU에서 `virtio-gpu-pci` 디바이스를 올림
- Linux 커널에서 `virtio_gpu` DRM 드라이버가 probe됨
- userspace에서 `libdrm` 기반 `testapp`로 DRM 리소스를 확인하고 modeset을 실습

## 빠른 시작

빌드:

```sh
make -j$(nproc)
```

QEMU 실행:

```sh
./run.sh
```

커널 DRM 디버그 로그까지 보고 싶으면:

```sh
./run.sh -d
```

게스트 안에서 `LIBGL_DEBUG=verbose`를 켜고 `modetest`를 실행하는 흐름이 필요하면:

```sh
./run.sh -v
```

`modetest`를 부팅 직후 자동 실행하는 테스트 모드도 있습니다.

```sh
./run.sh -t
```

## `testapp`

`testapp`는 `libdrm`을 이용해 DRM 기본 개념을 단계적으로 익히는 작은 테스트 앱입니다.

설치 경로:

```sh
/usr/bin/testapp
```

사용 가능한 옵션:

- `-a`: DRM 리소스 열람
- `-b`: legacy modeset 데모

### `testapp -a`

DRM 장치에 연결된 리소스를 출력합니다.

```sh
testapp -a
```

출력 항목:

- connectors
- CRTCs
- encoders
- planes

이 옵션은 `connector`, `crtc`, `encoder`, `plane`이 실제로 어떤 ID와 관계를 가지는지 확인할 때 유용합니다.

예를 들어 학습 포인트는 다음과 같습니다.

- 어떤 connector가 connected 상태인지
- connector가 어떤 encoder와 연결되는지
- encoder가 어떤 crtc를 사용할 수 있는지
- plane이 어떤 crtc에 붙을 수 있는지

### `testapp -b`

첫 번째 connected connector를 찾아서 dumb framebuffer를 만들고 legacy modeset을 수행합니다.

```sh
testapp -b
```

실행 흐름:

- `drmModeGetResources()`
- connected connector 선택
- encoder 선택
- `DRM_IOCTL_MODE_CREATE_DUMB`
- `drmModeAddFB2()`
- `DRM_IOCTL_MODE_MAP_DUMB`
- `drmModeSetCrtc()`

이 옵션은 화면에 실제로 무언가를 띄우는 최소한의 DRM/KMS 실습입니다.

## 예제 실행

게스트에서:

```sh
testapp -a
testapp -b
```

`modetest`와 함께 비교하고 싶다면:

```sh
modetest -M virtio_gpu -s 38@37:1024x768
```

## 빌드 관련

이 저장소에는 `testapp` 패키지가 Buildroot package로 추가되어 있습니다.

### 전체 빌드

```sh
make -j$(nproc)
```

### `testapp`만 다시 빌드

```sh
make -j$(nproc) testapp-rebuild
```

`libdrm`을 수정한 뒤에는 다음처럼 다시 빌드할 수 있습니다.

```sh
make -j$(nproc) libdrm-rebuild testapp-rebuild all
```

## 학습 순서 추천

DRM 초보자 기준으로는 아래 순서가 좋습니다.

1. `testapp -a`로 리소스 구조 보기
2. `modetest -M virtio_gpu -s 38@37:1024x768`로 화면 전환 보기
3. `testapp -b`로 dumb framebuffer와 `drmModeSetCrtc()` 이해하기
4. 이후 atomic modeset, plane, encoder 분리 실습으로 확장하기

## 관련 문서

- [virtio GPU DRM walkthrough](mydocs/virtio_gpu_drm_walkthrough.md)
- [kernel boot DRM walkthrough](mydocs/kernel_boot_drm_walkthrough.md)
- [kernel boot DRM sequence](mydocs/kernel_boot_drm_sequence.puml)
- [virtio GPU DRM stack sequence](mydocs/virtio_gpu_drm_stack.puml)
