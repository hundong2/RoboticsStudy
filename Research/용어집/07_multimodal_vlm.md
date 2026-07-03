# 07. VLM과 멀티모달 용어

## VLM 기본

| 용어 | 의미 | 기억법 |
|---|---|---|
| VLM | Vision-Language Model. 이미지와 언어를 함께 처리하는 모델 | 보는 LLM입니다. |
| Multimodal | 텍스트, 이미지, 오디오, 비디오 등 여러 modality 사용 | 입력 종류가 둘 이상입니다. |
| Modality | 정보의 형태. text, image, video 등 | 감각의 종류라고 기억합니다. |
| Vision encoder | 이미지를 feature로 바꾸는 모델 | 이미지를 token처럼 읽게 해줍니다. |
| Image token | 이미지 patch나 region을 언어 모델이 처리할 수 있게 만든 token | 그림 조각을 문장 조각처럼 다룹니다. |
| Projector | vision feature를 LLM embedding 공간에 맞추는 모듈 | 서로 다른 좌표계를 연결합니다. |
| Cross-modal alignment | 이미지와 텍스트 표현을 같은 의미 공간에 맞춤 | "강아지 사진"과 "dog"를 가깝게 둡니다. |

## 이미지/문서 이해

| 용어 | 의미 | 논문 신호 |
|---|---|---|
| OCR | 이미지 속 글자를 읽는 기술 | document VLM의 기본 병목입니다. |
| Document understanding | 문서 이미지의 글자, 표, 레이아웃, 의미를 이해 | OCR + layout + reasoning입니다. |
| Layout-aware | 텍스트 위치와 구조를 함께 고려 | 표, 양식, PDF에서 중요합니다. |
| Grounding | 언어 표현을 이미지 영역이나 좌표와 연결 | "빨간 컵"이 어디 있는지 찾습니다. |
| Referring expression | 특정 객체를 가리키는 표현 | "왼쪽 위의 작은 버튼" 같은 문장입니다. |
| Region feature | 이미지의 특정 영역 표현 | detection, grounding과 연결됩니다. |
| Bounding box | 객체 위치를 나타내는 사각형 좌표 | detection 평가의 기본입니다. |

## 비디오/3D/로봇 관련

| 용어 | 의미 |
|---|---|
| Temporal reasoning | 시간 흐름에 따른 변화와 사건을 이해 |
| Video token | frame 또는 clip을 token화한 표현 |
| 3D grounding | 언어 표현을 3D 공간 위치와 연결 |
| Spatial reasoning | 위치, 거리, 방향, 포함 관계를 추론 |
| Affordance | 물체가 어떤 행동을 가능하게 하는지 |
| Embodied AI | 물리 환경에서 인식, 계획, 행동을 함께 다루는 AI |
| World model | 상태와 행동으로 다음 상태를 예측하는 모델 |

## 자주 헷갈리는 쌍

| 쌍 | 구분 |
|---|---|
| Captioning vs VQA | captioning은 이미지 설명 생성, VQA는 이미지에 대한 질문 답변입니다. |
| Detection vs Grounding | detection은 객체 범주와 위치, grounding은 언어 표현과 위치 연결입니다. |
| OCR vs Document understanding | OCR은 글자 읽기, document understanding은 구조와 의미까지 해석합니다. |
| Alignment vs Fusion | alignment는 표현 공간을 맞추는 것, fusion은 여러 modality 정보를 결합하는 것입니다. |

## 30초 기억 문장

VLM 논문은 이미지를 LLM이 읽을 수 있는 token이나 feature로 바꾸고, 텍스트와 시각 정보를 어떻게 맞추고 결합할지 다룬다. 문서 VLM은 OCR과 layout, 로봇 VLM은 grounding과 spatial reasoning을 특히 주의해서 읽는다.

