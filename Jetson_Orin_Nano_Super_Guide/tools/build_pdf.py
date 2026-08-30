from __future__ import annotations

import html
import re
from pathlib import Path

from reportlab.lib import colors
from reportlab.lib.enums import TA_CENTER, TA_LEFT, TA_RIGHT
from reportlab.lib.pagesizes import letter
from reportlab.lib.styles import ParagraphStyle, getSampleStyleSheet
from reportlab.lib.units import inch
from reportlab.pdfbase import pdfmetrics
from reportlab.pdfbase.ttfonts import TTFont
from reportlab.platypus import (
    BaseDocTemplate,
    Frame,
    KeepTogether,
    ListFlowable,
    ListItem,
    PageBreak,
    PageTemplate,
    Paragraph,
    Spacer,
    CondPageBreak,
    Table,
    TableStyle,
)


ROOT = Path(__file__).resolve().parents[1]
OUTPUT = ROOT / "output" / "Jetson_Orin_Nano_Super_Vision_VLA_가이드북.pdf"
CHAPTERS = [
    ROOT / "docs" / "01_하드웨어와_전원.md",
    ROOT / "docs" / "02_핀과_커넥터.md",
    ROOT / "docs" / "03_JetPack_BSP_설치.md",
    ROOT / "docs" / "04_커스텀_BSP_브링업.md",
    ROOT / "docs" / "05_카메라와_Vision.md",
    ROOT / "docs" / "06_AI_가속과_최적화.md",
    ROOT / "docs" / "07_ROS2_Isaac_ROS.md",
    ROOT / "docs" / "08_Vision_VLA_로드맵.md",
    ROOT / "docs" / "09_운영_안전_문제해결.md",
    ROOT / "references" / "공식문서_목록.md",
    ROOT / "references" / "커버리지_매트릭스.md",
]

NAVY = colors.HexColor("#0B2545")
BLUE = colors.HexColor("#2E74B5")
DARK_BLUE = colors.HexColor("#1F4D78")
MUTED = colors.HexColor("#666D75")
LIGHT_BLUE = colors.HexColor("#E8EEF5")
LIGHT_GRAY = colors.HexColor("#F2F4F7")
GRID = colors.HexColor("#CCD5DF")


def register_fonts():
    regular = Path(r"C:\Windows\Fonts\malgun.ttf")
    bold = Path(r"C:\Windows\Fonts\malgunbd.ttf")
    if not regular.exists():
        raise FileNotFoundError("Malgun Gothic font not found")
    pdfmetrics.registerFont(TTFont("Malgun", str(regular)))
    pdfmetrics.registerFont(TTFont("Malgun-Bold", str(bold if bold.exists() else regular)))
    pdfmetrics.registerFontFamily("Malgun", normal="Malgun", bold="Malgun-Bold")


class GuideDocTemplate(BaseDocTemplate):
    def __init__(self, filename, **kwargs):
        super().__init__(filename, **kwargs)
        frame = Frame(
            self.leftMargin,
            self.bottomMargin,
            self.width,
            self.height,
            leftPadding=0,
            rightPadding=0,
            topPadding=0,
            bottomPadding=0,
            id="normal",
        )
        self.addPageTemplates(PageTemplate(id="guide", frames=[frame], onPage=self.draw_page))

    def draw_page(self, canvas, doc):
        canvas.saveState()
        canvas.setFont("Malgun-Bold", 7.5)
        canvas.setFillColor(MUTED)
        canvas.drawString(doc.leftMargin, letter[1] - 0.48 * inch, "JETSON ORIN NANO SUPER  |  VISION VLA FIELD GUIDE")
        canvas.setFont("Malgun", 8)
        canvas.drawRightString(letter[0] - doc.rightMargin, 0.47 * inch, f"Page {doc.page}")
        canvas.restoreState()


def styles():
    return {
        "body": ParagraphStyle(
            "Body",
            fontName="Malgun",
            fontSize=9.2,
            leading=13.0,
            textColor=colors.black,
            spaceAfter=6,
            wordWrap="CJK",
        ),
        "h1": ParagraphStyle(
            "H1", fontName="Malgun-Bold", fontSize=16, leading=21,
            textColor=BLUE, spaceBefore=15, spaceAfter=9, keepWithNext=True, wordWrap="CJK"
        ),
        "h2": ParagraphStyle(
            "H2", fontName="Malgun-Bold", fontSize=12.5, leading=17,
            textColor=BLUE, spaceBefore=12, spaceAfter=6, keepWithNext=True, wordWrap="CJK"
        ),
        "h3": ParagraphStyle(
            "H3", fontName="Malgun-Bold", fontSize=10.5, leading=15,
            textColor=DARK_BLUE, spaceBefore=9, spaceAfter=4, keepWithNext=True, wordWrap="CJK"
        ),
        "code": ParagraphStyle(
            "Code", fontName="Malgun", fontSize=7.7, leading=10.5,
            textColor=NAVY, leftIndent=8, rightIndent=5, borderPadding=7,
            backColor=LIGHT_GRAY, spaceBefore=4, spaceAfter=7, wordWrap="CJK"
        ),
        "table": ParagraphStyle(
            "Table", fontName="Malgun", fontSize=7.4, leading=9.8,
            textColor=colors.black, wordWrap="CJK"
        ),
        "table_head": ParagraphStyle(
            "TableHead", fontName="Malgun-Bold", fontSize=7.5, leading=10,
            textColor=NAVY, wordWrap="CJK"
        ),
        "small": ParagraphStyle(
            "Small", fontName="Malgun", fontSize=8, leading=11, textColor=MUTED, wordWrap="CJK"
        ),
    }


