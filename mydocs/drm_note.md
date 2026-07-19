# DRM Device(`struct drm_device`)의 개념적인 객체 구조

`struct drm_device`는 **DRM Core가 하나의 그래픽 장치(Display/GPU)를 관리하기 위한 최상위 객체**이다.

중요한 점은 **CRTC나 Plane을 직접 포함하는 것이 아니라**, 이들을 관리하는 **컨테이너(Container)** 역할을 한다는 것이다.

개념적으로는 다음과 같은 구조를 가진다.

```text
struct drm_device
│
├── DRM Driver
│     (drm_driver)
│
├── Mode Configuration
│     (drm_mode_config)
│
├── CRTC List
│     (0개 이상)
│
├── Plane List
│     (Primary / Cursor / Overlay)
│
├── Encoder List
│
├── Connector List
│
├── Bridge List
│
├── Panel
│
├── GEM Object Manager
│
├── Framebuffer List
│
├── DRM File List
│     (현재 open()한 프로세스)
│
├── VBlank 관리
│
├── IRQ 관리
│
├── Debugfs
│
├── DMA-BUF / PRIME
│
└── Device 정보
      (PCI, Platform, VirtIO ...)
```

---

# 주요 객체 설명

## 1. DRM Driver (`struct drm_driver`)

DRM 드라이버가 구현하는 콜백 집합이다.

예를 들어

* open()
* ioctl()
* mmap()
* GEM Object 생성
* PRIME 연동

등을 구현한다.

드라이버마다 구현은 다르지만 DRM Core는 동일한 인터페이스를 사용한다.

---

## 2. Mode Configuration (`struct drm_mode_config`)

DRM/KMS에서 가장 중요한 객체 중 하나이다.

여기에

* CRTC
* Plane
* Connector
* Encoder

등의 객체가 연결된다.

즉, **디스플레이 파이프라인 전체를 관리하는 객체**이다.

---

## 3. CRTC

CRTC는 **하나의 화면 출력 파이프라인(Display Pipeline)** 을 의미한다.

예를 들어

```text
CRTC0 → eDP

CRTC1 → HDMI
```

멀티 모니터 환경에서는 여러 개의 CRTC가 존재할 수 있다.

---

## 4. Plane

Plane은 화면에 출력되는 **이미지 레이어(Layer)** 이다.

대표적으로

* Primary Plane
* Cursor Plane
* Overlay Plane

이 존재한다.

Atomic DRM에서는 Plane이 가장 중요한 객체 중 하나이다.

---

## 5. Connector

실제 외부 출력 포트를 의미한다.

예를 들어

* HDMI
* DisplayPort
* eDP
* DSI

등이 Connector이다.

---

## 6. Encoder

CRTC에서 생성된 데이터를 Connector가 사용하는 신호 형식으로 변환한다.

예를 들어

```text
RGB

↓

HDMI TMDS
```

같은 역할을 수행한다.

---

## 7. GEM (Graphics Execution Manager)

GPU 메모리(Buffer)를 관리하는 객체이다.

대표적인 대상은

* Framebuffer
* Texture
* Command Buffer

등이다.

---

## 8. Framebuffer

Plane이 실제로 출력하는 이미지 버퍼이다.

개념적으로는

```text
Plane

↓

Framebuffer

↓

Memory
```

관계를 가진다.

---

## 9. DRM File (`struct drm_file`)

사용자가

```c
open("/dev/dri/card0");
```

를 호출하면 생성되는 객체이다.

프로세스마다 하나씩 생성되며

* 파일 디스크립터
* 권한
* 이벤트 큐

등을 관리한다.

---

## 10. VBlank

Vertical Blank Interrupt를 관리한다.

대표적으로

* VSync 이벤트
* Page Flip 완료
* Atomic Commit 완료

등에 사용된다.

---

# 객체 관계

가장 중요한 객체들의 관계는 다음과 같다.

```text
                 drm_device
                      │
      ┌───────────────┴───────────────┐
      │                               │
drm_driver                    drm_mode_config
                                      │
            ┌──────────────┬──────────┴──────────┐
            │              │                     │
         CRTC List     Plane List         Connector List
            │              │                     │
            └───────┬──────┘                     │
                    │                            │
               Framebuffer                  Encoder
                    │                            │
                    └─────────────── Display Hardware
```

---

# QEMU VirtIO GPU에서의 예

현재 사용하는 `virtio_gpu` 드라이버에서는 대략 다음과 같은 객체들이 생성된다.

