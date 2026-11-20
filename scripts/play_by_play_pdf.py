#!/usr/bin/env python3
"""Write a play-by-play PDF from the text a game prints.

This is the play-by-play PDF exporter. ``write_pdf`` is the layout every
later export uses. The engine's lines are not rewritten: a turn is set so
whose turn it is, the score, the runes, the moves, the plays (what was
paid), the combat, and a conquer or a hold can be told apart at a glance.

The text format is the one ``PlayByPlay::text`` prints. Startup lines, a
batch progress line, and a results table are skipped. A line that names a
card also shows that card's picture, taken from ``image_url`` on the card
class: the card played and the card it was played on, the unit that moved,
the battlefield conquered or held, the units in a combat, and the cards
named in the mulligan. A picture that cannot be fetched is left out. The
words of the line stay.

Requires ReportLab (``pip install reportlab``), which is not an engine
dependency. ``--self-check`` also needs PyMuPDF. Card pictures are fetched
when the PDF is written.

    python3 scripts/play_by_play_pdf.py --output game.pdf log.txt
    ./build/riftbound ... --web off --render-html off \\
        | python3 scripts/play_by_play_pdf.py --output game.pdf
"""

from __future__ import annotations

import argparse
import hashlib
import io
import re
import sys
import tempfile
import urllib.request
from dataclasses import dataclass, field
from pathlib import Path

from reportlab.lib.colors import Color, HexColor, white
from reportlab.lib.enums import TA_LEFT, TA_RIGHT
from reportlab.lib.pagesizes import letter
from reportlab.lib.styles import ParagraphStyle
from reportlab.pdfbase import pdfmetrics
from reportlab.pdfbase.ttfonts import TTFont
from reportlab.platypus import (
    BaseDocTemplate,
    CondPageBreak,
    Frame,
    HRFlowable,
    Image,
    PageTemplate,
    Paragraph,
    Spacer,
    Table,
    TableStyle,
)

PAGE_W, PAGE_H = letter
LEFT = 40
RIGHT = 40
TOP = 48
BOTTOM = 36
CONTENT_W = PAGE_W - LEFT - RIGHT
OUTER_X = 8
INNER_W = CONTENT_W - 2 * OUTER_X
SCORE_W = 108
LABEL_W = 58

INK = HexColor("#1f1d1a")
MUTED = HexColor("#5c574e")
RULE = HexColor("#d9d3c7")
BORDER = HexColor("#cfc6b8")
HEADER_BG = HexColor("#f6f3ec")
SCORE_BG = HexColor("#e7f0e6")
SCORE_INK = HexColor("#1a3a2a")
COMBAT_BG = HexColor("#f8f1e6")
COMBAT_EDGE = HexColor("#b08968")
P1_EDGE = HexColor("#3d4f3a")
P2_EDGE = HexColor("#1e3a5f")
MULLIGAN_BG = HexColor("#f7f6f3")

GAME_RE = re.compile(r"^Game \d+ \[seed=")
TURN_RE = re.compile(r"^Turn \d+ - \S+")
PLAYER_RE = re.compile(r"^Turn \d+ - (\S+)")

FONT = "Pbp"
FONT_BOLD = "Pbp-Bold"


def _register_fonts() -> None:
    global FONT, FONT_BOLD
    if FONT in pdfmetrics.getRegisteredFontNames():
        return
    regular = _find_font("DejaVuSans.ttf")
    bold = _find_font("DejaVuSans-Bold.ttf")
    if regular and bold:
        pdfmetrics.registerFont(TTFont(FONT, regular))
        pdfmetrics.registerFont(TTFont(FONT_BOLD, bold))
        return
    # Built-in faces. They cover the engine's ASCII lines.
    FONT = "Times-Roman"
    FONT_BOLD = "Times-Bold"


def _find_font(name: str) -> str | None:
    roots = (
        Path("/usr/share/fonts/truetype/dejavu"),
        Path("/usr/share/fonts/dejavu"),
        Path("/usr/share/fonts/TTF"),
    )
    for root in roots:
        path = root / name
        if path.is_file():
            return str(path)
    return None


def _xml(text: str) -> str:
    return text.replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;")


# How many pictures a kind of line can carry, and how wide each one is.
# A play shows the card that was played and what it was played on. A combat
# shows who fought. The score and the rune counts name no card.
_ART_LIMIT = {
    "play": 3,
    "move": 2,
    "result": 1,
    "combat": 8,
    "event": 2,
    "mulligan": 8,
    "outcome": 2,
}
_THUMB_W = {
    "play": 32,
    "move": 28,
    "result": 28,
    "combat": 24,
    "event": 28,
    "mulligan": 22,
    "outcome": 28,
}
_CARD_RATIO = 1039 / 744
_NAME_RE = re.compile(r'\bd\.name\s*=\s*R"RB\((.*?)\)RB"', re.S)
_URL_RE = re.compile(r'\bd\.image_url\s*=\s*R"RB\((.*?)\)RB"', re.S)


