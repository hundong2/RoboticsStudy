"""
HOG(Histograms of Oriented Gradients) from scratch.

Dalal & Triggs 2005 논문의 핵심 아이디어를 외부 패키지 없이 재현하는
학습용 코드다. 실제 OpenCV 구현처럼 보간, padding, multi-scale detection을
완전히 구현하지는 않는다. 대신 HOG의 뼈대인 다음 단계를 명확히 보여준다.

1. gradient gx, gy 계산
2. gradient magnitude / orientation 계산
3. cell 단위 orientation histogram 생성
4. block 단위 L2-Hys normalization
5. 전체 descriptor 생성

실행:
    python Research\\이론\\HOG\\hog_from_scratch.py
"""

from __future__ import annotations

from math import atan2, degrees, sqrt


Image = list[list[float]]


def make_test_image(size: int = 16) -> Image:
    """간단한 16x16 테스트 이미지를 만든다.

    왼쪽은 어둡고 오른쪽은 밝게 만들어 세로 edge가 생기도록 한다.
    HOG는 이 edge에서 강한 x 방향 gradient를 관찰하게 된다.
    """

    image: Image = []
    for y in range(size):
        row = []
        for x in range(size):
            if x < size // 2:
                row.append(20.0)
            else:
                row.append(220.0)
        image.append(row)
    return image


def compute_gradients(image: Image) -> tuple[Image, Image]:
    """중심 차분으로 gx, gy를 계산한다.

    가장자리 픽셀은 단순화를 위해 가장 가까운 내부 픽셀 값을 재사용한다.
    """

    height = len(image)
    width = len(image[0])
    gx = [[0.0 for _ in range(width)] for _ in range(height)]
    gy = [[0.0 for _ in range(width)] for _ in range(height)]

    for y in range(height):
        for x in range(width):
            left = image[y][max(x - 1, 0)]
            right = image[y][min(x + 1, width - 1)]
            up = image[max(y - 1, 0)][x]
            down = image[min(y + 1, height - 1)][x]
            gx[y][x] = right - left
            gy[y][x] = down - up

    return gx, gy


def magnitude_and_orientation(gx: Image, gy: Image) -> tuple[Image, Image]:
    """gradient magnitude와 unsigned orientation을 계산한다.

    HOG에서는 보통 0도와 180도를 같은 edge 방향으로 본다.
    그래서 angle을 0 이상 180 미만 범위로 접는다.
    """

    height = len(gx)
    width = len(gx[0])
    magnitude = [[0.0 for _ in range(width)] for _ in range(height)]
    orientation = [[0.0 for _ in range(width)] for _ in range(height)]

    for y in range(height):
        for x in range(width):
            dx = gx[y][x]
            dy = gy[y][x]
            magnitude[y][x] = sqrt(dx * dx + dy * dy)
            angle = degrees(atan2(dy, dx)) % 180.0
            orientation[y][x] = angle

    return magnitude, orientation


def build_cell_histograms(
    magnitude: Image,
    orientation: Image,
    cell_size: int = 8,
    bins: int = 9,
) -> list[list[list[float]]]:
    """cell마다 orientation histogram을 만든다.

    단순화를 위해 한 픽셀의 vote를 가장 가까운 bin 하나에만 넣는다.
    실제 HOG 구현은 orientation bin과 공간 위치에 보간을 적용할 수 있다.
    """

    height = len(magnitude)
    width = len(magnitude[0])
    cells_y = height // cell_size
    cells_x = width // cell_size
    bin_width = 180.0 / bins

    histograms = [
        [[0.0 for _ in range(bins)] for _ in range(cells_x)]
        for _ in range(cells_y)
    ]

    for cy in range(cells_y):
        for cx in range(cells_x):
            for yy in range(cell_size):
                for xx in range(cell_size):
                    y = cy * cell_size + yy
                    x = cx * cell_size + xx
                    angle = orientation[y][x]
                    mag = magnitude[y][x]
                    bin_index = int(angle // bin_width) % bins
                    histograms[cy][cx][bin_index] += mag

    return histograms


def normalize_l2_hys(vector: list[float], epsilon: float = 1e-6, clip: float = 0.2) -> list[float]:
    """Dalal-Triggs에서 널리 쓰인 L2-Hys normalization의 단순 버전.

    1. L2 normalize
    2. 큰 값을 clip
    3. 다시 L2 normalize
    """

    norm = sqrt(sum(v * v for v in vector) + epsilon * epsilon)
    normalized = [v / norm for v in vector]
    clipped = [min(v, clip) for v in normalized]
    clipped_norm = sqrt(sum(v * v for v in clipped) + epsilon * epsilon)
    return [v / clipped_norm for v in clipped]


def build_hog_descriptor(
    cell_histograms: list[list[list[float]]],
    block_size: int = 2,
) -> list[float]:
    """cell histogram들을 block 단위로 묶고 정규화해 descriptor를 만든다.

    block_size=2이면 2x2 cell의 histogram을 이어 붙여 하나의 block feature를
    만들고, 이 block을 한 cell 간격으로 이동한다.
    """

    cells_y = len(cell_histograms)
    cells_x = len(cell_histograms[0])
    descriptor: list[float] = []

    for by in range(cells_y - block_size + 1):
        for bx in range(cells_x - block_size + 1):
            block_vector: list[float] = []
            for dy in range(block_size):
                for dx in range(block_size):
                    block_vector.extend(cell_histograms[by + dy][bx + dx])
            descriptor.extend(normalize_l2_hys(block_vector))

    return descriptor


def print_cell_histograms(histograms: list[list[list[float]]]) -> None:
    """cell histogram을 사람이 보기 좋게 출력한다."""

    for cy, row in enumerate(histograms):
        for cx, hist in enumerate(row):
            compact = ", ".join(f"{value:.0f}" for value in hist)
            print(f"cell({cy}, {cx}) bins: [{compact}]")


def main() -> None:
    image = make_test_image(size=16)
    gx, gy = compute_gradients(image)
    magnitude, orientation = magnitude_and_orientation(gx, gy)
    cell_histograms = build_cell_histograms(magnitude, orientation, cell_size=8, bins=9)
    descriptor = build_hog_descriptor(cell_histograms, block_size=2)

    print("HOG from scratch demo")
    print(f"image size        : {len(image[0])}x{len(image)}")
    print("cell size         : 8x8")
    print("orientation bins  : 9")
    print("block size        : 2x2 cells")
    print(f"descriptor length : {len(descriptor)}")
    print()

    print("Cell histograms")
    print_cell_histograms(cell_histograms)
    print()

    print("First 12 descriptor values")
    print([round(v, 4) for v in descriptor[:12]])


if __name__ == "__main__":
    main()
