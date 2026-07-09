from __future__ import annotations

import json
from pathlib import Path
from textwrap import dedent


BASE = Path(__file__).resolve().parents[1]


def src(text: str) -> list[str]:
    text = dedent(text).strip("\n") + "\n"
    return text.splitlines(keepends=True)


def md(text: str) -> dict:
    return {"cell_type": "markdown", "metadata": {}, "source": src(text)}


def code(text: str) -> dict:
    return {
        "cell_type": "code",
        "execution_count": None,
        "metadata": {},
        "outputs": [],
        "source": src(text),
    }


def notebook(cells: list[dict]) -> dict:
    return {
        "cells": cells,
        "metadata": {
            "kernelspec": {
                "display_name": "Python 3",
                "language": "python",
                "name": "python3",
            },
            "language_info": {"name": "python", "pygments_lexer": "ipython3"},
        },
        "nbformat": 4,
        "nbformat_minor": 5,
    }


COMMON_SETUP = """
from pathlib import Path
import json
import math
import random
import statistics
from collections import Counter, defaultdict

import numpy as np
import pandas as pd

try:
    import matplotlib.pyplot as plt
except Exception as exc:
    plt = None
    print("matplotlib을 불러오지 못했습니다:", exc)

ROOT = Path.cwd()
ARTIFACTS = ROOT / "artifacts"
ARTIFACTS.mkdir(exist_ok=True)

random.seed(42)
np.random.seed(42)

def save_json(name, obj):
    path = ARTIFACTS / name
    path.write_text(json.dumps(obj, ensure_ascii=False, indent=2), encoding="utf-8")
    return path

def display_df(df, n=20):
    return df.head(n)
"""


def orientation_cells() -> list[dict]:
    return [
        md(
            """
            # 00. 직무 역량을 연구 커리큘럼으로 바꾸기

            목표는 채용 공고형 요구사항을 개인 학습 로드맵, 실험 체크리스트, 논문 산출물로 바꾸는 것입니다.
            회사 이름이나 특정 조직 이름이 필요한 곳은 `xxx`로 표기합니다.

            최종 연구 주제:

            **노이즈가 있는 현장 상황에서 ASR, 환경음, OCR 증거를 결합해 문서/상황 질문응답의 신뢰도를 높이는 경량 멀티모달 파이프라인**

            이 주제는 음성 인식, 음성 대화, 환경음 이해, OCR, VLM, 대규모 데이터 전처리, 평가, MLOps, 논문 작성까지 연결됩니다.
            """
        ),
        code(COMMON_SETUP),
        code(
            """
            job_requirements = [
                ("음성 인식", "ASR 전처리, WER/CER, 노이즈 강건성, 오류 분석"),
                ("음성 대화", "의도 분류, 슬롯 추출, 대화 상태, 응답 평가"),
                ("환경음 이해", "오디오 이벤트 분류, 임베딩, 시간 구간 추론"),
                ("이미지 이해", "이미지 전처리, OCR, 레이아웃 인식, TextVQA"),
                ("대규모 데이터", "수집 설계, 품질 지표, PII/회사명 마스킹, 누수 방지"),
                ("연구 재현", "논문 읽기, 베이스라인, ablation, 신뢰구간"),
                ("MLOps", "실험 추적, 모델 카드, 데이터 카드, 배포 전 체크"),
                ("기술 리드", "코드 리뷰, 의사결정 로그, 멘토링 자료화"),
                ("논문/대회", "문헌 매트릭스, 실험표, 그림, 논문 초안"),
            ]

            roadmap = pd.DataFrame(job_requirements, columns=["요구 역량", "학습/실습 포인트"])
            roadmap["노트북"] = [
                "01_audio_speech_language/01_audio_feature_baselines.ipynb",
                "03_multimodal_dialogue/01_speech_dialogue_state_machine.ipynb",
                "01_audio_speech_language/03_environmental_sound_understanding.ipynb",
                "02_vision_ocr_language/01_ocr_robustness_lab.ipynb",
                "04_data_mlops_leadership/01_data_engineering_quality.ipynb",
                "05_research_paper_project/02_reproduction_and_ablation.ipynb",
                "04_data_mlops_leadership/02_experiment_tracking_and_model_cards.ipynb",
                "04_data_mlops_leadership/03_code_review_mentoring_playbook.ipynb",
                "05_research_paper_project/03_paper_figures_and_draft.ipynb",
            ]
            roadmap.to_csv(ARTIFACTS / "skill_to_notebook_map.csv", index=False, encoding="utf-8-sig")
            roadmap
            """
        ),
        code(
            """
            def mask_company_names(text: str, extra_terms=None) -> str:
                \"\"\"민감한 회사명/조직명 후보를 xxx로 바꾸는 간단한 전처리기입니다.

                실제 프로젝트에서는 사내 금칙어 사전, 정규표현식, NER 결과를 함께 사용합니다.
                여기서는 공개 예제이므로 사용자가 직접 extra_terms에 민감어를 넣도록 설계합니다.
                \"\"\"
                extra_terms = extra_terms or []
                masked = text
                patterns = [
                    r"회사명\\s*[:：]\\s*[^\\n,;]+",
                    r"소속\\s*[:：]\\s*[^\\n,;]+",
                    r"client\\s*[:：]\\s*[^\\n,;]+",
                    r"vendor\\s*[:：]\\s*[^\\n,;]+",
                ]
                import re
                for pat in patterns:
                    masked = re.sub(pat, lambda m: m.group(0).split(":")[0] + ": xxx", masked, flags=re.IGNORECASE)
                for term in extra_terms:
                    if term:
                        masked = masked.replace(term, "xxx")
                return masked

            sample = "회사명: ExampleCorp, 음성 OCR 모델 개발 리드 / vendor: SecretVendor"
            mask_company_names(sample, extra_terms=["ExampleCorp", "SecretVendor"])
            """
        ),
        md(
            """
            ## 완료 기준

            - `artifacts/skill_to_notebook_map.csv`가 생성된다.
            - 본인의 경력/부족한 역량을 노트북 경로와 연결한다.
            - 최종 논문 주제가 너무 넓다고 느껴지면 `음성+OCR`, `환경음+OCR`, `문서VQA` 중 하나로 축소한다.
            """
        ),
    ]


def audio_feature_cells() -> list[dict]:
    return [
        md(
            """
            # 01. 오디오 특징량과 경량 베이스라인

            음성/환경음 모델 개발의 시작점은 파형을 바로 딥러닝에 넣는 것이 아니라, 신호가 어떤 구조를 갖는지 직접 보는 것입니다.
            이 노트북에서는 합성 신호로 RMS, zero-crossing rate, 스펙트럼 중심, 로그 스펙트럼을 만들고 간단한 분류기를 학습합니다.

            논문 연결: 최종 논문의 `Baseline`과 `Feature/Preprocessing` 절에 들어갈 최소 비교군을 만듭니다.
            """
        ),
        code(COMMON_SETUP),
        code(
            """
            sr = 16_000
            duration = 1.0
            t = np.linspace(0, duration, int(sr * duration), endpoint=False)

            def make_signal(kind: str, freq=440, noise=0.02):
                if kind == "tone":
                    y = np.sin(2 * np.pi * freq * t)
                elif kind == "chirp":
                    y = np.sin(2 * np.pi * (freq + 600 * t) * t)
                elif kind == "pulse":
                    y = ((np.sin(2 * np.pi * freq * t) > 0.8).astype(float) * 2) - 1
                elif kind == "noise":
                    y = np.random.normal(0, 1, len(t))
                else:
                    raise ValueError(kind)
                return y + np.random.normal(0, noise, len(t))

            signals = []
            for label in ["tone", "chirp", "pulse", "noise"]:
                for i in range(40):
                    signals.append((label, make_signal(label, freq=random.choice([220, 440, 880]))))

            if plt:
                fig, axes = plt.subplots(2, 2, figsize=(10, 5))
                for ax, (label, y) in zip(axes.ravel(), signals[::40]):
                    ax.plot(t[:800], y[:800])
                    ax.set_title(label)
                fig.tight_layout()
            """
        ),
        code(
            """
            def audio_features(y, sr=16_000):
                eps = 1e-9
                rms = float(np.sqrt(np.mean(y ** 2)))
                zcr = float(np.mean(np.abs(np.diff(np.signbit(y)))))
                spectrum = np.abs(np.fft.rfft(y))
                freqs = np.fft.rfftfreq(len(y), 1 / sr)
                centroid = float((freqs * spectrum).sum() / (spectrum.sum() + eps))
                bandwidth = float(np.sqrt((((freqs - centroid) ** 2) * spectrum).sum() / (spectrum.sum() + eps)))
                rolloff_idx = int(np.searchsorted(np.cumsum(spectrum), 0.85 * spectrum.sum()))
                rolloff = float(freqs[min(rolloff_idx, len(freqs) - 1)])
                return [rms, zcr, centroid, bandwidth, rolloff]

            rows = []
            for label, y in signals:
                rows.append([label] + audio_features(y, sr))
            df = pd.DataFrame(rows, columns=["label", "rms", "zcr", "centroid", "bandwidth", "rolloff"])
            df.to_csv(ARTIFACTS / "audio_features.csv", index=False, encoding="utf-8-sig")
            df.groupby("label").mean(numeric_only=True)
            """
        ),
        code(
            """
            try:
                from sklearn.model_selection import train_test_split
                from sklearn.preprocessing import StandardScaler
                from sklearn.linear_model import LogisticRegression
                from sklearn.pipeline import make_pipeline
                from sklearn.metrics import classification_report, confusion_matrix

                X = df.drop(columns=["label"])
                y = df["label"]
                X_train, X_test, y_train, y_test = train_test_split(X, y, test_size=0.25, stratify=y, random_state=42)
                clf = make_pipeline(StandardScaler(), LogisticRegression(max_iter=1000))
                clf.fit(X_train, y_train)
                pred = clf.predict(X_test)
                report = classification_report(y_test, pred, output_dict=True)
                save_json("audio_baseline_report.json", report)
                print(classification_report(y_test, pred))
                print(confusion_matrix(y_test, pred, labels=sorted(y.unique())))
            except Exception as exc:
                print("scikit-learn이 없으면 requirements-minimal.txt 설치 후 다시 실행하세요:", exc)
            """
        ),
        md(
            """
            ## 확장 과제

            - 실제 음성 클립에서 위 특징량을 추출하고, 합성 신호와 분포 차이를 비교합니다.
            - 논문용 그림으로 `label별 centroid/rolloff boxplot`을 추가합니다.
            - 모델이 틀린 샘플의 파형과 스펙트럼을 함께 저장합니다.
            """
        ),
    ]