class ArtCatalog:
    """Card name to a picture source. A source is an image URL or a local file.

    Names are matched longest-first, on letter boundaries, so "Fiora, Worthy"
    is not also read as a shorter name inside it.
    """

    def __init__(self, entries: list[tuple[str, str]]):
        self._entries = sorted(
            ((name, source) for name, source in entries if name and source),
            key=lambda item: len(item[0]),
            reverse=True,
        )
        self._files: dict[str, Path | None] = {}
        self.missing: list[str] = []
        self._cache = Path(tempfile.gettempdir()) / "riftbound-card-art"
        self._cache.mkdir(parents=True, exist_ok=True)

    def __bool__(self) -> bool:
        return bool(self._entries)

    def find(self, line: str) -> list[str]:
        if not line or not self._entries:
            return []
        occupied = bytearray(len(line))
        hits: list[tuple[int, str]] = []
        for name, source in self._entries:
            start = 0
            while True:
                at = line.find(name, start)
                if at < 0:
                    break
                end = at + len(name)
                before_ok = at == 0 or not line[at - 1].isalnum()
                after_ok = end == len(line) or not line[end].isalnum()
                if before_ok and after_ok and not any(occupied[at:end]):
                    hits.append((at, source))
                    for index in range(at, end):
                        occupied[index] = 1
                start = at + 1
        hits.sort(key=lambda item: item[0])
        return [source for _at, source in hits]

    def file_for(self, source: str) -> Path | None:
        if source in self._files:
            return self._files[source]
        path = self._resolve(source)
        self._files[source] = path
        if path is None:
            self.missing.append(source)
        return path

    def _resolve(self, source: str) -> Path | None:
        local = Path(source)
        if local.is_file():
            return local
        if not source.startswith(("http://", "https://")):
            return None
        digest = hashlib.sha256(source.encode()).hexdigest()[:24]
        dest = self._cache / f"{digest}.png"
        if dest.is_file() and dest.stat().st_size > 0:
            return dest
        try:
            request = urllib.request.Request(
                source, headers={"User-Agent": "alpharune-play-by-play"}
            )
            with urllib.request.urlopen(request, timeout=20) as response:
                data = response.read()
            from PIL import Image as PilImage

            image = PilImage.open(io.BytesIO(data)).convert("RGB")
            image.thumbnail((160, 224), PilImage.Resampling.LANCZOS)
            image.save(dest, "PNG")
            return dest
        except (OSError, ValueError):
            return None


def load_card_art(root: Path) -> ArtCatalog:
    """Pictures declared on the card classes. The first printing of a name wins."""
    cards = root / "src" / "cards"
    found: dict[str, str] = {}
    if cards.is_dir():
        for path in sorted(cards.rglob("*.cpp")):
            text = path.read_text(encoding="utf-8", errors="replace")
            for name, url in zip(_NAME_RE.findall(text), _URL_RE.findall(text)):
                name = name.strip()
                url = url.strip()
                if name and url and name not in found:
                    found[name] = url
    return ArtCatalog(list(found.items()))


def repo_root() -> Path:
    here = Path(__file__).resolve()
    for parent in here.parents:
        if (parent / "src" / "cards").is_dir():
            return parent
    return here.parents[1]


@dataclass
class Event:
    kind: str
    text: str


@dataclass
class Turn:
    heading: str
    score: str = ""
    runes_start: str = ""
    runes_end: str = ""
    events: list[Event] = field(default_factory=list)


@dataclass
class Game:
    header: str
    outcome: str = ""
    mulligan: str = ""
    extra: list[str] = field(default_factory=list)
    turns: list[Turn] = field(default_factory=list)


def classify(line: str) -> str:
    """What a body line is, from the verb the engine already printed."""
    if line.startswith("Combat at "):
        return "combat"
    parts = line.split(" ", 2)
    verb = parts[1] if len(parts) > 1 else ""
    if verb == "plays":
        return "play"
    if verb == "moves":
        return "move"
    if verb in ("conquers", "holds"):
        return "result"
    if verb == "Predicts" or verb.startswith("Predicts"):
        return "predict"
    return "event"


def _cost_markup(line: str) -> str:
    """The play sentence, with the cost clause the engine wrote set apart."""
    start = None
    for marker in (", ignoring", " for "):
        at = line.rfind(marker)
        if at != -1 and (start is None or at < start):
            start = at
    if start is None:
        return _xml(line)
    return (
        _xml(line[:start])
        + f'<font color="#1e3a5f"><b>{_xml(line[start:])}</b></font>'
    )


def _split_combat(line: str) -> tuple[str, str | None]:
    """Who fought, then who took damage. The words stay in order."""
    head, sep, tail = line.partition(". ")
    if not sep:
        return line, None
    return head + ".", tail


