# Darknet YOLO 실습 프로젝트

이 폴더는 `darknet.md` 내용을 실제로 따라 해보기 위한 작은 실습 템플릿이다. Darknet 설치 후 사전 학습 모델로 추론을 확인하고, 이후 직접 라벨링한 로봇 부품 데이터셋으로 학습을 시작할 수 있게 구성했다.

## 폴더 구조

```txt
darknet_practice/
  data/
    images/train/   학습 이미지
    images/valid/   검증 이미지
    labels/train/   학습 라벨
    labels/valid/   검증 라벨
  outputs/          추론 결과 저장 위치
  project/
    robot_parts.names
    robot_parts.data
    train.txt
    valid.txt
  scripts/
    create_dataset_lists.ps1
    run_coco_image.ps1
    setup_darknet_wsl.sh
    train_robot_parts_wsl.sh
  weights/          다운로드한 weights 저장 위치
```

## 1. Darknet 설치

WSL2 Ubuntu 기준으로 빠르게 설치하려면 다음 스크립트를 실행한다.

```bash
cd /mnt/d/workspace/RoboticsStudy/ETC/tech/darknet_practice
bash scripts/setup_darknet_wsl.sh
```

설치가 끝나면 버전을 확인한다.

```bash
darknet version
```

## 2. 사전 학습 모델로 이미지 추론

Windows PowerShell에서 샘플 weights를 내려받고 이미지 추론 명령 예시를 출력한다.

```powershell
cd D:\workspace\RoboticsStudy\ETC\tech\darknet_practice
.\scripts\run_coco_image.ps1 -ImagePath D:\workspace\RoboticsStudy\CV.jpeg
```

이 스크립트는 `weights/yolov4-tiny.weights`가 없으면 다운로드를 시도한다. Darknet이 PATH에 설치되어 있으면 곧바로 추론 명령을 실행한다.

## 3. 직접 학습할 데이터 넣기

이미지는 다음 위치에 넣는다.

```txt
data/images/train/
data/images/valid/
```

각 이미지와 같은 이름의 YOLO 라벨 파일을 다음 위치에 넣는다.

```txt
data/labels/train/
data/labels/valid/
```

예를 들어 `data/images/train/part001.jpg`가 있다면 라벨은 `data/labels/train/part001.txt`로 둔다.

YOLO 라벨 형식은 다음과 같다.

```txt
class_id x_center y_center width height
```

좌표는 모두 0.0부터 1.0 사이의 정규화 값이다.

예시:

```txt
0 0.512 0.438 0.210 0.155
1 0.280 0.620 0.120 0.090
```

## 4. 클래스 수정

기본 클래스는 로봇 부품 실습용으로 잡아두었다.

```txt
bolt
nut
washer
```

다른 객체를 학습하려면 `project/robot_parts.names`를 수정하고, `project/robot_parts.data`의 `classes` 값을 클래스 수에 맞춘다. cfg 파일의 마지막 YOLO layer 주변 `classes`, `filters` 값도 맞춰야 한다.

`filters` 계산식:

```txt
filters = (classes + 5) * 3
```

예를 들어 클래스가 3개면 `filters = 24`다.

## 5. train.txt / valid.txt 생성

PowerShell:

```powershell
cd D:\workspace\RoboticsStudy\ETC\tech\darknet_practice
.\scripts\create_dataset_lists.ps1
```

WSL:

```bash
cd /mnt/d/workspace/RoboticsStudy/ETC/tech/darknet_practice
powershell.exe -ExecutionPolicy Bypass -File scripts/create_dataset_lists.ps1
```

## 6. 학습 시작

Darknet 저장소의 `cfg/yolov4-tiny.cfg`를 복사해서 `project/robot_parts.cfg`로 만들고, 다음 항목을 수정한다.

- `batch=64`
- `subdivisions=16` 또는 GPU 메모리가 부족하면 더 큰 값
- `width`, `height`: 32로 나누어 떨어지는 값
- 각 `[yolo]` 블록의 `classes=3`
- 각 `[yolo]` 바로 앞 `[convolutional]` 블록의 `filters=24`
- `max_batches`: 클래스 수가 3이면 최소 6000 권장
- `steps`: `max_batches`의 80%, 90% 지점

그 다음 WSL에서 실행한다.

```bash
cd /mnt/d/workspace/RoboticsStudy/ETC/tech/darknet_practice
bash scripts/train_robot_parts_wsl.sh
```

## 7. 학습 결과 평가

학습 중 생성된 best weights로 mAP를 확인한다.

```bash
darknet detector map project/robot_parts.data project/robot_parts.cfg weights/robot_parts_best.weights
```

## 참고

실제 학습 품질은 모델보다 데이터 품질에 더 크게 좌우된다. 처음에는 클래스당 100장 이상, 가능한 다양한 조명/거리/각도/배경을 포함해 라벨링하는 것을 목표로 삼는 것이 좋다.