def asr_eval_cells() -> list[dict]:
    return [
        md(
            """
            # 02. ASR 평가, 오류 분석, 노이즈 강건성

            음성 인식 모델을 개발할 때 핵심은 단순 데모가 아니라 `정규화된 평가`, `오류 유형`, `노이즈 조건별 성능`입니다.
            이 노트북은 WER/CER을 직접 구현하고, 노이즈 조건별 오류표를 만듭니다.

            선택 실습으로 공개 ASR 모델을 연결할 수 있지만, 기본 실습은 모델 다운로드 없이 진행됩니다.
            """
        ),
        code(COMMON_SETUP),
        code(
            """
            def edit_distance(a, b):
                dp = [[0] * (len(b) + 1) for _ in range(len(a) + 1)]
                for i in range(len(a) + 1):
                    dp[i][0] = i
                for j in range(len(b) + 1):
                    dp[0][j] = j
                for i in range(1, len(a) + 1):
                    for j in range(1, len(b) + 1):
                        cost = 0 if a[i - 1] == b[j - 1] else 1
                        dp[i][j] = min(dp[i - 1][j] + 1, dp[i][j - 1] + 1, dp[i - 1][j - 1] + cost)
                return dp[-1][-1]

            def normalize_text(text):
                import re
                text = text.lower().strip()
                text = re.sub(r"[^0-9a-z가-힣ぁ-んァ-ン一-龥\\s]", "", text)
                text = re.sub(r"\\s+", " ", text)
                return text

            def wer(ref, hyp):
                r = normalize_text(ref).split()
                h = normalize_text(hyp).split()
                return edit_distance(r, h) / max(1, len(r))

            def cer(ref, hyp):
                r = list(normalize_text(ref).replace(" ", ""))
                h = list(normalize_text(hyp).replace(" ", ""))
                return edit_distance(r, h) / max(1, len(r))
            """
        ),
        code(
            """
            samples = [
                ("clean", "로봇이 왼쪽 선반의 빨간 상자를 집어 주세요", "로봇이 왼쪽 선반의 빨간 상자를 집어 주세요"),
                ("noise", "회의실 조명이 깜빡이고 있습니다", "회의실 조명이 깜박이고 있습니다"),
                ("noise", "請求書の合計金額を読んでください", "請求書の合計金額を呼んでください"),
                ("reverb", "start inspection after the alarm sound", "start inspection after alarm sounds"),
                ("overlap", "문서 번호 A17을 확인해 주세요", "문서 번호 a seventeen 확인해 주세요"),
                ("far_field", "환경음이 들리면 작업을 멈춰 주세요", "환경 음이 들리면 작업을 멈춰 주세요"),
            ]

            eval_rows = []
            for condition, ref, hyp in samples:
                eval_rows.append({
                    "condition": condition,
                    "reference": ref,
                    "hypothesis": hyp,
                    "wer": wer(ref, hyp),
                    "cer": cer(ref, hyp),
                })

            asr_eval = pd.DataFrame(eval_rows)
            asr_eval.to_csv(ARTIFACTS / "asr_error_eval.csv", index=False, encoding="utf-8-sig")
            asr_eval
            """
        ),
        code(
            """
            def classify_error(row):
                ref = normalize_text(row["reference"])
                hyp = normalize_text(row["hypothesis"])
                if ref == hyp:
                    return "exact"
                if row["cer"] < 0.08:
                    return "minor_spelling_or_spacing"
                if any(ch.isdigit() for ch in ref) or any(ch.isdigit() for ch in hyp):
                    return "number_or_code"
                if row["wer"] > 0.4:
                    return "severe_mismatch"
                return "semantic_or_token_error"

            asr_eval["error_type"] = asr_eval.apply(classify_error, axis=1)
            summary = asr_eval.groupby(["condition", "error_type"]).agg(
                n=("reference", "count"),
                mean_wer=("wer", "mean"),
                mean_cer=("cer", "mean"),
            ).reset_index()
            summary.to_csv(ARTIFACTS / "asr_error_summary.csv", index=False, encoding="utf-8-sig")
            summary
            """
        ),
        md(
            """
            ## 선택 실습: 실제 ASR 모델 연결

            `transformers`, `torch`, `soundfile`을 설치한 뒤 공개 ASR 모델을 연결합니다.
            모델 ID에 회사명 또는 조직명이 포함되어 보고서에 노출될 수 있으면 본문에서는 `xxx/model-name`처럼 마스킹하고,
            재현용 설정 파일에는 별도 비공개 변수로 둡니다.

            ```python
            # from transformers import pipeline
            # asr = pipeline("automatic-speech-recognition", model="모델_ID")
            # result = asr("sample.wav")
            # print(result["text"])
            ```

            논문에서는 모델명 자체보다 `모델 크기`, `학습 언어`, `추론 속도`, `WER/CER`을 중심으로 비교합니다.
            """
        ),
    ]


def environmental_sound_cells() -> list[dict]:
    return [
        md(
            """
            # 03. 환경음 이해와 이벤트 기반 상황 추론

            환경음 이해는 단순 분류보다 `언제 어떤 소리가 났는지`, `음성 명령과 충돌하는지`, `안전 관련 신호인지`가 중요합니다.
            이 노트북에서는 합성 이벤트 시퀀스를 만들고, 이벤트 분류와 시간 구간 요약을 연습합니다.
            """
        ),
        code(COMMON_SETUP),
        code(
            """
            events = ["alarm", "door", "keyboard", "speech", "silence"]
            event_to_freq = {"alarm": 1200, "door": 160, "keyboard": 3200, "speech": 550, "silence": 0}
            sr = 8_000
            win = 0.5
            t = np.linspace(0, win, int(sr * win), endpoint=False)

            def synth_event(label):
                if label == "silence":
                    y = np.random.normal(0, 0.01, len(t))
                else:
                    freq = event_to_freq[label]
                    y = np.sin(2 * np.pi * freq * t) * np.hanning(len(t))
                    y += np.random.normal(0, 0.05, len(t))
                    if label == "keyboard":
                        y *= (np.random.rand(len(t)) > 0.7)
                return y

            rows, audio_bank = [], []
            for label in events:
                for i in range(60):
                    y = synth_event(label)
                    audio_bank.append(y)
                    spectrum = np.abs(np.fft.rfft(y))
                    freqs = np.fft.rfftfreq(len(y), 1 / sr)
                    rows.append({
                        "label": label,
                        "energy": float(np.mean(y ** 2)),
                        "peak_freq": float(freqs[np.argmax(spectrum)]),
                        "spectral_mean": float(np.mean(spectrum)),
                        "spectral_std": float(np.std(spectrum)),
                    })

            snd = pd.DataFrame(rows)
            snd.sample(8, random_state=42)
            """
        ),
        code(
            """
            try:
                from sklearn.model_selection import StratifiedKFold, cross_val_score
                from sklearn.ensemble import RandomForestClassifier
                from sklearn.metrics import classification_report

                X = snd.drop(columns=["label"])
                y = snd["label"]
                clf = RandomForestClassifier(n_estimators=100, random_state=42)
                cv = StratifiedKFold(n_splits=5, shuffle=True, random_state=42)
                scores = cross_val_score(clf, X, y, cv=cv)
                clf.fit(X, y)
                pred = clf.predict(X)
                print("5-fold accuracy:", scores.round(3), "mean=", scores.mean().round(3))
                print(classification_report(y, pred))
                save_json("environment_sound_cv.json", {"scores": scores.tolist(), "mean": float(scores.mean())})
            except Exception as exc:
                print("scikit-learn 설치 후 실행하세요:", exc)
            """
        ),
        code(
            """
            timeline = [
                (0.0, "speech", "작업 시작"),
                (1.2, "keyboard", "입력 중"),
                (2.0, "alarm", "안전 경고"),
                (2.6, "speech", "계속 진행"),
                (3.1, "door", "출입문 열림"),
            ]

            def risk_policy(timeline):
                risk = []
                for ts, label, note in timeline:
                    if label == "alarm":
                        risk.append((ts, "STOP", "alarm_detected"))
                    elif label == "speech" and risk and risk[-1][1] == "STOP":
                        risk.append((ts, "REQUIRE_CONFIRMATION", "speech_after_alarm"))
                return risk

            policy_result = pd.DataFrame(risk_policy(timeline), columns=["time", "action", "reason"])
            policy_result.to_csv(ARTIFACTS / "sound_policy_events.csv", index=False, encoding="utf-8-sig")
            policy_result
            """
        ),
        md(
            """
            ## 논문 연결

            최종 시스템에서 환경음은 별도 답변을 만드는 모델이 아니라 `신뢰도 게이트`로 사용합니다.
            예: 알람이 감지되면 음성 명령의 실행 답변을 바로 수행하지 않고 확인 질문을 생성합니다.
            """
        ),
    ]