```text
drm_device
│
├── drm_driver (virtio_gpu_driver)
├── drm_mode_config
├── CRTC 1개
├── Primary Plane 1개
├── Cursor Plane 1개
├── Connector 1개
├── Encoder 1개
└── GEM Manager
```

`modetest`를 실행하면 이러한 DRM 객체들을 확인할 수 있다.

---

# Android DRM Driver에서도 동일한 구조

예를 들어 Qualcomm DPU에서는

```text
drm_device
│
├── CRTC0
├── CRTC1
├── Plane0
├── Plane1
├── Plane2
├── Connector (DSI-0)
├── Connector (HDMI)
└── GEM
```

MediaTek, Samsung DECON, Rockchip 등의 DRM 드라이버도 거의 동일한 개념을 따른다.

---

# 처음 공부할 때 가장 중요한 객체

처음에는 아래 여섯 개만 이해해도 DRM/KMS 구조를 이해하는 데 큰 도움이 된다.

```text
drm_device
    │
    ├── drm_driver
    ├── drm_mode_config
    ├── CRTC
    ├── Plane
    ├── Connector
    └── Framebuffer
```

이 여섯 객체를 중심으로 DRM Core가 그래픽 장치를 관리하며, GEM, PRIME, VBlank, Atomic State 등의 기능은 이 기본 구조 위에 추가되는 기능이라고 이해하면 된다.





# DRM Core 이후 무엇을 공부해야 할까?

현재까지 이해한 내용을 정리하면 다음과 같다.

* DRM Core의 역할
* `probe()` → `drm_dev_alloc()` → `drm_dev_register()`
* `struct drm_device`
* PCI / VirtIO / DRM Core의 관계
* GEM의 역할

이 정도면 **DRM Core의 초기화 과정은 이해한 상태**이다.

다음부터는 **DRM이 실제로 화면을 출력하는 과정**을 공부하는 것이 좋다.

---

# 1. KMS 객체 이해 (가장 중요)

먼저 DRM/KMS의 핵심 객체를 이해해야 한다.

```text
drm_device
        │
        ▼
drm_mode_config
        │
        ├── CRTC
        ├── Plane
        ├── Connector
        ├── Encoder
        └── Framebuffer
```

공부할 내용

* 각 객체의 역할
* 객체 간의 관계
* 왜 이런 구조로 설계되었는지

특히 **Plane**은 Android의 **HWC Layer**와 매우 비슷한 개념이므로 반드시 이해하는 것이 좋다.

---

# 2. Atomic KMS

현재 대부분의 DRM 드라이버는 Atomic KMS를 사용한다.

공부 순서는 다음과 같다.

```text
Atomic Commit

↓

Atomic State

↓

Plane State

↓

CRTC State

↓

Connector State
```

그리고 실제 호출 흐름은

```text
drm_atomic_commit()

↓

drm_atomic_helper_commit()

↓

driver atomic_commit_tail()
```

까지 따라가 보는 것이 좋다.

이 부분이 Android Display Stack과 가장 밀접하게 연결된다.

---

# 3. modetest 분석

현재 Buildroot + QEMU 환경을 이미 구축했으므로 `modetest`가 최고의 학습 도구이다.

예를 들어

```bash
modetest -M virtio_gpu
```

를 실행하면

* Connector
* Encoder
* CRTC
* Plane

정보를 확인할 수 있다.

그 후

```bash
modetest -s ...
```

를 실행하면서

커널 내부의

```text
drm_mode_object

↓

drm_atomic_commit()
```

에 브레이크포인트를 걸어 보면

객체들이 실제로 어떻게 동작하는지 이해하기 쉽다.

---

# 4. DRM IOCTL

Userspace에서 DRM Core까지의 호출 흐름을 공부한다.

```text
libdrm

↓

ioctl()

↓

DRM Core
```

예를 들어

```c
drmModeSetCrtc()
```

는

```text
DRM_IOCTL_MODE_SETCRTC
```

를 거쳐

```text
drm_mode_setcrtc()
```

까지 연결된다.

libdrm과 DRM Core의 연결 방식을 이해하는 것이 목표이다.

---

# 5. GEM

현재는 개념만 이해해도 충분하다.

나중에 다음 흐름을 공부하면 된다.

```text
drm_gem_create()

↓

mmap()

↓

DMA-BUF export
```

Android에서는 GEM과 DMA-BUF가 매우 중요한 역할을 한다.

---

# 6. DRM Driver 구현

이제 실제 드라이버를 읽기 시작한다.