def parse_log(text: str) -> list[Game]:
    """Pull each play-by-play block out of an engine log."""
    lines = text.splitlines()
    games: list[Game] = []
    index = 0
    while index < len(lines):
        if GAME_RE.match(lines[index].strip()) is None:
            index += 1
            continue
        header = lines[index].strip()
        index += 1
        game = Game(header=header)
        while index < len(lines):
            raw = lines[index]
            stripped = raw.strip()
            if GAME_RE.match(stripped) is not None and (
                game.turns or game.outcome or game.mulligan or game.extra
            ):
                break
            if not stripped:
                index += 1
                continue
            if TURN_RE.match(stripped):
                turn, index = _parse_turn(lines, index)
                game.turns.append(turn)
                continue
            if raw.startswith((" ", "\t")):
                game.extra.append(stripped)
                index += 1
                continue
            if stripped.startswith("Ended by "):
                game.outcome = stripped
                index += 1
                continue
            if stripped.startswith("Mulligan:"):
                game.mulligan = stripped
                index += 1
                continue
            if game.turns or game.outcome or game.mulligan:
                break
            game.extra.append(stripped)
            index += 1
        games.append(game)
    return games


def _parse_turn(lines: list[str], index: int) -> tuple[Turn, int]:
    heading = lines[index].strip()
    index += 1
    body: list[str] = []
    while index < len(lines):
        raw = lines[index]
        if not raw.strip():
            break
        if not raw.startswith((" ", "\t")):
            break
        body.append(raw.strip())
        index += 1
    turn = Turn(heading=heading)
    for line in body:
        if line.startswith("Score "):
            turn.score = line
        elif line.startswith("Runes at start:"):
            turn.runes_start = line
        elif line.startswith("Runes at end:"):
            turn.runes_end = line
        else:
            turn.events.append(Event(classify(line), line))
    return turn, index


def _styles() -> dict[str, ParagraphStyle]:
    def style(name: str, **kwargs) -> ParagraphStyle:
        base = dict(
            fontName=FONT,
            fontSize=9,
            leading=12,
            textColor=INK,
            alignment=TA_LEFT,
        )
        base.update(kwargs)
        return ParagraphStyle(name, **base)

    return {
        "title": style("title", fontName=FONT_BOLD, fontSize=14, leading=18),
        "header": style("header", fontSize=9.5, leading=13),
        "outcome": style("outcome", fontName=FONT_BOLD, fontSize=10, leading=13),
        "extra": style("extra", fontSize=9, leading=12, textColor=MUTED),
        "mulligan": style("mulligan", fontSize=8.5, leading=11.5),
        "turn": style("turn", fontName=FONT_BOLD, fontSize=10.5, leading=13),
        "score": style(
            "score",
            fontName=FONT_BOLD,
            fontSize=10.5,
            leading=13,
            textColor=SCORE_INK,
            alignment=TA_RIGHT,
        ),
        "rune": style("rune", fontSize=8.5, leading=11, textColor=MUTED),
        "rune_right": style(
            "rune_right",
            fontSize=8.5,
            leading=11,
            textColor=MUTED,
            alignment=TA_RIGHT,
        ),
        "label": style("label", fontName=FONT_BOLD, fontSize=7.5, leading=10),
        "body": style("body", fontSize=9, leading=12),
        "combat_kicker": style(
            "combat_kicker",
            fontName=FONT_BOLD,
            fontSize=7.5,
            leading=9,
            textColor=COMBAT_EDGE,
        ),
        "combat": style("combat", fontSize=9, leading=12),
    }


_LABEL_HEX = {
    "move": "#3f3a32",
    "play": "#1e3a5f",
    "result": "#245c3a",
    "predict": "#5c4a32",
    "event": "#5c574e",
}


def _label_text(kind: str) -> str:
    return {
        "move": "Move",
        "play": "Play",
        "result": "Result",
        "predict": "Predict",
        "event": "Event",
    }[kind]


def _player_edge(heading: str) -> Color:
    match = PLAYER_RE.match(heading)
    if match is None:
        return BORDER
    player = match.group(1)
    if player == "P1":
        return P1_EDGE
    if player == "P2":
        return P2_EDGE
    return BORDER


def _paragraph(text: str, style: ParagraphStyle) -> Paragraph:
    return Paragraph(_xml(text), style)


def _pictures(kind: str, text: str, art: ArtCatalog | None) -> list[Path]:
    """Local pictures for the cards this line is about. Empty when none resolve."""
    limit = _ART_LIMIT.get(kind, 0)
    if art is None or limit == 0 or not text:
        return []
    if kind == "combat":
        # Who fought, not the battlefield and not the damage sentence that
        # names those units again.
        fought, _damage = _split_combat(text)
        _place, sep, fighters = fought.partition(":")
        text = fighters if sep else fought
    paths: list[Path] = []
    for source in art.find(text)[:limit]:
        path = art.file_for(source)
        if path is not None:
            paths.append(path)
    return paths