def ocr_cells() -> list[dict]:
    return [
        md(
            """
            # 04. OCR 강건성 실습: 흐림, 회전, 대비, 문자 오류

            OCR 모델 개발은 깨끗한 이미지에서 끝나지 않습니다. 실제 문서는 흐림, 회전, 낮은 대비, 부분 가림, 다국어 문자 때문에 흔들립니다.
            이 노트북은 합성 문서 이미지를 만들고, 변형 조건별 오류율을 측정하는 실험 골격을 제공합니다.
            """
        ),
        code(COMMON_SETUP),
        code(
            """
            from PIL import Image, ImageDraw, ImageFont, ImageFilter, ImageOps

            def make_doc(text="Invoice No A17\\nTotal 42,000 KRW\\nStatus PAID", width=520, height=220):
                img = Image.new("RGB", (width, height), "white")
                draw = ImageDraw.Draw(img)
                font = ImageFont.load_default()
                y = 25
                for line in text.splitlines():
                    draw.text((30, y), line, fill="black", font=font)
                    y += 38
                return img

            def corrupt(img, blur=0, rotate=0, contrast=1.0, noise=0):
                out = img.copy()
                if blur:
                    out = out.filter(ImageFilter.GaussianBlur(radius=blur))
                if rotate:
                    out = out.rotate(rotate, expand=True, fillcolor="white")
                if contrast != 1.0:
                    gray = ImageOps.grayscale(out)
                    arr = np.asarray(gray).astype(np.float32)
                    arr = np.clip((arr - 128) * contrast + 128, 0, 255).astype(np.uint8)
                    out = Image.fromarray(arr).convert("RGB")
                if noise:
                    arr = np.asarray(out).astype(np.int16)
                    arr = np.clip(arr + np.random.normal(0, noise, arr.shape), 0, 255).astype(np.uint8)
                    out = Image.fromarray(arr)
                return out

            base_text = "Invoice No A17\\nTotal 42,000 KRW\\nStatus PAID"
            base_img = make_doc(base_text)
            variants = {
                "clean": base_img,
                "blur": corrupt(base_img, blur=1.4),
                "rotate": corrupt(base_img, rotate=4),
                "low_contrast": corrupt(base_img, contrast=0.45),
                "noise": corrupt(base_img, noise=18),
            }

            for name, img in variants.items():
                img.save(ARTIFACTS / f"ocr_variant_{name}.png")

            if plt:
                fig, axes = plt.subplots(1, len(variants), figsize=(15, 3))
                for ax, (name, img) in zip(axes, variants.items()):
                    ax.imshow(img)
                    ax.set_title(name)
                    ax.axis("off")
                fig.tight_layout()
            """
        ),
        code(
            """
            def simulate_ocr(text, condition):
                replacements = {
                    "clean": {},
                    "blur": {"A17": "AI7", "42,000": "42000"},
                    "rotate": {"Invoice": "lnvoice"},
                    "low_contrast": {"PAID": "PAlD"},
                    "noise": {"Total": "Tota1", "42,000": "42.000"},
                }
                out = text
                for a, b in replacements.get(condition, {}).items():
                    out = out.replace(a, b)
                return out

            def char_error_rate(ref, hyp):
                a = list(ref.replace("\\n", ""))
                b = list(hyp.replace("\\n", ""))
                return edit_distance(a, b) / max(1, len(a))

            def edit_distance(a, b):
                dp = [[0] * (len(b) + 1) for _ in range(len(a) + 1)]
                for i in range(len(a) + 1):
                    dp[i][0] = i
                for j in range(len(b) + 1):
                    dp[0][j] = j
                for i in range(1, len(a) + 1):
                    for j in range(1, len(b) + 1):
                        dp[i][j] = min(dp[i-1][j] + 1, dp[i][j-1] + 1, dp[i-1][j-1] + (a[i-1] != b[j-1]))
                return dp[-1][-1]

            rows = []
            for condition in variants:
                hyp = simulate_ocr(base_text, condition)
                rows.append({"condition": condition, "reference": base_text, "ocr_text": hyp, "cer": char_error_rate(base_text, hyp)})

            ocr_eval = pd.DataFrame(rows)
            ocr_eval.to_csv(ARTIFACTS / "ocr_robustness_eval.csv", index=False, encoding="utf-8-sig")
            ocr_eval
            """
        ),
        md(
            """
            ## 선택 실습: 실제 OCR 엔진 연결

            `pytesseract` 또는 `easyocr`를 설치한 뒤 `variants` 이미지를 실제 OCR에 넣어 보세요.
            보고서에는 엔진 제공 회사명/서비스명이 필요한 경우 `xxx OCR API`처럼 마스킹합니다.
            """
        ),
    ]


def docqa_cells() -> list[dict]:
    return [
        md(
            """
            # 05. 문서 레이아웃과 DocVQA 미니 베이스라인

            OCR 텍스트만 이어 붙이면 테이블/영수증/청구서에서 위치 정보를 잃습니다.
            이 노트북은 bounding box를 가진 OCR 토큰을 만들고, 텍스트 검색 베이스라인과 레이아웃 힌트 베이스라인을 비교합니다.
            """
        ),
        code(COMMON_SETUP),
        code(
            """
            tokens = pd.DataFrame([
                ("invoice", 30, 20, 100, 40, "header"),
                ("no", 110, 20, 135, 40, "header"),
                ("a17", 145, 20, 180, 40, "header"),
                ("item", 30, 80, 80, 100, "table_header"),
                ("price", 240, 80, 300, 100, "table_header"),
                ("sensor", 30, 120, 120, 140, "row"),
                ("42000", 240, 120, 300, 140, "row"),
                ("status", 30, 170, 95, 190, "footer"),
                ("paid", 240, 170, 290, 190, "footer"),
            ], columns=["text", "x1", "y1", "x2", "y2", "region"])

            questions = pd.DataFrame([
                ("문서 번호는?", "a17", "header"),
                ("가격은?", "42000", "row"),
                ("상태는?", "paid", "footer"),
            ], columns=["question", "answer", "target_region"])

            tokens
            """
        ),
        code(
            """
            def text_only_answer(question, tokens):
                q = question.lower()
                if "번호" in q or "no" in q:
                    candidates = tokens[tokens["text"].str.contains("a|[0-9]", regex=True)]
                elif "가격" in q or "price" in q:
                    candidates = tokens[tokens["text"].str.contains("[0-9]", regex=True)]
                elif "상태" in q or "status" in q:
                    candidates = tokens[tokens["text"].isin(["paid", "unpaid", "done"])]
                else:
                    candidates = tokens
                return candidates.iloc[-1]["text"] if len(candidates) else ""

            def layout_answer(question, tokens, target_region):
                region_tokens = tokens[tokens["region"] == target_region]
                return text_only_answer(question, region_tokens if len(region_tokens) else tokens)

            rows = []
            for _, row in questions.iterrows():
                pred_text = text_only_answer(row.question, tokens)
                pred_layout = layout_answer(row.question, tokens, row.target_region)
                rows.append({
                    "question": row.question,
                    "answer": row.answer,
                    "text_only": pred_text,
                    "layout_aware": pred_layout,
                    "text_only_em": pred_text == row.answer,
                    "layout_em": pred_layout == row.answer,
                })
            docqa_eval = pd.DataFrame(rows)
            docqa_eval.to_csv(ARTIFACTS / "docqa_mini_eval.csv", index=False, encoding="utf-8-sig")
            docqa_eval
            """
        ),
        code(
            """
            if plt:
                fig, ax = plt.subplots(figsize=(6, 3))
                ax.set_xlim(0, 340)
                ax.set_ylim(220, 0)
                for _, r in tokens.iterrows():
                    rect = plt.Rectangle((r.x1, r.y1), r.x2-r.x1, r.y2-r.y1, fill=False)
                    ax.add_patch(rect)
                    ax.text(r.x1, r.y1 - 2, r.text, fontsize=9)
                ax.set_title("OCR tokens with layout boxes")
                ax.axis("off")
                fig.tight_layout()
                fig.savefig(ARTIFACTS / "docqa_layout_boxes.png", dpi=160)
            """
        ),
        md(
            """
            ## 논문 연결

            최종 논문에서 `텍스트만 사용한 QA`와 `레이아웃/품질 힌트를 넣은 QA`를 비교합니다.
            실제 데이터로 확장할 때는 DocVQA, TextVQA, OCRBench 계열의 태스크 정의를 참고하세요.
            """
        ),
    ]