VirtIO GPU 기준 추천 순서는 다음과 같다.

```text
drivers/gpu/drm/virtio/

↓

virtgpu_drv.c

↓

virtgpu_kms.c

↓

virtgpu_display.c

↓

virtgpu_plane.c

↓

virtgpu_crtc.c
```

여기에서

* `drm_crtc_funcs`
* `drm_plane_funcs`
* `drm_connector_funcs`

등을 실제로 구현하는 과정을 볼 수 있다.

---

# 7. Android Display와 연결

이제 Android Display Stack과 연결된다.

```text
SurfaceFlinger

↓

HWComposer

↓

DRM Atomic Commit

↓

Display Controller
```

VirtIO DRM Driver에서 배운 구조는

* Qualcomm
* MediaTek
* Samsung
* Rockchip

등 대부분의 DRM 드라이버에서도 거의 동일하게 사용된다.

---

# 추천 학습 순서

```text
1. KMS 객체
   (CRTC, Plane, Connector, Encoder, Framebuffer)

↓

2. Atomic KMS
   (Atomic State, Atomic Commit)

↓

3. modetest와 virtio_gpu 연결
   (객체와 실제 드라이버 매핑)

↓

4. 실제 Android DRM Driver 분석
   (MediaTek / Qualcomm / Samsung)
```

---

# 최종 목표

최종적으로는 다음 흐름이 머릿속에 자연스럽게 그려질 정도가 되면 좋다.

```text
Application

↓

libdrm

↓

DRM IOCTL

↓

DRM Core

↓

Atomic Commit

↓

CRTC / Plane / Connector

↓

Display Controller

↓

Panel (DSI / HDMI / DP)
```

이 흐름을 이해하면 VirtIO뿐 아니라 Android SoC의 DRM 드라이버(Qualcomm, MediaTek, Samsung 등)도 같은 관점에서 읽을 수 있으며, Display Driver 구조를 이해하는 데 큰 도움이 된다.


# DRM/KMS 객체 이해

## 목표

이 문서의 목표는 DRM/KMS의 핵심 객체인

* CRTC
* Plane
* Connector
* Encoder
* Framebuffer

가 **왜 존재하는지**, **서로 어떤 관계를 가지는지**를 이해하는 것이다.

---

# 1. KMS(Kernel Mode Setting)란?

KMS는 **커널이 디스플레이의 출력 상태를 관리하는 프레임워크**이다.

예를 들어

* 해상도 변경
* 주사율 변경
* 모니터 연결/분리
* 화면 출력

등을 담당한다.

Userspace(libdrm)는 KMS를 이용하여 화면을 제어한다.

---

# 2. KMS의 전체 구조

KMS의 핵심 객체는 다음과 같다.

```text
                  drm_device
                       │
                drm_mode_config
                       │
      ┌────────────────┼────────────────┐
      │                │                │
   Connector        CRTC            Plane
      │                │                │
      │                └──────┬─────────┘
      │                       │
      │                  Framebuffer
      │
   Encoder
```

각 객체는 서로 다른 역할을 가진다.

---

# 3. CRTC

## 역할

CRTC는 **화면 하나를 출력하는 파이프라인(Display Pipeline)** 이다.

쉽게 말하면

> "어떤 Framebuffer를 어떤 타이밍으로 화면에 출력할 것인가"

를 담당한다.

---

## 예

노트북

```text
CRTC0
   │
eDP
```

듀얼 모니터

```text
CRTC0 → HDMI

CRTC1 → DisplayPort
```

---

## Android에서

CRTC는

Display Controller의 출력 파이프라인에 해당한다.

예)

* Qualcomm DPU
* Samsung DECON
* MediaTek DDP

---

# 4. Plane

## 역할

Plane은

**화면에 출력될 이미지 레이어**이다.

Framebuffer 하나를 가진다.

```text
Plane

↓

Framebuffer
```

---

## 종류

### Primary Plane

메인 화면

---

### Cursor Plane

마우스 커서 전용

---

### Overlay Plane

비디오

카메라

UI

등을 위한 레이어

---

## Android에서

Plane은 거의

**HWC Layer**

와 대응된다.

예를 들어

```text
App1

↓

Layer0

↓

Primary Plane

----------------

Video

↓

Layer1

↓

Overlay Plane
```

---

# 5. Framebuffer

Framebuffer는

실제 출력될 이미지 버퍼이다.

예를 들어

```text
1920 x 1080

ARGB8888
```

메모리 공간이다.