LINK_RE = re.compile(r"\[([^\]]+)\]\((https?://[^)]+)\)")
CODE_RE = re.compile(r"`([^`]+)`")


def inline(text):
    escaped = html.escape(text)
    escaped = LINK_RE.sub(lambda m: f'<link href="{m.group(2)}" color="#2E74B5"><u>{m.group(1)}</u></link>', escaped)
    escaped = CODE_RE.sub(lambda m: f'<font color="#1F4D78">{m.group(1)}</font>', escaped)
    return escaped


def parse_table(lines, start):
    rows = []
    idx = start
    while idx < len(lines) and lines[idx].strip().startswith("|"):
        rows.append([c.strip() for c in lines[idx].strip().strip("|").split("|")])
        idx += 1
    if len(rows) >= 2 and all(re.fullmatch(r":?-{3,}:?", c.replace(" ", "")) for c in rows[1]):
        return [rows[0]] + rows[2:], idx
    return None, start


def widths_for(headers, max_width):
    n = len(headers)
    if n == 2:
        return [max_width * 0.24, max_width * 0.76]
    if n == 4 and headers[0].lower() in {"핀", "pin"}:
        return [max_width * 0.10, max_width * 0.40, max_width * 0.25, max_width * 0.25]
    if n == 5 and headers[0].lower() in {"핀", "pin"}:
        return [max_width * 0.08, max_width * 0.22, max_width * 0.12, max_width * 0.30, max_width * 0.28]
    if n == 5:
        return [max_width * 0.16, max_width * 0.22, max_width * 0.24, max_width * 0.11, max_width * 0.27]
    return [max_width / n] * n


def make_table(rows, st, max_width):
    n = max(len(r) for r in rows)
    rows = [r + [""] * (n - len(r)) for r in rows]
    data = []
    for r_idx, row in enumerate(rows):
        style = st["table_head"] if r_idx == 0 else st["table"]
        data.append([Paragraph(inline(cell), style) for cell in row])
    table = Table(data, colWidths=widths_for(rows[0], max_width), repeatRows=1, hAlign="LEFT")
    table.setStyle(TableStyle([
        ("BACKGROUND", (0, 0), (-1, 0), LIGHT_BLUE),
        ("TEXTCOLOR", (0, 0), (-1, 0), NAVY),
        ("GRID", (0, 0), (-1, -1), 0.35, GRID),
        ("VALIGN", (0, 0), (-1, -1), "MIDDLE"),
        ("LEFTPADDING", (0, 0), (-1, -1), 5),
        ("RIGHTPADDING", (0, 0), (-1, -1), 5),
        ("TOPPADDING", (0, 0), (-1, -1), 4),
        ("BOTTOMPADDING", (0, 0), (-1, -1), 4),
    ]))
    return table


def markdown_story(path, st, max_width):
    lines = path.read_text(encoding="utf-8").splitlines()
    story = []
    idx = 0
    in_code = False
    code = []
    para = []

    def flush():
        nonlocal para
        if para:
            story.append(Paragraph(inline(" ".join(x.strip() for x in para)), st["body"]))
            para = []

    while idx < len(lines):
        line = lines[idx]
        stripped = line.strip()
        if stripped.startswith("```"):
            flush()
            if in_code:
                rendered_lines = []
                for raw_line in code:
                    rendered = html.escape(raw_line).replace(" ", "&nbsp;")
                    rendered = rendered.replace("\\", '<font name="Courier">\\</font>')
                    rendered_lines.append(rendered)
                content = "<br/>".join(rendered_lines)
                story.append(Paragraph(content, st["code"]))
                code = []
                in_code = False
            else:
                in_code = True
            idx += 1
            continue
        if in_code:
            code.append(line)
            idx += 1
            continue
        if not stripped:
            flush()
            idx += 1
            continue
        if stripped.startswith("|"):
            flush()
            rows, next_idx = parse_table(lines, idx)
            if rows:
                story.extend([make_table(rows, st, max_width), Spacer(1, 6)])
                idx = next_idx
                continue
        heading = re.match(r"^(#{1,4})\s+(.+)$", stripped)
        if heading:
            flush()
            level = min(len(heading.group(1)), 3)
            # Reserve enough room for the heading and at least the opening
            # lines of its section, avoiding orphaned headings at page foot.
            story.append(CondPageBreak(54))
            story.append(Paragraph(inline(heading.group(2)), st[f"h{level}"]))
            idx += 1
            continue
        bullet = re.match(r"^-\s+(.+)$", stripped)
        if bullet:
            flush()
            items = []
            while idx < len(lines):
                match = re.match(r"^-\s+(.+)$", lines[idx].strip())
                if not match:
                    break
                items.append(ListItem(Paragraph(inline(match.group(1)), st["body"]), leftIndent=14))
                idx += 1
            story.append(ListFlowable(items, bulletType="bullet", start="circle", leftIndent=18, bulletFontName="Malgun", bulletFontSize=7, spaceAfter=3))
            continue
        numbered = re.match(r"^\d+\.\s+(.+)$", stripped)
        if numbered:
            flush()
            items = []
            while idx < len(lines):
                match = re.match(r"^\d+\.\s+(.+)$", lines[idx].strip())
                if not match:
                    break
                items.append(ListItem(Paragraph(inline(match.group(1)), st["body"]), leftIndent=18))
                idx += 1
            story.append(ListFlowable(items, bulletType="1", start="1", leftIndent=22, bulletFontName="Malgun", bulletFontSize=8, spaceAfter=3))
            continue
        para.append(stripped)
        idx += 1
    flush()
    return story