def retrieval_cells() -> list[dict]:
    return [
        md(
            """
            # 06. 이미지-텍스트 검색과 VLM식 증거 검색

            VLM을 학습하기 전에도 이미지 설명, OCR 텍스트, 질문을 같은 검색 공간에 두는 연습이 필요합니다.
            여기서는 TF-IDF로 단순 검색 베이스라인을 만들고 Recall@K를 계산합니다.
            """
        ),
        code(COMMON_SETUP),
        code(
            """
            items = pd.DataFrame([
                ("img001", "receipt total 42000 paid sensor", "가격이 표시된 영수증"),
                ("img002", "warning alarm red light emergency stop", "알람과 정지 안내"),
                ("img003", "manual robot arm calibration joint", "로봇 암 보정 매뉴얼"),
                ("img004", "meeting room schedule projector", "회의실 일정표"),
                ("img005", "invoice number a17 status paid", "청구서 번호와 결제 상태"),
            ], columns=["image_id", "evidence_text", "caption_ko"])

            queries = pd.DataFrame([
                ("q1", "문서 번호 A17이 있는 이미지를 찾아줘", "img005"),
                ("q2", "알람이 울리는 위험 상황", "img002"),
                ("q3", "센서 가격이 있는 영수증", "img001"),
            ], columns=["query_id", "query", "target_image_id"])
            """
        ),
        code(
            """
            try:
                from sklearn.feature_extraction.text import TfidfVectorizer
                from sklearn.metrics.pairwise import cosine_similarity

                corpus = pd.concat([items["evidence_text"], queries["query"]], ignore_index=True)
                vectorizer = TfidfVectorizer(analyzer="char_wb", ngram_range=(2, 4))
                X = vectorizer.fit_transform(corpus)
                item_vecs = X[:len(items)]
                query_vecs = X[len(items):]
                sims = cosine_similarity(query_vecs, item_vecs)

                ranked_rows = []
                for qi, qrow in queries.iterrows():
                    order = np.argsort(-sims[qi])
                    for rank, idx in enumerate(order[:3], start=1):
                        ranked_rows.append({
                            "query_id": qrow.query_id,
                            "rank": rank,
                            "image_id": items.iloc[idx].image_id,
                            "score": float(sims[qi, idx]),
                            "hit": items.iloc[idx].image_id == qrow.target_image_id,
                        })
                ranking = pd.DataFrame(ranked_rows)
                recall_at_1 = ranking[ranking["rank"] == 1]["hit"].mean()
                save_json("retrieval_metrics.json", {"recall_at_1": float(recall_at_1)})
                print("Recall@1:", recall_at_1)
                ranking
            except Exception as exc:
                print("scikit-learn 설치 후 실행하세요:", exc)
            """
        ),
        md(
            """
            ## 확장 과제

            - OCR 텍스트와 이미지 캡션을 따로 색인하고 late fusion을 구현합니다.
            - 실제 VLM 임베딩 모델을 사용할 경우 보고서 본문에는 회사명/조직명을 `xxx`로 마스킹합니다.
            - 검색 실패 사례를 `텍스트 누락`, `동의어`, `숫자/코드`, `시각 단서 부족`으로 분류합니다.
            """
        ),
    ]


def dialogue_cells() -> list[dict]:
    return [
        md(
            """
            # 07. 음성 대화: 의도, 슬롯, 상태, 안전 확인

            음성 대화 모델은 ASR 결과를 그대로 실행하지 않습니다.
            의도 분류, 슬롯 추출, 상태 관리, 위험 조건 확인이 필요합니다.
            """
        ),
        code(COMMON_SETUP),
        code(
            """
            commands = pd.DataFrame([
                ("로봇 팔을 왼쪽 선반으로 이동해", "move", "left shelf"),
                ("문서 번호 A17을 읽어줘", "read_doc", "A17"),
                ("알람이 들리면 멈춰", "safety_rule", "alarm"),
                ("請求書の合計金額を読んで", "read_doc", "total"),
                ("stop if you hear an alarm", "safety_rule", "alarm"),
                ("move to the charging station", "move", "charging station"),
            ], columns=["utterance", "intent", "slot"])

            def classify_intent(text):
                lower = text.lower()
                if any(k in lower for k in ["move", "이동", "station", "선반"]):
                    return "move"
                if any(k in lower for k in ["문서", "읽", "read", "請求書", "金額"]):
                    return "read_doc"
                if any(k in lower for k in ["alarm", "알람", "멈", "stop"]):
                    return "safety_rule"
                return "unknown"

            def extract_slot(text):
                import re
                m = re.search(r"[A-Z]\\d+", text.upper())
                if m:
                    return m.group(0)
                for key in ["alarm", "알람", "left shelf", "charging station", "total"]:
                    if key in text.lower():
                        return key
                if "金額" in text:
                    return "total"
                return ""

            commands["pred_intent"] = commands["utterance"].map(classify_intent)
            commands["pred_slot"] = commands["utterance"].map(extract_slot)
            commands["intent_ok"] = commands.intent == commands.pred_intent
            commands
            """
        ),
        code(
            """
            class DialogueState:
                def __init__(self):
                    self.safety_stop = False
                    self.pending_action = None

                def handle(self, utterance, sound_event=None):
                    intent = classify_intent(utterance)
                    slot = extract_slot(utterance)
                    if sound_event == "alarm":
                        self.safety_stop = True
                        self.pending_action = None
                        return {"action": "stop", "reason": "alarm_detected", "intent": intent, "slot": slot}
                    if self.safety_stop and intent == "move":
                        self.pending_action = (intent, slot)
                        return {"action": "ask_confirmation", "reason": "move_after_alarm", "intent": intent, "slot": slot}
                    if intent == "move":
                        return {"action": "execute_move", "target": slot}
                    if intent == "read_doc":
                        return {"action": "run_ocr_docqa", "target": slot}
                    if intent == "safety_rule":
                        return {"action": "update_policy", "trigger": slot}
                    return {"action": "fallback"}

            state = DialogueState()
            scenario = [
                ("문서 번호 A17을 읽어줘", None),
                ("왼쪽 선반으로 이동해", "alarm"),
                ("계속 이동해", None),
            ]
            pd.DataFrame([state.handle(u, s) for u, s in scenario])
            """
        ),
        md(
            """
            ## 논문 연결

            대화 상태는 최종 파이프라인의 사용자 가치 부분입니다.
            논문 실험에서는 `알람 감지 후 잘못된 실행을 얼마나 줄이는가`를 safety metric으로 둘 수 있습니다.
            """
        ),
    ]