Plane은

Framebuffer를 참조한다.

```text
Plane

↓

Framebuffer

↓

Memory
```

---

# 6. Connector

Connector는

실제 출력 포트이다.

예를 들어

* HDMI
* DP
* eDP
* DSI
* VGA

등이다.

Connector는

모니터가 연결되었는지

EDID가 무엇인지

등을 관리한다.

---

# 7. Encoder

Encoder는

CRTC의 출력 데이터를

Connector가 사용하는 신호로 변환한다.

예를 들어

```text
RGB

↓

TMDS

↓

HDMI
```

또는

```text
RGB

↓

DSI Packet
```

---

# 객체들의 연결 관계

가장 중요한 관계는 다음과 같다.

```text
Framebuffer
      ▲
      │
Plane
      │
      ▼
CRTC
      │
      ▼
Encoder
      │
      ▼
Connector
      │
      ▼
Monitor
```

즉,

Plane은 Framebuffer를 선택하고,

CRTC는 Plane들을 합성하여(Display Pipeline),

Encoder를 통해,

Connector로 출력한다.

---

# 여러 Plane이 있는 경우

예를 들어

```text
Primary Plane

↓

UI

Overlay Plane

↓

Video

Cursor Plane

↓

Mouse
```

를 생각해 보자.

CRTC는

```text
Primary

+

Overlay

+

Cursor
```

를 합성하여

최종 화면을 만든다.

---

# Atomic DRM에서는

Atomic Commit 시

각 객체의 상태(State)를 한 번에 변경한다.

```text
Plane State

↓

CRTC State

↓

Connector State

↓

Atomic Commit
```

그래서 화면이 찢어지지 않고

한 번에 변경된다.

---

# modetest와 연결

다음 명령을 실행해 보자.

```bash
modetest -M virtio_gpu
```

그러면

```text
Connectors

CRTCs

Planes

Encoders
```

목록이 출력된다.

즉,

modetest는

DRM Device 안에 등록된 KMS 객체를 그대로 보여주는 도구이다.

---

# 실제 화면 출력 과정

사용자가

```bash
modetest -s 38@37:1024x768
```

를 실행하면

개념적으로는 다음 과정이 수행된다.

```text
Framebuffer 생성
        │
        ▼
Plane에 연결
        │
        ▼
CRTC에 연결
        │
        ▼
Encoder
        │
        ▼
Connector
        │
        ▼
Monitor 출력
```

---

# Android와 비교

```text
Android

SurfaceFlinger
        │
        ▼
HWComposer Layer
        │
        ▼
Plane
        │
        ▼
CRTC
        │
        ▼
DSI
        │
        ▼
Panel
```

즉,

Android의 Layer와 DRM의 Plane은 매우 비슷한 개념이다.

---

# 핵심 관계

다음 그림 하나만 이해해도 KMS 구조를 이해했다고 볼 수 있다.

```text
                    drm_device
                          │
                    drm_mode_config
                          │
        ┌─────────────────┴─────────────────┐
        │                                   │
     Connector                        CRTC
                                            │
                    ┌───────────────────────┘
                    │
                 Plane
                    │
                    ▼
             Framebuffer
                    │
                    ▼
                 Memory
```

---

# 반드시 기억할 것

| 객체                  | 역할                            |
| ------------------- | ----------------------------- |
| **Framebuffer**     | 이미지 데이터가 저장된 메모리              |
| **Plane**           | 어떤 Framebuffer를 사용할지 결정하는 레이어 |
| **CRTC**            | 여러 Plane을 하나의 화면으로 출력하는 파이프라인 |
| **Encoder**         | 출력 신호 변환                      |
| **Connector**       | 실제 출력 포트(HDMI, DSI, DP 등)     |
| **drm_mode_config** | KMS 객체들을 관리하는 관리자             |
| **drm_device**      | DRM 전체를 대표하는 최상위 객체           |

---

# 다음 단계

KMS 객체를 이해했다면 다음으로 공부할 내용은 **Atomic KMS**이다.

추천 순서:

```text
Plane State

↓

CRTC State

↓

Connector State

↓

Atomic State

↓

drm_atomic_commit()

↓

Driver atomic_commit_tail()
```

이 흐름을 이해하면 `modetest`가 실행될 때 DRM Core와 드라이버가 실제로 어떤 과정을 거쳐 화면을 출력하는지 분석할 수 있으며, Android의 SurfaceFlinger → HWComposer → Display Driver 구조와도 자연스럽게 연결된다.