def _picture_strip(paths: list[Path], width: float, max_width: float):
    """A row of card pictures. Wraps onto another row when the line names many."""
    if not paths:
        return None
    gap = 3.0
    cell_w = width + gap
    per_row = max(1, int(max_width // cell_w))
    rows = []
    for start in range(0, len(paths), per_row):
        chunk = paths[start : start + per_row]
        cells = []
        for path in chunk:
            image = Image(
                str(path),
                width=width,
                height=width * _CARD_RATIO,
                hAlign="LEFT",
            )
            frame = Table([[image]], colWidths=[cell_w], hAlign="LEFT")
            frame.setStyle(
                TableStyle(
                    [
                        ("BOX", (0, 0), (-1, -1), 0.4, BORDER),
                        ("BACKGROUND", (0, 0), (-1, -1), white),
                        ("LEFTPADDING", (0, 0), (-1, -1), 0.6),
                        ("RIGHTPADDING", (0, 0), (-1, -1), 0.6),
                        ("TOPPADDING", (0, 0), (-1, -1), 0.6),
                        ("BOTTOMPADDING", (0, 0), (-1, -1), 0.6),
                        ("VALIGN", (0, 0), (-1, -1), "MIDDLE"),
                    ]
                )
            )
            cells.append(frame)
        row = Table([cells], colWidths=[cell_w] * len(cells), hAlign="LEFT")
        row.setStyle(
            TableStyle(
                [
                    ("VALIGN", (0, 0), (-1, -1), "TOP"),
                    ("LEFTPADDING", (0, 0), (-1, -1), 0),
                    ("RIGHTPADDING", (0, 0), (-1, -1), 0),
                    ("TOPPADDING", (0, 0), (-1, -1), 0),
                    ("BOTTOMPADDING", (0, 0), (-1, -1), 0),
                ]
            )
        )
        rows.append(row)
    if len(rows) == 1:
        return rows[0]
    stack = Table([[row] for row in rows], colWidths=[per_row * cell_w], hAlign="LEFT")
    stack.setStyle(
        TableStyle(
            [
                ("VALIGN", (0, 0), (-1, -1), "TOP"),
                ("LEFTPADDING", (0, 0), (-1, -1), 0),
                ("RIGHTPADDING", (0, 0), (-1, -1), 0),
                ("TOPPADDING", (0, 0), (-1, -1), 0),
                ("BOTTOMPADDING", (0, 0), (-1, -1), 2),
            ]
        )
    )
    return stack


def _strip_width(count: int, width: float, max_width: float) -> float:
    cell_w = width + 3.0
    per_row = max(1, int(max_width // cell_w))
    return min(count, per_row) * cell_w


def _event_row(event: Event, styles: dict[str, ParagraphStyle], art: ArtCatalog | None) -> Table:
    if event.kind == "play":
        body = Paragraph(_cost_markup(event.text), styles["body"])
    else:
        body = _paragraph(event.text, styles["body"])
    label = Paragraph(
        f'<font color="{_LABEL_HEX[event.kind]}">{_label_text(event.kind)}</font>',
        styles["label"],
    )
    pictures = _pictures(event.kind, event.text, art)
    if pictures:
        thumb_w = _THUMB_W[event.kind]
        art_w = _strip_width(len(pictures), thumb_w, INNER_W - LABEL_W - 160)
        strip = _picture_strip(pictures, thumb_w, art_w)
        cells = [label, strip, body]
        widths = [LABEL_W, art_w, INNER_W - LABEL_W - art_w]
    else:
        cells = [label, body]
        widths = [LABEL_W, INNER_W - LABEL_W]
    row = Table([cells], colWidths=widths, hAlign="LEFT")
    row.setStyle(
        TableStyle(
            [
                ("VALIGN", (0, 0), (-1, -1), "TOP"),
                ("LEFTPADDING", (0, 0), (-1, -1), 0),
                ("RIGHTPADDING", (0, 0), (-1, -1), 0),
                ("TOPPADDING", (0, 0), (-1, -1), 1),
                ("BOTTOMPADDING", (0, 0), (-1, -1), 1),
                ("RIGHTPADDING", (0, 0), (0, 0), 6),
                ("LEFTPADDING", (1, 0), (1, 0), 2),
            ]
        )
    )
    return row


def _combat_box(line: str, styles: dict[str, ParagraphStyle], art: ArtCatalog | None) -> Table:
    fought, damage = _split_combat(line)
    # The two sentences are the engine's line. The kickers name the halves.
    # Pictures sit with who fought, so the damage sentence is not repeated in art.
    rows: list[list] = [[Paragraph("Who fought", styles["combat_kicker"])]]
    pictures = _pictures("combat", line, art)
    if pictures:
        rows.append([_picture_strip(pictures, _THUMB_W["combat"], INNER_W - 36)])
    rows.append([_paragraph(fought, styles["combat"])])
    if damage is not None:
        rows.append([Paragraph("Took damage", styles["combat_kicker"])])
        rows.append([_paragraph(damage, styles["combat"])])
    box = Table(rows, colWidths=[INNER_W - 14], hAlign="LEFT")
    box.setStyle(
        TableStyle(
            [
                ("BACKGROUND", (0, 0), (-1, -1), COMBAT_BG),
                ("BOX", (0, 0), (-1, -1), 0.8, COMBAT_EDGE),
                ("LINEBEFORE", (0, 0), (0, -1), 3, COMBAT_EDGE),
                ("LEFTPADDING", (0, 0), (-1, -1), 8),
                ("RIGHTPADDING", (0, 0), (-1, -1), 6),
                ("TOPPADDING", (0, 0), (-1, 0), 5),
                ("BOTTOMPADDING", (0, -1), (-1, -1), 6),
                ("TOPPADDING", (0, 1), (-1, -1), 1),
                ("BOTTOMPADDING", (0, 0), (-1, -2), 1),
                ("VALIGN", (0, 0), (-1, -1), "TOP"),
            ]
        )
    )
    return box


def _turn_table(turn: Turn, styles: dict[str, ParagraphStyle], art: ArtCatalog | None) -> Table:
    rune = Table(
        [
            [
                _paragraph(turn.runes_start, styles["rune"]) if turn.runes_start else "",
                _paragraph(turn.runes_end, styles["rune_right"]) if turn.runes_end else "",
            ]
        ],
        colWidths=[(INNER_W - SCORE_W) / 2, (INNER_W - SCORE_W) / 2],
        hAlign="LEFT",
    )
    rune.setStyle(
        TableStyle(
            [
                ("VALIGN", (0, 0), (-1, -1), "MIDDLE"),
                ("LEFTPADDING", (0, 0), (-1, -1), 0),
                ("RIGHTPADDING", (0, 0), (-1, -1), 0),
                ("TOPPADDING", (0, 0), (-1, -1), 1),
                ("BOTTOMPADDING", (0, 0), (-1, -1), 0),
                ("ALIGN", (1, 0), (1, 0), "RIGHT"),
            ]
        )
    )
    header = Table(
        [
            [
                _paragraph(turn.heading, styles["turn"]),
                _paragraph(turn.score, styles["score"]) if turn.score else "",
            ],
            [rune, ""],
        ],
        colWidths=[INNER_W - SCORE_W, SCORE_W],
        hAlign="LEFT",
    )
    header.setStyle(
        TableStyle(
            [
                ("SPAN", (1, 0), (1, 1)),
                ("VALIGN", (0, 0), (0, 1), "MIDDLE"),
                ("VALIGN", (1, 0), (1, 1), "MIDDLE"),
                ("BACKGROUND", (1, 0), (1, 1), SCORE_BG),
                ("ALIGN", (1, 0), (1, 1), "RIGHT"),
                ("LEFTPADDING", (0, 0), (0, 1), 0),
                ("RIGHTPADDING", (0, 0), (0, 1), 6),
                ("LEFTPADDING", (1, 0), (1, 1), 6),
                ("RIGHTPADDING", (1, 0), (1, 1), 2),
                ("TOPPADDING", (0, 0), (-1, 0), 2),
                ("BOTTOMPADDING", (0, 0), (-1, 0), 1),
                ("TOPPADDING", (0, 1), (-1, 1), 1),
                ("BOTTOMPADDING", (0, 1), (-1, 1), 2),
                ("VALIGN", (0, 0), (-1, -1), "MIDDLE"),
            ]
        )
    )

    rows: list[list] = [[header]]
    combat_rows: list[int] = []
    for event in turn.events:
        if event.kind == "combat":
            combat_rows.append(len(rows))
            rows.append([_combat_box(event.text, styles, art)])
        else:
            rows.append([_event_row(event, styles, art)])

    table = Table(rows, colWidths=[CONTENT_W], hAlign="LEFT", repeatRows=1)
    commands: list[tuple] = [
        ("VALIGN", (0, 0), (-1, -1), "TOP"),
        ("BACKGROUND", (0, 0), (-1, 0), HEADER_BG),
        ("BOX", (0, 0), (-1, -1), 0.8, BORDER),
        ("LINEBEFORE", (0, 0), (0, -1), 3.5, _player_edge(turn.heading)),
        ("LINEBELOW", (0, 0), (-1, 0), 0.4, RULE),
        ("LEFTPADDING", (0, 0), (-1, -1), OUTER_X),
        ("RIGHTPADDING", (0, 0), (-1, -1), OUTER_X),
        ("TOPPADDING", (0, 0), (-1, -1), 2),
        ("BOTTOMPADDING", (0, 0), (-1, -1), 2),
        ("TOPPADDING", (0, 0), (-1, 0), 6),
        ("BOTTOMPADDING", (0, 0), (-1, 0), 5),
        ("BOTTOMPADDING", (0, -1), (-1, -1), 6),
        ("BACKGROUND", (0, 0), (-1, 0), HEADER_BG),
    ]
    for row_index in combat_rows:
        # Padding above and below the box, so the combat cannot sit on the next line.
        commands.append(("TOPPADDING", (0, row_index), (-1, row_index), 5))
        commands.append(("BOTTOMPADDING", (0, row_index), (-1, row_index), 5))
    table.setStyle(TableStyle(commands))
    table.spaceBefore = 9
    table.spaceAfter = 2
    return table


def _noted(text: str, style: ParagraphStyle, kind: str, art: ArtCatalog | None, width: float):
    """The sentence, then the pictures of the cards it names."""
    pictures = _pictures(kind, text, art)
    if not pictures:
        return _paragraph(text, style)
    strip = _picture_strip(pictures, _THUMB_W[kind], width - 16)
    block = Table(
        [[_paragraph(text, style)], [strip]],
        colWidths=[width],
        hAlign="LEFT",
    )
    block.setStyle(
        TableStyle(
            [
                ("VALIGN", (0, 0), (-1, -1), "TOP"),
                ("LEFTPADDING", (0, 0), (-1, -1), 0),
                ("RIGHTPADDING", (0, 0), (-1, -1), 0),
                ("TOPPADDING", (0, 0), (-1, 0), 0),
                ("BOTTOMPADDING", (0, 0), (-1, 0), 3),
                ("TOPPADDING", (0, 1), (-1, 1), 1),
                ("BOTTOMPADDING", (0, 1), (-1, 1), 0),
            ]
        )
    )
    return block


def _game_story(game: Game, styles: dict[str, ParagraphStyle], first: bool, art: ArtCatalog | None) -> list:
    story: list = []
    if not first:
        story.append(Spacer(1, 12))
        story.append(CondPageBreak(130))
    story.append(
        HRFlowable(width="100%", thickness=0.6, color=RULE, spaceBefore=2, spaceAfter=6)
    )
    story.append(_paragraph(game.header, styles["header"]))
    if game.outcome:
        story.append(Spacer(1, 2))
        story.append(_noted(game.outcome, styles["outcome"], "outcome", art, CONTENT_W))
    for line in game.extra:
        story.append(Spacer(1, 2))
        story.append(_paragraph(line, styles["extra"]))
    if game.mulligan:
        mulligan = Table(
            [[_noted(game.mulligan, styles["mulligan"], "mulligan", art, CONTENT_W - 16)]],
            colWidths=[CONTENT_W],
            hAlign="LEFT",
        )
        mulligan.setStyle(
            TableStyle(
                [
                    ("BACKGROUND", (0, 0), (-1, -1), MULLIGAN_BG),
                    ("BOX", (0, 0), (-1, -1), 0.4, RULE),
                    ("LEFTPADDING", (0, 0), (-1, -1), 8),
                    ("RIGHTPADDING", (0, 0), (-1, -1), 8),
                    ("TOPPADDING", (0, 0), (-1, -1), 5),
                    ("BOTTOMPADDING", (0, 0), (-1, -1), 5),
                    ("VALIGN", (0, 0), (-1, -1), "TOP"),
                ]
            )
        )
        mulligan.spaceBefore = 6
        mulligan.spaceAfter = 2
        story.append(mulligan)
    for turn in game.turns:
        # A turn header does not start in the last inch of a page, jammed
        # against the previous turn's wrapped line.
        story.append(CondPageBreak(86))
        story.append(_turn_table(turn, styles, art))
    return story


def _draw_page(canvas, doc) -> None:
    canvas.saveState()
    canvas.setFillColor(MUTED)
    canvas.setFont(FONT, 8)
    canvas.drawString(LEFT, PAGE_H - 30, "Play-by-play")
    canvas.drawRightString(PAGE_W - RIGHT, PAGE_H - 30, str(doc.page))
    canvas.setStrokeColor(RULE)
    canvas.setLineWidth(0.4)
    canvas.line(LEFT, PAGE_H - 36, PAGE_W - RIGHT, PAGE_H - 36)
    canvas.restoreState()


def write_pdf(
    games: list[Game],
    path: str | Path,
    title: str | None = None,
    art: ArtCatalog | None = None,
) -> None:
    """Write ``games`` in the scannable layout. This is the only layout."""
    if not games:
        raise ValueError("no play-by-play games to write")
    _register_fonts()
    styles = _styles()
    doc = BaseDocTemplate(
        str(path),
        pagesize=letter,
        leftMargin=LEFT,
        rightMargin=RIGHT,
        topMargin=TOP,
        bottomMargin=BOTTOM,
        title=title or "Play-by-play",
        author="",
    )
    frame = Frame(LEFT, BOTTOM, CONTENT_W, PAGE_H - TOP - BOTTOM, id="body", showBoundary=0)
    doc.addPageTemplates([PageTemplate(id="main", frames=[frame], onPage=_draw_page)])
    story: list = []
    if title:
        story.append(Paragraph(_xml(title), styles["title"]))
        story.append(Spacer(1, 4))
    for number, game in enumerate(games):
        story.extend(_game_story(game, styles, first=number == 0, art=art))
    doc.build(story)
    if art is not None and art.missing:
        print(
            f"{len(art.missing)} card picture(s) could not be fetched; those lines stay text",
            file=sys.stderr,
        )


def layout_problems(path: str | Path) -> list[str]:
    """Ways a page fails the glance rules. Empty means the pages are clean.

    A text line may not paint over another line. A turn header may not sit
    on the previous turn. A combat's damage line may not meet the next
    section. Nothing may run past the page margin.
    """
    import pymupdf

    doc = pymupdf.open(str(path))
    problems: list[str] = []
    try:
        for page_index, page in enumerate(doc):
            rows = _page_rows(page)
            problems.extend(_overlap_problems(page_index, rows))
            problems.extend(_margin_problems(page_index, page.rect.width, rows))
            problems.extend(_turn_gap_problems(page_index, rows))
            problems.extend(_combat_gap_problems(page_index, rows))
            problems.extend(_art_problems(page_index, page, rows))
    finally:
        doc.close()
    return problems


def _page_rows(page) -> list[tuple[str, object]]:
    rows = []
    for block in page.get_text("dict")["blocks"]:
        if block.get("type") != 0:
            continue
        for line in block["lines"]:
            text = "".join(span["text"] for span in line["spans"]).strip()
            if not text:
                continue
            x0 = min(span["bbox"][0] for span in line["spans"])
            y0 = min(span["bbox"][1] for span in line["spans"])
            x1 = max(span["bbox"][2] for span in line["spans"])
            y1 = max(span["bbox"][3] for span in line["spans"])
            rows.append((text, (x0, y0, x1, y1)))
    rows.sort(key=lambda item: (item[1][1], item[1][0]))
    return rows


def _intersects(a, b, slack: float = 0.8) -> bool:
    ax0, ay0, ax1, ay1 = a
    bx0, by0, bx1, by1 = b
    return (
        min(ax1, bx1) - max(ax0, bx0) > slack
        and min(ay1, by1) - max(ay0, by0) > slack
    )


def _overlap_problems(page_index: int, rows: list[tuple[str, tuple]]) -> list[str]:
    found = []
    for i, (left_text, left_box) in enumerate(rows):
        for right_text, right_box in rows[i + 1 :]:
            if not _intersects(left_box, right_box):
                continue
            found.append(
                f"page {page_index + 1}: {left_text!r} overlaps {right_text!r}"
            )
            if len(found) >= 8:
                return found
    return found


def _margin_problems(page_index: int, width: float, rows) -> list[str]:
    found = []
    for text, (x0, _y0, x1, _y1) in rows:
        if x0 < LEFT - 2 or x1 > width - RIGHT + 2:
            found.append(
                f"page {page_index + 1}: {text!r} runs past the margin ({x0:.1f},{x1:.1f})"
            )
    return found


def _turn_gap_problems(page_index: int, rows) -> list[str]:
    headers = [(text, box) for text, box in rows if text.startswith("Turn ")]
    found = []
    for text, box in headers[1:]:
        above = [other for _other_text, other in rows if other[3] <= box[1] + 0.5]
        if not above:
            continue
        gap = box[1] - max(other[3] for other in above)
        if gap < 6:
            found.append(
                f"page {page_index + 1}: {text!r} is {gap:.1f}pt under the line above it"
            )
    return found


_SECTION_LABELS = {"Move", "Play", "Result", "Predict", "Event", "Who fought"}


def _combat_gap_problems(page_index: int, rows) -> list[str]:
    """The damage sentence stays above the next section, with air between them.

    A wrapped damage sentence is still that sentence. The gap is measured
    from its last line to the next section, not between its own lines.
    """
    found = []
    for index, (text, box) in enumerate(rows):
        if text != "Took damage" and not text.startswith("No combat damage"):
            continue
        last = box
        nxt = None
        for next_text, next_box in rows[index + 1 :]:
            if next_text in _SECTION_LABELS or next_text.startswith("Turn "):
                nxt = (next_text, next_box)
                break
            last = next_box
        if nxt is None:
            continue
        next_text, next_box = nxt
        gap = next_box[1] - last[3]
        if gap < 4:
            found.append(
                f"page {page_index + 1}: combat damage is {gap:.1f}pt from {next_text!r}"
            )
    return found


def _art_problems(page_index: int, page, rows) -> list[str]:
    """A card picture stays inside the margin and off the words of the line."""
    found = []
    boxes = []
    for block in page.get_text("dict")["blocks"]:
        if block.get("type") != 1:
            continue
        boxes.append(tuple(block["bbox"]))
    for box in boxes:
        x0, _y0, x1, _y1 = box
        if x0 < LEFT - 2 or x1 > page.rect.width - RIGHT + 2:
            found.append(
                f"page {page_index + 1}: a card picture runs past the margin ({x0:.1f},{x1:.1f})"
            )
        for text, text_box in rows:
            if _intersects(box, text_box):
                found.append(f"page {page_index + 1}: a card picture overlaps {text!r}")
                break
    for i, left in enumerate(boxes):
        for right in boxes[i + 1 :]:
            if _intersects(left, right, slack=0.4):
                found.append(f"page {page_index + 1}: two card pictures overlap")
                return found
    return found


_SELF_CHECK_LOG = """\
Game 1 [seed=7]: decks fiora vs yi_rebuild, agents random-player:pass=0 vs random-player:pass=0.
Ended by points: P1 reached 8 points. P1 wins. Score 8-3. Turns 2. Decisions 40.
Mulligan: P1 shipped Charm and a second Charm and a third card with a long name, and kept Fiora, Grand Duelist, Recruit the Trainees, and Defiant Dance so this mulligan wraps inside its own box.
Turn 1 - P1 (Grand Duelist)
  Runes at start: 2.
  P1 moves Fiora, Grand Duelist from base to Monument of the Returned King.
  P1 conquers Monument of the Returned King. P1 controls it.
  P1 plays Discipline on Defiant Dance for 2 energy.
  Combat at Monument of the Returned King: P1's Fiora, Grand Duelist and Recruit the Trainees against P2's Defiant Dance and Bedeviling Stranger and Stellacorn Herder and another unit that makes this sentence wrap inside the combat box. P2's Defiant Dance takes 4 damage, P2's Bedeviling Stranger takes 3 damage, P2's Stellacorn Herder takes 2 damage, and P1's Fiora, Grand Duelist takes 5 damage.
  P1 plays Charm on Defiant Dance for 1 energy and 1 power.
  Runes at end: 4.
  Score 1-0.
Turn 2 - P2 (Wuju Bladesman - Starter)
  Runes at start: 3.
  P2 plays Blade of the Exile from face down to base, ignoring its cost.
  P2 holds Monument of the Returned King. P2 controls it.
  Runes at end: 5.
  Score 1-0.
"""


def self_check() -> list[str]:
    """Render a wrapped fixture and return every layout or wording problem."""
    games = parse_log(_SELF_CHECK_LOG)
    problems: list[str] = []
    if len(games) != 1 or len(games[0].turns) != 2:
        problems.append(f"fixture parsed as {len(games)} games")
        return problems
    turn = games[0].turns[0]
    kinds = [event.kind for event in turn.events]
    if kinds != ["move", "result", "play", "combat", "play"]:
        problems.append(f"turn 1 kinds {kinds}")
    combat = next(event.text for event in turn.events if event.kind == "combat")
    fought, damage = _split_combat(combat)
    if fought + " " + (damage or "") != combat:
        problems.append("combat split dropped or reordered words")
    if "for 2 energy." not in _cost_markup(turn.events[2].text):
        problems.append("play cost was dropped")
    with tempfile.TemporaryDirectory() as tmp:
        tmp_path = Path(tmp)
        swatch = tmp_path / "card.png"
        _write_swatch(swatch)
        # The fixture names these. Real exports use the card classes' pictures.
        art = ArtCatalog(
            [
                ("Charm", str(swatch)),
                ("Discipline", str(swatch)),
                ("Defiant Dance", str(swatch)),
                ("Fiora, Grand Duelist", str(swatch)),
                ("Monument of the Returned King", str(swatch)),
            ]
        )
        path = tmp_path / "layout.pdf"
        write_pdf(games, path, title="Layout check", art=art)
        problems.extend(layout_problems(path))
        import pymupdf

        doc = pymupdf.open(path)
        text = "".join(page.get_text() for page in doc)
        images = sum(len(page.get_images()) for page in doc)
        doc.close()
        if images < 1:
            problems.append("no card pictures were drawn")
    for line in (
        "Game 1 [seed=7]:",
        "Ended by points: P1 reached 8 points.",
        "Mulligan: P1 shipped Charm",
        "Turn 1 - P1 (Grand Duelist)",
        "Runes at start: 2.",
        "Runes at end: 4.",
        "Score 1-0.",
        "P1 moves Fiora, Grand Duelist from base to Monument of the Returned King.",
        "P1 conquers Monument of the Returned King. P1 controls it.",
        "for 2 energy.",
        "Who fought",
        "Took damage",
        "takes 5 damage.",
        "P1 plays Charm on Defiant Dance for 1 energy and 1 power.",
        "Turn 2 - P2 (Wuju Bladesman - Starter)",
        "ignoring its cost.",
        "P2 holds Monument of the Returned King. P2 controls it.",
    ):
        if line not in text:
            problems.append(f"missing {line!r}")
    return problems


def _write_swatch(path: Path) -> None:
    from PIL import Image as PilImage

    PilImage.new("RGB", (80, 112), (176, 138, 86)).save(path, "PNG")


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "logs",
        nargs="*",
        type=Path,
        help="Engine logs. Omit to read stdin.",
    )
    parser.add_argument("--output", "-o", type=Path, help="PDF to write.")
    parser.add_argument("--title", help="Title above the first game.")
    parser.add_argument(
        "--self-check",
        action="store_true",
        help="Render a fixture and check that turns and combat stay apart.",
    )
    args = parser.parse_args(argv)
    if args.self_check:
        problems = self_check()
        if problems:
            for problem in problems:
                print(problem, file=sys.stderr)
            return 1
        print("play-by-play layout check passed")
        return 0
    if args.output is None:
        parser.error("--output is required")
    if args.logs:
        text = "\n".join(path.read_text(encoding="utf-8") for path in args.logs)
    else:
        text = sys.stdin.read()
    games = parse_log(text)
    if not games:
        print("no play-by-play found in the input", file=sys.stderr)
        return 1
    art = load_card_art(repo_root())
    if not art:
        print("no card pictures found on the card classes; writing text only", file=sys.stderr)
        art = None
    write_pdf(games, args.output, title=args.title, art=art)
    print(f"wrote {args.output} ({len(games)} game{'s' if len(games) != 1 else ''})")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