def data_quality_cells() -> list[dict]:
    return [
        md(
            """
            # 08. 대규모 데이터 수집/전처리/품질 설계

            대규모 실제 데이터 경험은 모델 코드보다 데이터 계약에서 드러납니다.
            여기서는 멀티모달 샘플 스키마, 회사명 마스킹, split 누수 검사, 품질 점수를 만듭니다.
            """
        ),
        code(COMMON_SETUP),
        code(
            """
            raw = pd.DataFrame([
                ("session01", "audio001.wav", "doc001.png", "회사명: Alpha / invoice A17", "clean", 0.95),
                ("session01", "audio002.wav", "doc001.png", "same document repeated", "noise", 0.71),
                ("session02", "audio003.wav", "doc002.png", "소속: Beta / alarm log", "alarm", 0.88),
                ("session03", "audio004.wav", "doc003.png", "client: Gamma / handwritten note", "blur", 0.63),
                ("session04", "audio005.wav", "doc004.png", "public manual page", "clean", 0.92),
            ], columns=["session_id", "audio_path", "image_path", "raw_text", "condition", "source_quality"])

            import re
            def mask_sensitive(text):
                text = re.sub(r"(회사명|소속|client)\\s*[:：]\\s*[^/\\n]+", r"\\1: xxx ", text, flags=re.IGNORECASE)
                text = re.sub(r"[A-Za-z0-9._%+-]+@[A-Za-z0-9.-]+", "xxx@xxx", text)
                return text

            raw["masked_text"] = raw["raw_text"].map(mask_sensitive)
            raw["has_audio"] = raw.audio_path.str.endswith(".wav")
            raw["has_image"] = raw.image_path.str.endswith(".png")
            raw["quality_bucket"] = pd.cut(raw.source_quality, bins=[0, 0.7, 0.85, 1.0], labels=["low", "mid", "high"])
            raw
            """
        ),
        code(
            """
            def assign_split(session_id):
                # 세션 단위 split으로 같은 문서/녹음 조건이 train/test에 동시에 들어가는 누수를 줄입니다.
                h = sum(ord(c) for c in session_id) % 10
                if h < 7:
                    return "train"
                if h < 9:
                    return "valid"
                return "test"

            raw["split"] = raw["session_id"].map(assign_split)
            leakage = raw.groupby("session_id")["split"].nunique().reset_index(name="n_splits")
            assert leakage["n_splits"].max() == 1, "같은 session_id가 여러 split에 섞였습니다."

            data_card = {
                "task": "multimodal field document QA",
                "modalities": ["speech", "environment sound", "document image", "text"],
                "sensitive_fields": ["company_name", "email", "client_name"],
                "masking_policy": "본문/로그/보고서에는 회사명과 고객명을 xxx로 표기",
                "split_policy": "session_id 단위 분리",
                "known_risks": ["ASR noise bias", "OCR low contrast", "layout leakage", "language imbalance"],
            }
            save_json("data_card.json", data_card)
            raw.to_csv(ARTIFACTS / "multimodal_dataset_manifest.csv", index=False, encoding="utf-8-sig")
            raw
            """
        ),
        md(
            """
            ## 확장 과제

            - 언어별, 소음 조건별, 문서 유형별 샘플 수를 집계합니다.
            - 품질 낮은 샘플을 버리는 실험과 보정해서 쓰는 실험을 비교합니다.
            - 사내/고객 이름은 논문 표, 로그, 파일명에서 모두 `xxx`로 남도록 테스트를 추가합니다.
            """
        ),
    ]


def experiment_tracking_cells() -> list[dict]:
    return [
        md(
            """
            # 09. 실험 관리, 평가표, 모델 카드

            연구를 논문으로 만들려면 `무엇을 바꿨고`, `왜 좋아졌고`, `어떤 조건에서 실패했는지`가 남아야 합니다.
            이 노트북은 CSV 기반의 경량 실험 추적과 모델 카드 생성을 연습합니다.
            """
        ),
        code(COMMON_SETUP),
        code(
            """
            experiments = pd.DataFrame([
                ("E001", "text_only", "OCR text only", 0.62, 0.00, 120, "baseline"),
                ("E002", "ocr_layout", "OCR + layout region", 0.71, 0.00, 145, "layout helps table fields"),
                ("E003", "asr_ocr", "ASR command + OCR", 0.74, 0.08, 180, "speech intent reduces ambiguity"),
                ("E004", "asr_ocr_sound_gate", "ASR + OCR + sound safety gate", 0.76, 0.03, 210, "fewer unsafe actions"),
            ], columns=["run_id", "system", "description", "qa_accuracy", "unsafe_action_rate", "latency_ms", "note"])

            experiments["score"] = experiments["qa_accuracy"] - 2.0 * experiments["unsafe_action_rate"] - 0.0002 * experiments["latency_ms"]
            experiments.to_csv(ARTIFACTS / "experiment_log.csv", index=False, encoding="utf-8-sig")
            experiments.sort_values("score", ascending=False)
            """
        ),
        code(
            """
            best = experiments.sort_values("score", ascending=False).iloc[0].to_dict()
            model_card = f'''
            # Model Card: {best["system"]}

            ## Intended Use
            현장형 문서/상황 QA에서 음성 명령, OCR 텍스트, 환경음 게이트를 결합해 답변 또는 안전 확인을 수행한다.

            ## Metrics
            - QA accuracy: {best["qa_accuracy"]:.3f}
            - Unsafe action rate: {best["unsafe_action_rate"]:.3f}
            - Latency: {best["latency_ms"]:.0f} ms

            ## Data Handling
            회사명, 고객명, 소속명은 모든 보고 산출물에서 xxx로 마스킹한다.

            ## Limitations
            낮은 대비 문서, 겹쳐 말하기, 다국어 숫자 코드에서 오류가 발생할 수 있다.
            '''
            path = ARTIFACTS / "MODEL_CARD.md"
            path.write_text(dedent(model_card).strip() + "\\n", encoding="utf-8")
            print(path)
            """
        ),
        md(
            """
            ## 리더십 연결

            팀 리드 역할에서는 실험표가 의사결정 도구입니다.
            코드 리뷰 때 `정확도만 올랐는가`, `latency/safety/regression은 어떤가`, `재현 가능한가`를 함께 봅니다.
            """
        ),
    ]


def review_playbook_cells() -> list[dict]:
    return [
        md(
            """
            # 10. 기술 리드: 코드 리뷰, 멘토링, 의사결정 로그

            5~10명 규모 팀을 이끌려면 모델 성능만큼 리뷰 기준과 의사결정 기록이 중요합니다.
            이 노트북은 멀티모달 ML 코드 리뷰 체크리스트와 ADR(Architecture Decision Record)을 생성합니다.
            """
        ),
        code(COMMON_SETUP),
        code(
            """
            review_items = pd.DataFrame([
                ("data", "회사명/고객명/이메일이 xxx로 마스킹되는가?", "privacy"),
                ("data", "train/valid/test가 session 단위로 분리되는가?", "leakage"),
                ("audio", "샘플레이트, 채널, loudness 정규화가 명시되는가?", "reproducibility"),
                ("ocr", "회전/흐림/저대비 조건별 평가가 있는가?", "robustness"),
                ("eval", "주요 metric 외 failure taxonomy가 있는가?", "research"),
                ("mlops", "run_id, seed, commit, dependency가 기록되는가?", "operation"),
                ("safety", "환경음 경고 조건에서 실행 전 확인을 하는가?", "product"),
            ], columns=["area", "question", "risk_type"])

            review_items.to_csv(ARTIFACTS / "code_review_checklist.csv", index=False, encoding="utf-8-sig")
            review_items
            """
        ),
        code(
            """
            adr = {
                "id": "ADR-001",
                "title": "Use confidence-gated multimodal fusion before end-to-end large model training",
                "status": "proposed",
                "context": "데이터 규모가 작고 안전 요구가 있어 해석 가능한 베이스라인이 필요하다.",
                "decision": "ASR, OCR, 환경음 모듈을 분리 평가하고 confidence gate로 결합한다.",
                "consequences": [
                    "오류 원인 분석이 쉽다.",
                    "대형 end-to-end 모델보다 성능 상한은 낮을 수 있다.",
                    "논문 ablation이 명확해진다.",
                ],
            }
            save_json("ADR-001.json", adr)
            adr
            """
        ),
        code(
            """
            jp_brief_template = '''
            件名: マルチモーダルQA実験の進捗共有

            目的:
            音声認識、OCR、環境音ゲートを組み合わせ、ノイズ環境でのQA精度と安全性を評価します。

            今週の結果:
            - ベースライン:
            - 改善点:
            - 未解決課題:

            次のアクション:
            - エラー分析を行い、アブレーション実験を追加します。
            '''
            (ARTIFACTS / "jp_technical_brief_template.md").write_text(dedent(jp_brief_template).strip() + "\\n", encoding="utf-8")
            print(ARTIFACTS / "jp_technical_brief_template.md")
            """
        ),
    ]


