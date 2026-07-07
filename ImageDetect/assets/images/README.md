# Sample Images

이 폴더의 JPG 파일은 COCO 2017 validation 이미지 서버에서 직접 다운로드한 실습용 이미지입니다.
COCO 사전학습 객체 탐지 모델은 보통 80개 COCO 클래스를 출력하므로, 모델 테스트용으로 잘 맞습니다.

| 파일 | 원본 URL | 실습 포인트 |
| --- | --- | --- |
| `coco_000000000139.jpg` | `http://images.cocodataset.org/val2017/000000000139.jpg` | 실내 장면, TV, 의자, 사람, 화병 |
| `coco_000000039769.jpg` | `http://images.cocodataset.org/val2017/000000039769.jpg` | 고양이, 소파, 리모컨 |
| `coco_000000397133.jpg` | `http://images.cocodataset.org/val2017/000000397133.jpg` | 주방, 사람, 식탁, 조리 도구 |

다시 다운로드하려면:

```powershell
cd D:\workspace\RoboticsStudy\ImageDetect
powershell -ExecutionPolicy Bypass -File .\scripts\download_sample_images.ps1
```
