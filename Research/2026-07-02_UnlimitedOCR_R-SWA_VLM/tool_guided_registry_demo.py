"""
Tool-Guided VLM의 image registry 개념을 흉내 내는 작은 데모.

실제 논문은 VLM이 crop, line drawing, channel isolation 같은 도구를 호출하고
새 이미지 리소스를 registry에 추가해가며 추론한다.

여기서는 외부 이미지 라이브러리 없이 2D 숫자 배열을 "이미지"로 보고,
도구 호출 결과를 immutable resource로 저장한다.

실행:
    python Research\\2026-07-02_UnlimitedOCR_R-SWA_VLM\\tool_guided_registry_demo.py
"""

from __future__ import annotations

from dataclasses import dataclass


Image = list[list[int]]


@dataclass(frozen=True)
class Resource:
    """registry에 저장되는 불변 이미지 리소스."""

    resource_id: str
    description: str
    image: Image


class ImageRegistry:
    """도구 호출 결과를 순서대로 저장하는 간단한 registry."""

    def __init__(self) -> None:
        self._resources: list[Resource] = []

    def add(self, description: str, image: Image) -> Resource:
        """새 이미지 리소스를 추가한다.

        실제 Tool-Guided VLM에서는 도구가 만든 crop/annotation 결과를
        다시 참조할 수 있도록 registry에 쌓아둔다.
        """

        resource = Resource(f"img_{len(self._resources):03d}", description, image)
        self._resources.append(resource)
        return resource

    def list(self) -> list[Resource]:
        return list(self._resources)


def crop(image: Image, top: int, left: int, height: int, width: int) -> Image:
    """관심 영역만 잘라낸다."""

    return [row[left : left + width] for row in image[top : top + height]]


def threshold(image: Image, cutoff: int) -> Image:
    """밝기 값을 이진화한다.

    미세한 대비를 확인하거나 배경을 제거하는 도구를 단순화한 것이다.
    """

    return [[255 if value >= cutoff else 0 for value in row] for row in image]


def draw_vertical_line(image: Image, x: int, value: int = 180) -> Image:
    """세로 가이드 라인을 그린다.

    착시/정렬 문제에서는 기준선을 긋는 것만으로도 판단이 쉬워진다.
    """

    copied = [row[:] for row in image]
    for row in copied:
        if 0 <= x < len(row):
            row[x] = value
    return copied


def render_ascii(image: Image) -> str:
    """숫자 이미지를 콘솔에서 보기 쉬운 문자 이미지로 바꾼다."""

    def pixel(value: int) -> str:
        if value >= 220:
            return "#"
        if value >= 120:
            return "|"
        if value >= 40:
            return "."
        return " "

    return "\n".join("".join(pixel(value) for value in row) for row in image)


def main() -> None:
    """도구 호출이 registry에 증거를 쌓는 흐름을 보여준다."""

    original = [
        [10, 10, 10, 10, 10, 10, 10, 10],
        [10, 50, 50, 50, 10, 10, 10, 10],
        [10, 50, 240, 50, 10, 200, 200, 10],
        [10, 50, 50, 50, 10, 200, 200, 10],
        [10, 10, 10, 10, 10, 10, 10, 10],
        [10, 10, 120, 120, 120, 120, 10, 10],
        [10, 10, 120, 240, 240, 120, 10, 10],
        [10, 10, 120, 120, 120, 120, 10, 10],
    ]

    registry = ImageRegistry()
    base = registry.add("원본 이미지", original)
    roi = registry.add("좌상단 관심 영역 crop", crop(base.image, top=1, left=1, height=3, width=3))
    binary = registry.add("crop 결과를 threshold=200으로 이진화", threshold(roi.image, cutoff=200))
    annotated = registry.add("원본에 세로 기준선 x=4 추가", draw_vertical_line(base.image, x=4))

    for resource in registry.list():
        print(f"{resource.resource_id}: {resource.description}")
        print(render_ascii(resource.image))
        print()

    print("해석:")
    print("- 원본만 보면 여러 밝기 패턴이 섞여 있다.")
    print("- crop과 threshold를 거치면 관심 영역 안의 강한 신호가 분리된다.")
    print("- 기준선을 추가한 이미지는 좌우 관계를 다시 판단하는 증거가 된다.")


if __name__ == "__main__":
    main()