def literature_cells() -> list[dict]:
    return [
        md(
            """
            # 11. 문헌 조사 매트릭스와 연구 질문 만들기

            최신 기술 트렌드를 따라가는 목적은 모델 이름을 외우는 것이 아니라,
            `어떤 평가가 아직 부족한지`, `내 실험이 어떤 gap을 메우는지`를 찾는 것입니다.
            """
        ),
        code(COMMON_SETUP),
        code(
            """
            papers = pd.DataFrame([
                ("MSEB", "audio benchmark", "audio embeddings need broad evaluation", "audio tasks: transcription, classification, retrieval, reasoning", "환경음+ASR 평가축 설계"),
                ("OCRBench v2", "OCR/VLM benchmark", "text localization and reasoning remain hard", "31 scenarios, difficult text-centric QA", "OCR 조건별 오류 taxonomy"),
                ("TrOCR", "OCR model", "transformer encoder-decoder can do end-to-end OCR", "printed, handwritten, scene text", "OCR baseline and robustness"),
                ("TextVQA", "text-rich VQA", "VQA needs reading and reasoning over image text", "45k+ questions over 28k+ images", "문서/이미지 QA baseline"),
                ("DocVQA", "document QA", "purpose-driven document understanding", "document images with user questions", "최종 태스크 형식"),
            ], columns=["paper", "area", "main_claim", "evidence", "how_used"])

            papers["gap_for_my_paper"] = [
                "음성과 환경음을 문서QA safety gate로 쓰는 분석이 부족함",
                "텍스트 중심 VLM 평가는 많지만 음성 명령과 결합한 현장 QA 평가는 부족함",
                "OCR 자체보다 downstream QA와 안전 정책에 미치는 영향 분석 필요",
                "장면 텍스트 QA를 현장 음성 명령과 연결하는 실험 필요",
                "문서 QA에 환경음/음성 신뢰도 조건을 넣는 프로토콜 제안 가능",
            ]
            papers.to_csv(ARTIFACTS / "literature_review_matrix.csv", index=False, encoding="utf-8-sig")
            papers
            """
        ),
        code(
            """
            research_questions = [
                "RQ1. ASR+OCR 결합은 OCR-only 문서 QA보다 노이즈 조건에서 정확도를 높이는가?",
                "RQ2. 환경음 safety gate는 QA 정확도를 크게 희생하지 않고 unsafe action rate를 낮추는가?",
                "RQ3. OCR 품질 점수와 ASR 오류 유형을 사용한 confidence fusion은 단순 결합보다 강건한가?",
            ]
            hypotheses = [
                "H1. 음성 의도/슬롯은 문서 내 후보 영역을 줄여 정확도를 높인다.",
                "H2. 알람/문 열림 같은 환경음은 실행형 응답에서 안전 확인을 유도해 위험 행동을 줄인다.",
                "H3. 품질 점수 기반 fusion은 흐림/소음 조건에서 성능 하락을 완화한다.",
            ]
            save_json("research_questions.json", {"RQ": research_questions, "H": hypotheses})
            pd.DataFrame({"research_question": research_questions, "hypothesis": hypotheses})
            """
        ),
    ]


def ablation_cells() -> list[dict]:
    return [
        md(
            """
            # 12. 재현, ablation, 통계적 비교

            논문 한 편을 만들려면 `내 방법이 좋아 보인다`가 아니라 `무엇 때문에 좋아졌는지`를 보여야 합니다.
            이 노트북은 실험 조건별 결과를 만들고 bootstrap 신뢰구간을 계산합니다.
            """
        ),
        code(COMMON_SETUP),
        code(
            """
            rng = np.random.default_rng(42)
            n = 200
            conditions = rng.choice(["clean", "noise", "blur", "alarm"], size=n, p=[0.35, 0.25, 0.25, 0.15])

            def simulate_correct(system, cond):
                base = {"clean": 0.72, "noise": 0.52, "blur": 0.48, "alarm": 0.55}[cond]
                bonus = {
                    "ocr_only": 0.00,
                    "ocr_layout": 0.07 if cond in ["blur", "clean"] else 0.03,
                    "asr_ocr": 0.10 if cond in ["noise", "clean"] else 0.05,
                    "asr_ocr_sound_gate": 0.11 if cond != "blur" else 0.05,
                }[system]
                return rng.random() < min(0.95, base + bonus)

            systems = ["ocr_only", "ocr_layout", "asr_ocr", "asr_ocr_sound_gate"]
            rows = []
            for i, cond in enumerate(conditions):
                for sys in systems:
                    rows.append({"sample_id": i, "condition": cond, "system": sys, "correct": simulate_correct(sys, cond)})
            res = pd.DataFrame(rows)
            pivot = res.groupby(["system", "condition"])["correct"].mean().reset_index()
            pivot.to_csv(ARTIFACTS / "ablation_by_condition.csv", index=False, encoding="utf-8-sig")
            pivot
            """
        ),
        code(
            """
            def bootstrap_ci(values, n_boot=2000, alpha=0.05):
                vals = np.asarray(values).astype(float)
                boots = [np.mean(np.random.choice(vals, size=len(vals), replace=True)) for _ in range(n_boot)]
                return float(np.mean(vals)), float(np.quantile(boots, alpha / 2)), float(np.quantile(boots, 1 - alpha / 2))

            ci_rows = []
            for sys in systems:
                vals = res[res.system == sys]["correct"]
                mean, lo, hi = bootstrap_ci(vals)
                ci_rows.append({"system": sys, "accuracy": mean, "ci_low": lo, "ci_high": hi})
            ci = pd.DataFrame(ci_rows).sort_values("accuracy", ascending=False)
            ci.to_csv(ARTIFACTS / "ablation_bootstrap_ci.csv", index=False, encoding="utf-8-sig")
            ci
            """
        ),
        code(
            """
            if plt:
                fig, ax = plt.subplots(figsize=(8, 4))
                order = ci.sort_values("accuracy")["system"]
                plot_df = ci.set_index("system").loc[order]
                ax.barh(plot_df.index, plot_df["accuracy"], xerr=[
                    plot_df["accuracy"] - plot_df["ci_low"],
                    plot_df["ci_high"] - plot_df["accuracy"],
                ])
                ax.set_xlabel("Accuracy")
                ax.set_title("Ablation with bootstrap CI")
                fig.tight_layout()
                fig.savefig(ARTIFACTS / "ablation_ci.png", dpi=160)
            """
        ),
    ]


def final_prototype_cells() -> list[dict]:
    return [
        md(
            """
            # 13. 최종 논문 프로토타입: Confidence-Gated Multimodal QA

            앞 단계의 ASR, OCR, 환경음, 데이터 품질, ablation을 하나로 묶습니다.
            이 노트북의 결과표와 그림은 논문 `Method`, `Experiments`, `Results`의 뼈대가 됩니다.
            """
        ),
        code(COMMON_SETUP),
        code(
            """
            rng = np.random.default_rng(7)
            dataset = pd.DataFrame({
                "sample_id": range(120),
                "ocr_quality": rng.uniform(0.35, 0.98, 120),
                "asr_confidence": rng.uniform(0.40, 0.99, 120),
                "sound_risk": rng.choice([0, 1], size=120, p=[0.82, 0.18]),
                "layout_match": rng.choice([0, 1], size=120, p=[0.30, 0.70]),
            })
            dataset["difficulty"] = 1 - (0.45 * dataset.ocr_quality + 0.35 * dataset.asr_confidence + 0.20 * dataset.layout_match)

            def system_predict(row, system):
                if system == "ocr_only":
                    p = 0.40 + 0.45 * row.ocr_quality
                    unsafe = row.sound_risk and rng.random() < 0.45
                elif system == "asr_ocr":
                    p = 0.35 + 0.30 * row.ocr_quality + 0.25 * row.asr_confidence + 0.10 * row.layout_match
                    unsafe = row.sound_risk and rng.random() < 0.32
                elif system == "gated_fusion":
                    gate = min(row.ocr_quality, row.asr_confidence)
                    p = 0.30 + 0.30 * row.ocr_quality + 0.25 * row.asr_confidence + 0.12 * row.layout_match + 0.10 * gate
                    unsafe = row.sound_risk and rng.random() < 0.08
                else:
                    raise ValueError(system)
                return rng.random() < min(0.95, p), bool(unsafe)

            rows = []
            for _, row in dataset.iterrows():
                for system in ["ocr_only", "asr_ocr", "gated_fusion"]:
                    correct, unsafe = system_predict(row, system)
                    rows.append({"sample_id": row.sample_id, "system": system, "correct": correct, "unsafe": unsafe})
            out = pd.DataFrame(rows)
            metrics = out.groupby("system").agg(
                qa_accuracy=("correct", "mean"),
                unsafe_action_rate=("unsafe", "mean"),
            ).reset_index()
            metrics["paper_score"] = metrics.qa_accuracy - 2 * metrics.unsafe_action_rate
            metrics.to_csv(ARTIFACTS / "final_prototype_metrics.csv", index=False, encoding="utf-8-sig")
            metrics.sort_values("paper_score", ascending=False)
            """
        ),
        code(
            """
            method = '''
            Proposed method.
            Given ASR transcript confidence c_a, OCR quality c_o, layout match score l, and environmental sound risk r,
            the system computes an evidence score s = w_a c_a + w_o c_o + w_l l + w_g min(c_a, c_o).
            If r indicates a safety-critical event, execution-type answers are converted into confirmation prompts.
            Otherwise, the answer is generated from the highest-scoring OCR/layout evidence.
            '''
            (ARTIFACTS / "method_section_draft.md").write_text(dedent(method).strip() + "\\n", encoding="utf-8")
            print(ARTIFACTS / "method_section_draft.md")
            """
        ),
        code(
            """
            if plt:
                fig, axes = plt.subplots(1, 2, figsize=(9, 3.5))
                axes[0].bar(metrics["system"], metrics["qa_accuracy"])
                axes[0].set_ylim(0, 1)
                axes[0].set_title("QA accuracy")
                axes[0].tick_params(axis="x", rotation=20)

                axes[1].bar(metrics["system"], metrics["unsafe_action_rate"], color="tomato")
                axes[1].set_ylim(0, max(0.5, metrics["unsafe_action_rate"].max() + 0.05))
                axes[1].set_title("Unsafe action rate")
                axes[1].tick_params(axis="x", rotation=20)
                fig.tight_layout()
                fig.savefig(ARTIFACTS / "final_prototype_metrics.png", dpi=160)
            """
        ),
        md(
            """
            ## 논문 기여문 초안

            1. 음성 명령, OCR 문서 증거, 환경음 위험 신호를 함께 평가하는 경량 재현 프로토콜을 제안한다.
            2. ASR/OCR confidence와 환경음 safety gate를 결합한 해석 가능한 fusion 방법을 제시한다.
            3. 노이즈, 흐림, 알람 조건에서 QA 정확도와 unsafe action rate를 동시에 분석한다.
            """
        ),
    ]