def cover(st):
    title = ParagraphStyle("CoverTitle", parent=st["h1"], fontSize=27, leading=35, textColor=NAVY, alignment=TA_CENTER, spaceAfter=12)
    kicker = ParagraphStyle("CoverKicker", parent=st["small"], fontName="Malgun-Bold", fontSize=11, textColor=BLUE, alignment=TA_CENTER, spaceAfter=14)
    subtitle = ParagraphStyle("CoverSubtitle", parent=st["body"], fontSize=12, leading=18, textColor=DARK_BLUE, alignment=TA_CENTER, spaceAfter=30)
    meta = ParagraphStyle("CoverMeta", parent=st["small"], alignment=TA_CENTER, spaceAfter=5)
    return [
        Spacer(1, 1.45 * inch),
        Paragraph("JETSON ORIN NANO SUPER", kicker),
        Paragraph("임베디드 Vision VLA<br/>개발 가이드북", title),
        Paragraph("하드웨어 · 핀 · JetPack/BSP · 카메라 · AI 가속 · ROS 2 · 안전한 행동 배포", subtitle),
        Spacer(1, 0.35 * inch),
        Paragraph("기준: JetPack 7.2.1 / Jetson Linux 39.2.1 / ROS 2 Jazzy", meta),
        Paragraph("문서 기준일 2026-08-29", meta),
        PageBreak(),
        Paragraph("사용 안내", st["h1"]),
        Paragraph("이 책은 공식 NVIDIA/ROS 문서를 한국어로 재서술한 실무 해설서다. 전기 절대 정격, 플래시 명령, 보안 키 작업은 설치 릴리스와 동일한 공식 원문을 최종 기준으로 확인한다.", st["body"]),
        Paragraph("권장 읽기 경로", st["h2"]),
        ListFlowable([
            ListItem(Paragraph("처음 설치: 1장 → 2장 → 3장 → 5장 → 6장", st["body"])),
            ListItem(Paragraph("커스텀 캐리어보드: 2장 → 4장 → 9장", st["body"])),
            ListItem(Paragraph("Vision VLA: 5장 → 6장 → 7장 → 8장 → 9장", st["body"])),
        ], bulletType="bullet", leftIndent=18, bulletFontName="Malgun"),
        Paragraph("목차", st["h2"]),
        ListFlowable([
            ListItem(Paragraph(path.read_text(encoding="utf-8").splitlines()[0].lstrip("# "), st["body"]))
            for path in CHAPTERS
        ], bulletType="1", leftIndent=22, bulletFontName="Malgun"),
    ]


def build():
    register_fonts()
    OUTPUT.parent.mkdir(parents=True, exist_ok=True)
    st = styles()
    doc = GuideDocTemplate(
        str(OUTPUT),
        pagesize=letter,
        leftMargin=0.78 * inch,
        rightMargin=0.78 * inch,
        topMargin=0.78 * inch,
        bottomMargin=0.72 * inch,
        title="Jetson Orin Nano Super 임베디드 Vision VLA 개발 가이드북",
        author="RoboticsStudy",
        subject="핀, BSP, Vision, VLA, ROS 2, Isaac ROS",
    )
    story = cover(st)
    for path in CHAPTERS:
        # Keep the coverage matrix adjacent to the source index so the
        # appendix does not end with a nearly empty spill page.
        if path.name != "커버리지_매트릭스.md":
            story.append(PageBreak())
        story.extend(markdown_story(path, st, doc.width))
    doc.build(story)
    print(OUTPUT)


if __name__ == "__main__":
    build()
