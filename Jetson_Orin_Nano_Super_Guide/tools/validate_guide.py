from pathlib import Path
import re
import sys


ROOT = Path(__file__).resolve().parents[1]

required_files = [
    ROOT / "README.md",
    *(ROOT / "docs" / f for f in [
        "01_하드웨어와_전원.md",
        "02_핀과_커넥터.md",
        "03_JetPack_BSP_설치.md",
        "04_커스텀_BSP_브링업.md",
        "05_카메라와_Vision.md",
        "06_AI_가속과_최적화.md",
        "07_ROS2_Isaac_ROS.md",
        "08_Vision_VLA_로드맵.md",
        "09_운영_안전_문제해결.md",
    ]),
    ROOT / "references" / "공식문서_목록.md",
    ROOT / "references" / "커버리지_매트릭스.md",
    ROOT / "references" / "original" / "Jetson_Orin_Nano_DevKit_Carrier_Board_Specification.pdf",
    ROOT / "references" / "original" / "Jetson_Linux_Release_Notes_r39.2.1.pdf",
    ROOT / "translations" / "Jetson_Orin_Nano_DevKit_Carrier_Board_Spec_v1.3_KO.md",
    ROOT / "output" / "Jetson_Orin_Nano_Carrier_Board_Spec_v1.3_KO.docx",
    ROOT / "output" / "Jetson_Orin_Nano_Carrier_Board_Spec_v1.3_KO.pdf",
    ROOT / "interfaces_practice" / "README.md",
    ROOT / "interfaces_practice" / "CMakeLists.txt",
    *(ROOT / "interfaces_practice" / "docs" / f for f in [
        "00_공통_기초와_안전.md",
        "01_I2C.md",
        "02_UART.md",
        "03_SPI.md",
        "04_GPIO_PWM.md",
        "05_CAN.md",
        "06_I2S와_기타.md",
        "07_문제해결.md",
    ]),
    *(ROOT / "interfaces_practice" / "src" / f for f in [
        "i2c_read.cpp",
        "uart_loopback.c",
        "spi_loopback.c",
        "gpio_v2_toggle.c",
        "can_socketcan.cpp",
        "pwm_sysfs.cpp",
    ]),
]

errors = []
for path in required_files:
    if not path.exists() or path.stat().st_size == 0:
        errors.append(f"missing/empty: {path}")

all_text = "\n".join(
    path.read_text(encoding="utf-8")
    for path in required_files
    if path.suffix == ".md" and path.exists()
)

for token in (
    "JetPack 7.2.1",
    "Jetson Linux 39.2.1",
    "J12",
    "J14",
    "J20",
    "J21",
    "l4t_initrd_flash.sh",
    "TensorRT",
    "ROS 2 Jazzy",
    "safety supervisor",
):
    if token not in all_text:
        errors.append(f"required coverage token missing: {token}")

pin_doc = (ROOT / "docs" / "02_핀과_커넥터.md").read_text(encoding="utf-8")
for pin in range(1, 41):
    if not re.search(rf"\|\s*{pin}\s*\|", pin_doc):
        errors.append(f"J12 pin {pin} not found")

translation = (ROOT / "translations" / "Jetson_Orin_Nano_DevKit_Carrier_Board_Spec_v1.3_KO.md").read_text(encoding="utf-8")
for token in (
    "SP-11324-001_v1.3",
    "Orin NX 40W(MAXN_SUPER)",
    "Table 2-1",
    "Table 2-8",
    "Table 3-1",
    "Table 3-5~3-10",
    "Table 5-1",
    "Table 5-3",
    "Figure 1-1~1-5",
    "Figure 5-1",
    "NVIDIA 고지",
):
    if token not in translation:
        errors.append(f"carrier translation token missing: {token}")
for pin in range(1, 41):
    if not re.search(rf"\|\s*{pin}\s*\|", translation):
        errors.append(f"translated J12 pin {pin} not found")

practice_text = "\n".join(
    path.read_text(encoding="utf-8")
    for path in (ROOT / "interfaces_practice").rglob("*")
    if path.suffix in {".md", ".c", ".cpp"}
)
for token in (
    "I2C_RDWR",
    "termios",
    "SPI_IOC_MESSAGE",
    "GPIO_V2_GET_LINE_IOCTL",
    "SocketCAN",
    "/sys/class/pwm",
    "I2S0_SCLK",
    "3.3V",
):
    if token not in practice_text:
        errors.append(f"interface practice token missing: {token}")

if errors:
    print("VALIDATION FAILED")
    print("\n".join(errors))
    sys.exit(1)

print(f"VALIDATION OK: {len(required_files)} required files; J12 pins 1-40 and interface practice coverage verified")