def paper_draft_cells() -> list[dict]:
    return [
        md(
            """
            # 14. 논문 초안, 표/그림, 제출 체크리스트

            이 마지막 노트북은 앞 노트북의 산출물을 모아 논문 골격을 만듭니다.
            실제 제출 전에는 공개 데이터셋 라이선스, 재현 스크립트, 익명화 규정을 반드시 확인하세요.
            """
        ),
        code(COMMON_SETUP),
        code(
            """
            paper = {
                "title": "Confidence-Gated Multimodal Evidence Fusion for Robust Field Document Question Answering",
                "abstract_points": [
                    "현장형 문서 QA는 음성 명령 오류, OCR 품질 저하, 환경음 위험 신호 때문에 실패할 수 있다.",
                    "본 연구는 ASR confidence, OCR quality, layout evidence, environmental sound gate를 결합한다.",
                    "합성 및 공개 데이터 기반 프로토콜에서 QA 정확도와 unsafe action rate를 함께 평가한다.",
                    "결과는 해석 가능한 confidence gate가 정확도와 안전성 균형을 개선할 가능성을 보인다.",
                ],
                "sections": [
                    "Introduction",
                    "Related Work",
                    "Task and Dataset Protocol",
                    "Method",
                    "Experiments",
                    "Results and Error Analysis",
                    "Limitations and Ethics",
                    "Conclusion",
                ],
            }
            save_json("paper_outline.json", paper)
            paper
            """
        ),
        code(
            """
            latex = r'''
            \\begin{table}[t]
            \\centering
            \\caption{Main results. Company and client names are masked as xxx in all examples.}
            \\begin{tabular}{lcc}
            \\hline
            System & QA Accuracy $\\uparrow$ & Unsafe Action Rate $\\downarrow$ \\\\
            \\hline
            OCR-only & -- & -- \\\\
            ASR+OCR & -- & -- \\\\
            Gated Fusion & -- & -- \\\\
            \\hline
            \\end{tabular}
            \\end{table}
            '''
            (ARTIFACTS / "paper_table_template.tex").write_text(dedent(latex).strip() + "\\n", encoding="utf-8")
            print(ARTIFACTS / "paper_table_template.tex")
            """
        ),
        code(
            """
            checklist = pd.DataFrame([
                ("data", "회사명/고객명/개인정보가 xxx로 마스킹되었는가?", False),
                ("data", "데이터 라이선스와 사용 범위가 명시되었는가?", False),
                ("experiment", "seed, split, metric, baseline이 재현 가능한가?", False),
                ("experiment", "ablation과 failure analysis가 포함되었는가?", False),
                ("paper", "기여점 3개가 실험 결과와 직접 연결되는가?", False),
                ("paper", "한계와 윤리 항목이 과장 없이 작성되었는가?", False),
                ("release", "README, requirements, 실행 순서가 정리되었는가?", False),
            ], columns=["area", "item", "done"])
            checklist.to_csv(ARTIFACTS / "paper_submission_checklist.csv", index=False, encoding="utf-8-sig")
            checklist
            """
        ),
        md(
            """
            ## 추천 제출 경로

            - 첫 목표: 워크숍/국내 학회/사내 기술 리포트 수준의 4~6쪽 논문
            - 확장 목표: ICASSP, INTERSPEECH, ACL/EMNLP workshop, CVPR workshop 계열의 멀티모달/문서/오디오 태스크
            - 대회형 확장: DocVQA/TextVQA/OCRBench 계열 태스크 또는 환경음 분류 태스크에 ablation 리포트로 참여
            """
        ),
    ]


NOTEBOOKS = [
    ("00_orientation", "00_job_to_research_roadmap.ipynb", orientation_cells),
    ("01_audio_speech_language", "01_audio_feature_baselines.ipynb", audio_feature_cells),
    ("01_audio_speech_language", "02_asr_evaluation_error_analysis.ipynb", asr_eval_cells),
    ("01_audio_speech_language", "03_environmental_sound_understanding.ipynb", environmental_sound_cells),
    ("02_vision_ocr_language", "01_ocr_robustness_lab.ipynb", ocr_cells),
    ("02_vision_ocr_language", "02_document_layout_docvqa_baseline.ipynb", docqa_cells),
    ("02_vision_ocr_language", "03_image_text_retrieval_vlm_baseline.ipynb", retrieval_cells),
    ("03_multimodal_dialogue", "01_speech_dialogue_state_machine.ipynb", dialogue_cells),
    ("04_data_mlops_leadership", "01_data_engineering_quality.ipynb", data_quality_cells),
    ("04_data_mlops_leadership", "02_experiment_tracking_and_model_cards.ipynb", experiment_tracking_cells),
    ("04_data_mlops_leadership", "03_code_review_mentoring_playbook.ipynb", review_playbook_cells),
    ("05_research_paper_project", "01_literature_review_matrix.ipynb", literature_cells),
    ("05_research_paper_project", "02_reproduction_and_ablation.ipynb", ablation_cells),
    ("05_research_paper_project", "03_final_multimodal_paper_prototype.ipynb", final_prototype_cells),
    ("05_research_paper_project", "04_paper_figures_and_draft.ipynb", paper_draft_cells),
]


README = """
# 멀티모달 음성·비전·언어 연구 커리큘럼

이 커리큘럼은 음성 인식, 음성 대화, 환경음 이해, OCR/문서 이해, VLM, 데이터 전처리, 실험 관리, 기술 리드, 논문 작성 역량을 한 번의 연구 프로젝트로 연결합니다.

회사명, 고객명, 소속명처럼 민감하거나 특정 회사를 가리키는 정보는 본문과 산출물에서 `xxx`로 표기합니다.

## 최종 논문 목표

**Confidence-Gated Multimodal Evidence Fusion for Robust Field Document Question Answering**

핵심 질문은 다음과 같습니다.

- ASR+OCR 결합은 OCR-only 문서 QA보다 노이즈 조건에서 정확도를 높이는가?
- 환경음 safety gate는 QA 정확도를 크게 희생하지 않고 unsafe action rate를 낮추는가?
- OCR 품질 점수와 ASR 오류 유형을 사용한 confidence fusion은 단순 결합보다 강건한가?

## 실행 순서

1. `00_orientation/00_job_to_research_roadmap.ipynb`
2. `01_audio_speech_language/01_audio_feature_baselines.ipynb`
3. `01_audio_speech_language/02_asr_evaluation_error_analysis.ipynb`
4. `01_audio_speech_language/03_environmental_sound_understanding.ipynb`
5. `02_vision_ocr_language/01_ocr_robustness_lab.ipynb`
6. `02_vision_ocr_language/02_document_layout_docvqa_baseline.ipynb`
7. `02_vision_ocr_language/03_image_text_retrieval_vlm_baseline.ipynb`
8. `03_multimodal_dialogue/01_speech_dialogue_state_machine.ipynb`
9. `04_data_mlops_leadership/01_data_engineering_quality.ipynb`
10. `04_data_mlops_leadership/02_experiment_tracking_and_model_cards.ipynb`
11. `04_data_mlops_leadership/03_code_review_mentoring_playbook.ipynb`
12. `05_research_paper_project/01_literature_review_matrix.ipynb`
13. `05_research_paper_project/02_reproduction_and_ablation.ipynb`
14. `05_research_paper_project/03_final_multimodal_paper_prototype.ipynb`
15. `05_research_paper_project/04_paper_figures_and_draft.ipynb`

## 폴더 구조

```text
00_orientation/                  직무 요구사항을 역량 지도와 연구 주제로 변환
01_audio_speech_language/         음성 특징량, ASR 평가, 환경음 이해
02_vision_ocr_language/           OCR 강건성, 문서 레이아웃 QA, 이미지-텍스트 검색
03_multimodal_dialogue/           음성 대화 상태와 안전 확인 정책
04_data_mlops_leadership/         데이터 품질, 실험 관리, 모델 카드, 코드 리뷰
05_research_paper_project/        문헌조사, 재현/ablation, 최종 논문 프로토타입
data/                             공개 데이터셋 연결 메모
templates/                        문헌조사/실험로그/리뷰 체크리스트 템플릿
scripts/                          노트북 생성 및 검증 스크립트
```

## 환경 준비

최소 실습:

```powershell
python -m pip install -r requirements-minimal.txt
```

선택 실습:

```powershell
python -m pip install -r requirements-optional.txt
```

대형 모델이나 공개 데이터셋을 내려받는 셀은 기본 실행 경로에서 제외했습니다. 먼저 합성 데이터 실습으로 평가 틀을 만들고, 이후 공개 데이터셋으로 교체하세요.

## 완료 산출물

- `artifacts/skill_to_notebook_map.csv`
- `artifacts/asr_error_summary.csv`
- `artifacts/ocr_robustness_eval.csv`
- `artifacts/docqa_mini_eval.csv`
- `artifacts/data_card.json`
- `artifacts/experiment_log.csv`
- `artifacts/literature_review_matrix.csv`
- `artifacts/ablation_bootstrap_ci.csv`
- `artifacts/final_prototype_metrics.csv`
- `artifacts/paper_outline.json`
- `artifacts/paper_table_template.tex`

이 산출물을 모으면 4~6쪽 워크숍 논문 또는 기술 리포트 초안을 작성할 수 있습니다.
"""


PAPER_BLUEPRINT = """
# 논문 작성 블루프린트

## 제목

Confidence-Gated Multimodal Evidence Fusion for Robust Field Document Question Answering

## 문제 정의

현장형 문서 QA 시스템은 다음 세 가지 실패 요인을 동시에 겪습니다.

- 음성 명령은 노이즈, 겹침 발화, 다국어 숫자 코드에서 오류가 생깁니다.
- OCR은 흐림, 회전, 낮은 대비, 레이아웃 복잡도에서 오류가 생깁니다.
- 환경음은 작업 실행 여부를 바꿔야 하는 안전 신호가 될 수 있습니다.

## 제안 방법

ASR confidence, OCR quality, layout match, environmental sound risk를 분리 추정한 뒤,
confidence-gated fusion으로 QA 후보를 선택하고 위험 환경음이 있으면 실행형 응답을 확인 질문으로 바꿉니다.

## 최소 실험

- Baseline 1: OCR-only text QA
- Baseline 2: OCR + layout hint
- Baseline 3: ASR + OCR
- Proposed: ASR + OCR + layout + sound safety gate

## 주요 지표

- QA accuracy / exact match
- CER/WER by condition
- unsafe action rate
- latency
- condition-wise robustness
- bootstrap confidence interval

## 논문 기여점

1. 음성, OCR, 환경음 신호를 함께 다루는 현장형 문서 QA 평가 프로토콜
2. 해석 가능한 confidence-gated multimodal fusion
3. 정확도와 안전 지표를 함께 보는 ablation 및 오류 분석

## 한계

- 합성 데이터만으로는 실제 현장 분포를 대표할 수 없습니다.
- 대형 end-to-end 모델과 직접 비교하려면 충분한 공개 데이터와 GPU 예산이 필요합니다.
- 회사명, 고객명, 사내 문서명은 모두 `xxx`로 마스킹해야 합니다.
"""


REFERENCES = """
# 참고 문헌 및 데이터셋 출발점

회사명/조직명은 학습 노트 본문에서 직접 노출하지 않고 필요 시 `xxx`로 표기합니다.
재현을 위해 링크는 원문 주소를 유지합니다.

## 오디오·음성

- Massive Sound Embedding Benchmark (MSEB), 2026: https://arxiv.org/abs/2602.07143
- AudioSet 논문: https://dl.acm.org/doi/10.1109/ICASSP.2017.7952261
- Common Voice 논문: https://arxiv.org/abs/1912.06670
- ESC-50 환경음 데이터셋: https://github.com/karolpiczak/esc-50

## OCR·문서·VQA

- OCRBench v2, 2025 revision: https://arxiv.org/abs/2501.00321
- TrOCR: https://arxiv.org/abs/2109.10282
- TextVQA: https://arxiv.org/abs/1904.08920
- DocVQA: https://www.docvqa.org/

## 실험 설계 팁

- 최신 SOTA 이름을 나열하기보다 `평가 태스크`, `데이터 조건`, `오류 유형`, `재현 가능성`을 먼저 정리합니다.
- 논문 초안에서는 공개 모델 제공 조직명이나 API 제공 회사명을 노출해야 할 때 `xxx`로 마스킹하고, 비공개 재현 메모에만 원본을 둡니다.
"""


DATA_README = """
# data 폴더 사용법

이 폴더에는 대용량 원본 데이터를 커밋하지 않습니다.

권장 구조:

```text
data/
  raw/                 원본 공개 데이터 또는 직접 수집 데이터
  interim/             변환 중간 산출물
  processed/           학습/평가용 manifest
  sample/              커밋 가능한 작은 예제
```

민감 정보 규칙:

- 회사명, 고객명, 소속명, 이메일은 `xxx`로 마스킹합니다.
- 파일명에도 특정 회사명을 넣지 않습니다.
- 외부 공개 데이터셋은 라이선스와 사용 제한을 `DATA_LICENSES.md`에 기록합니다.
"""


REVIEW_TEMPLATE = """
# 코드 리뷰 체크리스트

- [ ] 회사명/고객명/개인정보가 `xxx`로 마스킹된다.
- [ ] 데이터 split이 세션/문서 단위로 분리되어 누수가 없다.
- [ ] ASR은 WER/CER과 오류 유형을 함께 기록한다.
- [ ] OCR은 흐림/회전/저대비 조건별 평가를 포함한다.
- [ ] 환경음 safety gate가 실행형 명령에 적용된다.
- [ ] 실험 run id, seed, commit, dependency가 기록된다.
- [ ] 모델 카드와 데이터 카드가 최신이다.
- [ ] 논문 표/그림을 재생성하는 명령이 문서화되어 있다.
"""


REQ_MINIMAL = """
numpy
pandas
matplotlib
scikit-learn
pillow
jupyter
ipykernel
nbformat
"""


REQ_OPTIONAL = """
torch
torchaudio
transformers
datasets
evaluate
jiwer
librosa
soundfile
opencv-python
pytesseract
easyocr
sentence-transformers
mlflow
"""


def write_text(path: Path, text: str) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(dedent(text).strip() + "\n", encoding="utf-8")


def main() -> None:
    BASE.mkdir(parents=True, exist_ok=True)
    for folder, filename, factory in NOTEBOOKS:
        out_dir = BASE / folder
        out_dir.mkdir(parents=True, exist_ok=True)
        nb = notebook(factory())
        (out_dir / filename).write_text(json.dumps(nb, ensure_ascii=False, indent=2), encoding="utf-8")

    write_text(BASE / "README.md", README)
    write_text(BASE / "paper_blueprint.md", PAPER_BLUEPRINT)
    write_text(BASE / "references.md", REFERENCES)
    write_text(BASE / "data" / "README.md", DATA_README)
    write_text(BASE / "templates" / "review_checklist.md", REVIEW_TEMPLATE)
    write_text(BASE / "requirements-minimal.txt", REQ_MINIMAL)
    write_text(BASE / "requirements-optional.txt", REQ_OPTIONAL)
    write_text(
        BASE / "templates" / "literature_review_matrix.csv",
        "paper,area,main_claim,evidence,how_used,gap_for_my_paper\n",
    )
    write_text(
        BASE / "templates" / "experiment_log.csv",
        "run_id,system,dataset,seed,metric,value,latency_ms,notes\n",
    )

    print(f"Generated {len(NOTEBOOKS)} notebooks under {BASE}")


if __name__ == "__main__":
    main()
