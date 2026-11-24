#!/usr/bin/env python3
"""Write a play-by-play PDF from the text a game prints.

This is the play-by-play PDF exporter. ``write_pdf`` is the only layout.
Later exports call it. The engine's lines are not rewritten.

A turn is a card you can read at a glance. The header names whose turn it
is, the score, and the runes at the start and at the end. Under that, space
and type separate the lines: a play with the cost marked, a quieter move, a
conquer or a hold as a result, and a combat box that says who fought and
then who took damage. A hide stays on its line. A facedown play still says
it was from face down. A unit still names the player who controls it, in
parentheses. Accelerate stays in the cost when it was paid.

A line that names a card shows that card's picture beside the line, from
``image_url`` on the card class. The pictures are not gathered into a
gallery. A picture that cannot be fetched is left out. The words stay.

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
from reportlab.lib.enums import TA_CENTER, TA_LEFT, TA_RIGHT
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
LEFT = 46
RIGHT = 46
TOP = 54
BOTTOM = 42
CONTENT_W = PAGE_W - LEFT - RIGHT
PAD_L = 14
PAD_R = 12
INNER = CONTENT_W - PAD_L - PAD_R
SCORE_COL = 118

INK = HexColor("#1c1915")
MUTED = HexColor("#5e584f")
RULE = HexColor("#e3dcd2")
BORDER = HexColor("#ddd4c8")
PAPER = HexColor("#f4f1eb")
P1_EDGE = HexColor("#2c4a34")
P2_EDGE = HexColor("#1c3d66")
P1_BAND = HexColor("#e4f0e4")
P2_BAND = HexColor("#e3eef8")
SCORE_INK = HexColor("#163324")
SCORE_EDGE = HexColor("#b7d0b2")
COST_HEX = "#1c3d66"
COST_BG_HEX = "#e3eef8"
FACE_HEX = "#6b4e12"
CONQUER_BG = HexColor("#e3f2e2")
CONQUER_EDGE = HexColor("#2f7a45")
CONQUER_HEX = "#1c6b38"
HOLD_BG = HexColor("#f6f1e2")
HOLD_EDGE = HexColor("#a6843d")
HOLD_HEX = "#6e5420"
COMBAT_BG = HexColor("#fbf6ee")
COMBAT_EDGE = HexColor("#b08968")
COMBAT_HEX = "#6b4e32"
QUIET = HexColor("#4e4942")

P1_HEX = "#2c4a34"
P2_HEX = "#1c3d66"

GAME_RE = re.compile(r"^Game \d+ \[seed=")
TURN_RE = re.compile(r"^Turn \d+ - \S+")
PLAYER_RE = re.compile(r"^Turn \d+ - (\S+)")
_ACTOR_RE = re.compile(r"^(P1|P2)\b")
_SEAT_RE = re.compile(r"\(P[12]\)")
_POSSESSIVE_RE = re.compile(r"P[12](?='s)")
_FACE_RE = re.compile(r"from face down")
_HIDE_RE = re.compile(r"\bhides\b")
_TAKES_RE = re.compile(r"takes \d+ damage")
_SCORE_RE = re.compile(r"^Score (\d+-\d+)\.$")
_RUNE_RE = re.compile(r"^(.*?)(\d+)(\.)$")

FONT = "Pbp"
FONT_BOLD = "Pbp-Bold"
FONT_CJK = "Pbp-CJK"

# How many pictures a kind of line can carry, and the preferred width.
# A play shows the card and what it was played on. A combat shows who fought.
# The score and the rune counts name no card.
_ART_LIMIT = {
    "play": 3,
    "move": 2,
    "result": 1,
    "combat": 8,
    "event": 2,
    "hide": 2,
    "mulligan": 8,
    "outcome": 2,
}
_THUMB_W = {
    "play": 38,
    "move": 32,
    "result": 34,
    "combat": 28,
    "event": 30,
    "hide": 36,
    "mulligan": 24,
    "outcome": 30,
}
_ART_SHARE = {"combat": 0.56, "mulligan": 0.46}
# A unit is 744 wide by 1039 tall. A battlefield is that card turned
# sideways. Each picture is drawn at the file's own ratio.
_CARD_RATIO = 1039 / 744
_PIXEL_SIZE: dict[str, tuple[int, int]] = {}
_NAME_RE = re.compile(r'\bd\.name\s*=\s*R"RB\((.*?)\)RB"', re.S)
_URL_RE = re.compile(r'\bd\.image_url\s*=\s*R"RB\((.*?)\)RB"', re.S)
_THUMB_CHROME = 2.4
_THUMB_GAP = 4.0


def _register_fonts() -> None:
    global FONT, FONT_BOLD, FONT_CJK
    if FONT in pdfmetrics.getRegisteredFontNames():
        return
    regular = _first_file(
        (
            "/usr/share/fonts/truetype/macos/Inter-Regular.ttf",
            "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        )
    )
    bold = _first_file(
        (
            "/usr/share/fonts/truetype/macos/Inter-Bold.ttf",
            "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf",
        )
    )
    cjk = _first_file(("/usr/share/fonts/truetype/droid/DroidSansFallbackFull.ttf",))
    if regular and bold:
        pdfmetrics.registerFont(TTFont(FONT, regular))
        pdfmetrics.registerFont(TTFont(FONT_BOLD, bold))
        pdfmetrics.registerFontFamily(FONT, normal=FONT, bold=FONT_BOLD, italic=FONT, boldItalic=FONT_BOLD)
    else:
        FONT = "Times-Roman"
        FONT_BOLD = "Times-Bold"
    if cjk:
        pdfmetrics.registerFont(TTFont(FONT_CJK, cjk))
    else:
        FONT_CJK = FONT


def _first_file(paths: tuple[str, ...]) -> str | None:
    for path in paths:
        if Path(path).is_file():
            return path
    return None


def _xml(text: str) -> str:
    return text.replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;")


def _xml_runs(text: str) -> str:
    """Escape text, and draw characters Inter does not cover in the CJK face."""
    if not text:
        return ""
    parts: list[str] = []
    buf: list[str] = []
    cjk: bool | None = None

    def flush() -> None:
        if not buf:
            return
        chunk = _xml("".join(buf))
        if cjk and FONT_CJK != FONT:
            parts.append(f'<font name="{FONT_CJK}">{chunk}</font>')
        else:
            parts.append(chunk)
        buf.clear()

    for ch in text:
        is_cjk = ord(ch) > 0x2E7F
        if cjk is not None and is_cjk != cjk:
            flush()
        cjk = is_cjk
        buf.append(ch)
    flush()
    return "".join(parts)


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

    def spans(self, line: str) -> list[tuple[int, int, str]]:
        """``(start, end, source)`` for each card name in ``line``, in order."""
        if not line or not self._entries:
            return []
        occupied = bytearray(len(line))
        hits: list[tuple[int, int, str]] = []
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
                    hits.append((at, end, source))
                    for index in range(at, end):
                        occupied[index] = 1
                start = at + 1
        hits.sort(key=lambda item: item[0])
        return hits

    def find(self, line: str) -> list[str]:
        return [source for _start, _end, source in self.spans(line)]

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
    if verb == "hides":
        return "hide"
    if verb in ("conquers", "holds"):
        return "result"
    if verb == "Predicts" or verb.startswith("Predicts"):
        return "predict"
    return "event"


def _cost_at(line: str) -> int | None:
    """Where the cost clause starts, so it can be marked and still read."""
    ignoring = line.rfind(", ignoring")
    paying = line.rfind(" for ")
    start = None
    if ignoring != -1:
        start = ignoring + 2
    if paying != -1 and (start is None or paying + 1 < start):
        start = paying + 1
    return start


def _cost_markup(line: str) -> str:
    """The play sentence, with the cost clause the engine wrote set apart."""
    return _rich_text(line, cost=True)


def _split_combat(line: str) -> tuple[str, str | None]:
    """Who fought, then who took damage. The words stay in order."""
    head, sep, tail = line.partition(". ")
    if not sep:
        return line, None
    return head + ".", tail


def _rich_text(
    text: str,
    art: ArtCatalog | None = None,
    *,
    cost: bool = False,
    damage: bool = False,
) -> str:
    """The engine's words, with the cost, the controller, and a card name marked."""
    cost_at = _cost_at(text) if cost else None
    body = text if cost_at is None else text[:cost_at]
    tail = "" if cost_at is None else text[cost_at:]
    marked = _mark(body, art, damage=damage)
    if tail:
        # The cost stays one chip. A wrapped "1" / "power" reads as two costs.
        solid = tail.replace(" ", "\u00a0")
        marked += (
            f'<font color="{COST_HEX}" backColor="{COST_BG_HEX}"><b>{_xml_runs(solid)}</b></font>'
        )
    return marked


def _mark(text: str, art: ArtCatalog | None, *, damage: bool) -> str:
    spans: list[tuple[int, int, str]] = []
    actor = _ACTOR_RE.match(text)
    if actor:
        spans.append((actor.start(), actor.end(), "actor"))
    if art is not None:
        for start, end, _source in art.spans(text):
            spans.append((start, end, "name"))
    for match in _SEAT_RE.finditer(text):
        spans.append((match.start(), match.end(), "seat"))
    for match in _POSSESSIVE_RE.finditer(text):
        spans.append((match.start(), match.end(), "actor"))
    for match in _FACE_RE.finditer(text):
        spans.append((match.start(), match.end(), "face"))
    for match in _HIDE_RE.finditer(text):
        spans.append((match.start(), match.end(), "hide"))
    if damage:
        for match in _TAKES_RE.finditer(text):
            spans.append((match.start(), match.end(), "damage"))
    spans.sort(key=lambda item: (item[0], -(item[1] - item[0])))
    parts: list[str] = []
    cursor = 0
    for start, end, kind in spans:
        if start < cursor:
            continue
        if start > cursor:
            parts.append(_xml_runs(text[cursor:start]))
        parts.append(_wrap(kind, text[start:end]))
        cursor = end
    if cursor < len(text):
        parts.append(_xml_runs(text[cursor:]))
    return "".join(parts)


def _wrap(kind: str, token: str) -> str:
    escaped = _xml_runs(token)
    if kind in ("actor", "seat"):
        color = P1_HEX if "P1" in token else P2_HEX
        return f'<font color="{color}"><b>{escaped}</b></font>'
    if kind == "name":
        return f"<b>{escaped}</b>"
    if kind in ("face", "hide"):
        return f'<font color="{FACE_HEX}"><b>{escaped}</b></font>'
    if kind == "damage":
        return f"<b>{escaped}</b>"
    return escaped


def _score_markup(score: str) -> str:
    match = _SCORE_RE.match(score.strip())
    if match is None:
        return _xml_runs(score)
    return (
        f'<font color="#5e584f" size="8">Score </font>'
        f'<font color="#163324" size="14"><b>{_xml(match.group(1))}.</b></font>'
    )


def _rune_markup(line: str) -> str:
    match = _RUNE_RE.match(line.strip())
    if match is None:
        return _xml_runs(line)
    return (
        f'<font color="#5e584f">{_xml_runs(match.group(1))}</font>'
        f"<b>{_xml(match.group(2))}</b>"
        f'<font color="#5e584f">{_xml(match.group(3))}</font>'
    )


def _heading_markup(heading: str) -> str:
    match = re.match(r"^(Turn \d+ - )(P1|P2)(.*)$", heading)
    if match is None:
        return _xml_runs(heading)
    color = P1_HEX if match.group(2) == "P1" else P2_HEX
    return (
        _xml_runs(match.group(1))
        + f'<font color="{color}"><b>{match.group(2)}</b></font>'
        + _xml_runs(match.group(3))
    )


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
            fontSize=10,
            leading=13.5,
            textColor=INK,
            alignment=TA_LEFT,
        )
        base.update(kwargs)
        return ParagraphStyle(name, **base)

    return {
        "title": style("title", fontName=FONT_BOLD, fontSize=16, leading=20),
        "header": style("header", fontSize=9.5, leading=13, textColor=INK),
        "outcome": style("outcome", fontName=FONT_BOLD, fontSize=11, leading=15),
        "extra": style("extra", fontSize=9, leading=12, textColor=MUTED),
        "mulligan": style("mulligan", fontSize=9, leading=12.5),
        "turn": style("turn", fontName=FONT_BOLD, fontSize=12, leading=15.5),
        "score": style(
            "score",
            fontName=FONT_BOLD,
            fontSize=14,
            leading=17,
            textColor=SCORE_INK,
            alignment=TA_CENTER,
        ),
        "rune": style("rune", fontSize=8.5, leading=11, textColor=MUTED),
        "rune_right": style(
            "rune_right",
            fontSize=8.5,
            leading=11,
            textColor=MUTED,
            alignment=TA_RIGHT,
        ),
        "play": style("play", fontSize=10.5, leading=14),
        "move": style("move", fontSize=9.5, leading=12.5, textColor=QUIET),
        "quiet": style("quiet", fontSize=9, leading=12, textColor=MUTED),
        "kicker": style("kicker", fontName=FONT_BOLD, fontSize=8, leading=10),
        "combat": style("combat", fontSize=10, leading=13.5),
        "result": style("result", fontSize=10.5, leading=14),
    }


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


def _player_band(heading: str) -> Color:
    match = PLAYER_RE.match(heading)
    if match is not None and match.group(1) == "P2":
        return P2_BAND
    if match is not None and match.group(1) == "P1":
        return P1_BAND
    return HexColor("#f3efe8")


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
    seen: set[str] = set()
    for source in art.find(text):
        if source in seen:
            continue
        seen.add(source)
        if len(paths) >= limit:
            break
        path = art.file_for(source)
        if path is not None:
            paths.append(path)
    return paths


def _pixel_size(path: Path) -> tuple[int, int]:
    key = str(path)
    if key not in _PIXEL_SIZE:
        from PIL import Image as PilImage

        with PilImage.open(path) as image:
            _PIXEL_SIZE[key] = image.size
    return _PIXEL_SIZE[key]


def _draw_size(path: Path, thumb: float) -> tuple[float, float]:
    """The short side is ``thumb``. The other side follows the file.

    A portrait card stays as tall as it was. A battlefield, which is that
    card on its side, comes out wide and short instead of squeezed into the
    portrait frame.
    """
    width, height = _pixel_size(path)
    if width <= 0 or height <= 0:
        return thumb, thumb * _CARD_RATIO
    if height >= width:
        return thumb, thumb * (height / width)
    return thumb * (width / height), thumb


def _cell_width(draw_w: float) -> float:
    return draw_w + _THUMB_CHROME + _THUMB_GAP


def _picture_strip(paths: list[Path], thumb: float):
    """One row of card pictures, each at its own aspect ratio."""
    if not paths:
        return None, 0.0
    cells = []
    widths = []
    for path in paths:
        draw_w, draw_h = _draw_size(path, thumb)
        cell_w = _cell_width(draw_w)
        image = Image(str(path), width=draw_w, height=draw_h, hAlign="LEFT")
        frame = Table([[image]], colWidths=[cell_w], hAlign="LEFT")
        frame.setStyle(
            TableStyle(
                [
                    ("BOX", (0, 0), (-1, -1), 0.4, BORDER),
                    ("BACKGROUND", (0, 0), (-1, -1), white),
                    ("LEFTPADDING", (0, 0), (-1, -1), 0.5),
                    ("RIGHTPADDING", (0, 0), (-1, -1), 0.5),
                    ("TOPPADDING", (0, 0), (-1, -1), 0.5),
                    ("BOTTOMPADDING", (0, 0), (-1, -1), 0.5),
                    ("VALIGN", (0, 0), (-1, -1), "MIDDLE"),
                    ("RIGHTPADDING", (0, 0), (0, 0), 0.5 + _THUMB_GAP),
                ]
            )
        )
        cells.append(frame)
        widths.append(cell_w)
    row = Table([cells], colWidths=widths, hAlign="LEFT")
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
    return row, sum(widths)


def _strip_width(paths: list[Path], thumb: float) -> float:
    return sum(_cell_width(_draw_size(path, thumb)[0]) for path in paths)


def _beside(paragraph, pictures: list[Path], kind: str, width: float):
    """The sentence, with the pictures of the cards it names on its left."""
    if not pictures:
        return paragraph
    share = _ART_SHARE.get(kind, 0.40)
    budget = min(width * share, max(56.0, width - 168.0))
    thumb = float(_THUMB_W.get(kind, 28))
    art_w = _strip_width(pictures, thumb)
    limit = min(budget, width - 150)
    if art_w > limit > 0:
        thumb *= limit / art_w
        art_w = _strip_width(pictures, thumb)
    if art_w > width - 120 > 0:
        thumb *= (width - 120) / art_w
        art_w = _strip_width(pictures, thumb)
    strip, art_w = _picture_strip(pictures, thumb)
    row = Table(
        [[strip, paragraph]],
        colWidths=[art_w, width - art_w],
        hAlign="LEFT",
    )
    row.setStyle(
        TableStyle(
            [
                ("VALIGN", (0, 0), (-1, -1), "TOP"),
                ("LEFTPADDING", (0, 0), (-1, -1), 0),
                ("RIGHTPADDING", (0, 0), (-1, -1), 0),
                ("TOPPADDING", (0, 0), (-1, -1), 1),
                ("BOTTOMPADDING", (0, 0), (-1, -1), 1),
                ("LEFTPADDING", (1, 0), (1, 0), 2),
            ]
        )
    )
    return row


def _sentence(
    text: str,
    style: ParagraphStyle,
    kind: str,
    art: ArtCatalog | None,
    width: float,
    *,
    cost: bool = False,
    damage: bool = False,
):
    paragraph = Paragraph(_rich_text(text, art, cost=cost, damage=damage), style)
    return _beside(paragraph, _pictures(kind, text, art), kind, width)


def _pads(kind: str, previous: str | None) -> tuple[float, float]:
    """Air around a line. A run of moves stays a run. A play or a result opens up."""
    if kind == "combat":
        return 10, 8
    if kind == "result":
        return 9, 8
    if kind == "move" and previous == "move":
        return 3, 2
    if kind == "move":
        return 6, 3
    if kind in ("predict", "event"):
        return 4, 3
    return 8, 5


def _result_band(event: Event, styles: dict[str, ParagraphStyle], art: ArtCatalog | None) -> Table:
    parts = event.text.split(" ", 2)
    verb = "Hold" if len(parts) > 1 and parts[1] == "holds" else "Conquer"
    if verb == "Hold":
        bg, edge, color = HOLD_BG, HOLD_EDGE, HOLD_HEX
    else:
        bg, edge, color = CONQUER_BG, CONQUER_EDGE, CONQUER_HEX
    inner = INNER - 20
    kicker = Paragraph(f'<font color="{color}"><b>{verb}</b></font>', styles["kicker"])
    body = _sentence(event.text, styles["result"], "result", art, inner)
    band = Table([[kicker], [body]], colWidths=[INNER], hAlign="LEFT")
    band.setStyle(
        TableStyle(
            [
                ("BACKGROUND", (0, 0), (-1, -1), bg),
                ("BOX", (0, 0), (-1, -1), 0.6, edge),
                ("LINEBEFORE", (0, 0), (0, -1), 3.5, edge),
                ("LEFTPADDING", (0, 0), (-1, -1), 10),
                ("RIGHTPADDING", (0, 0), (-1, -1), 8),
                ("TOPPADDING", (0, 0), (-1, 0), 6),
                ("BOTTOMPADDING", (0, 0), (-1, 0), 1),
                ("TOPPADDING", (0, 1), (-1, 1), 1),
                ("BOTTOMPADDING", (0, 1), (-1, 1), 7),
                ("VALIGN", (0, 0), (-1, -1), "TOP"),
            ]
        )
    )
    return band


def _combat_box(line: str, styles: dict[str, ParagraphStyle], art: ArtCatalog | None) -> Table:
    fought, damage = _split_combat(line)
    inner = INNER - 20
    kicker = styles["kicker"]
    who = Paragraph(f'<font color="{COMBAT_HEX}"><b>Who fought</b></font>', kicker)
    # Pictures come from the full combat line, so the damage half is not drawn again.
    fought_line = _beside(
        Paragraph(_rich_text(fought, art), styles["combat"]),
        _pictures("combat", line, art),
        "combat",
        inner,
    )
    rows: list[list] = [[who], [fought_line]]
    if damage is not None:
        took = Paragraph(f'<font color="{COMBAT_HEX}"><b>Took damage</b></font>', kicker)
        harm = Paragraph(_rich_text(damage, art, damage=True), styles["combat"])
        rows.append([took])
        rows.append([harm])
    box = Table(rows, colWidths=[INNER], hAlign="LEFT")
    commands: list[tuple] = [
        ("BACKGROUND", (0, 0), (-1, -1), COMBAT_BG),
        ("BOX", (0, 0), (-1, -1), 0.7, COMBAT_EDGE),
        ("LINEBEFORE", (0, 0), (0, -1), 3.5, COMBAT_EDGE),
        ("LEFTPADDING", (0, 0), (-1, -1), 10),
        ("RIGHTPADDING", (0, 0), (-1, -1), 8),
        ("VALIGN", (0, 0), (-1, -1), "TOP"),
        ("TOPPADDING", (0, 0), (-1, 0), 7),
        ("BOTTOMPADDING", (0, 0), (-1, 0), 1),
        ("TOPPADDING", (0, 1), (-1, 1), 2),
        ("BOTTOMPADDING", (0, 1), (-1, 1), 4),
    ]
    if damage is not None:
        commands.extend(
            [
                ("TOPPADDING", (0, 2), (-1, 2), 6),
                ("BOTTOMPADDING", (0, 2), (-1, 2), 1),
                ("TOPPADDING", (0, 3), (-1, 3), 1),
                ("BOTTOMPADDING", (0, 3), (-1, 3), 8),
            ]
        )
    else:
        commands.append(("BOTTOMPADDING", (0, 1), (-1, 1), 8))
    box.setStyle(TableStyle(commands))
    return box


def _turn_header(turn: Turn, styles: dict[str, ParagraphStyle]) -> Table:
    heading = Paragraph(_heading_markup(turn.heading), styles["turn"])
    if turn.score:
        score = Paragraph(_score_markup(turn.score), styles["score"])
        chip = Table([[score]], colWidths=[SCORE_COL - 6], hAlign="RIGHT")
        chip.setStyle(
            TableStyle(
                [
                    ("BACKGROUND", (0, 0), (-1, -1), white),
                    ("BOX", (0, 0), (-1, -1), 0.7, SCORE_EDGE),
                    ("LEFTPADDING", (0, 0), (-1, -1), 4),
                    ("RIGHTPADDING", (0, 0), (-1, -1), 4),
                    ("TOPPADDING", (0, 0), (-1, -1), 2),
                    ("BOTTOMPADDING", (0, 0), (-1, -1), 1),
                    ("VALIGN", (0, 0), (-1, -1), "MIDDLE"),
                    ("ALIGN", (0, 0), (-1, -1), "CENTER"),
                ]
            )
        )
    else:
        chip = ""
    start = Paragraph(_rune_markup(turn.runes_start), styles["rune"]) if turn.runes_start else ""
    end = Paragraph(_rune_markup(turn.runes_end), styles["rune_right"]) if turn.runes_end else ""
    text_w = INNER - SCORE_COL
    header = Table(
        [[heading, chip], [start, end]],
        colWidths=[text_w, SCORE_COL],
        hAlign="LEFT",
    )
    header.setStyle(
        TableStyle(
            [
                ("VALIGN", (0, 0), (0, 0), "MIDDLE"),
                ("VALIGN", (1, 0), (1, 0), "MIDDLE"),
                ("VALIGN", (0, 1), (-1, 1), "MIDDLE"),
                ("ALIGN", (1, 0), (1, 0), "RIGHT"),
                ("LEFTPADDING", (0, 0), (-1, -1), 0),
                ("RIGHTPADDING", (0, 0), (-1, -1), 0),
                ("TOPPADDING", (0, 0), (-1, 0), 1),
                ("BOTTOMPADDING", (0, 0), (-1, 0), 2),
                ("TOPPADDING", (0, 1), (-1, 1), 3),
                ("BOTTOMPADDING", (0, 1), (-1, 1), 1),
                ("RIGHTPADDING", (0, 0), (0, -1), 8),
            ]
        )
    )
    return header


def _turn_table(turn: Turn, styles: dict[str, ParagraphStyle], art: ArtCatalog | None) -> Table:
    rows: list[list] = [[_turn_header(turn, styles)]]
    kinds = ["header"]
    previous: str | None = None
    for event in turn.events:
        if event.kind == "combat":
            rows.append([_combat_box(event.text, styles, art)])
        elif event.kind == "result":
            rows.append([_result_band(event, styles, art)])
        elif event.kind == "play":
            rows.append([_sentence(event.text, styles["play"], "play", art, INNER, cost=True)])
        elif event.kind == "hide":
            rows.append([_sentence(event.text, styles["play"], "hide", art, INNER, cost=True)])
        elif event.kind == "move":
            rows.append([_sentence(event.text, styles["move"], "move", art, INNER)])
        else:
            rows.append([_sentence(event.text, styles["quiet"], event.kind, art, INNER)])
        kinds.append(event.kind)
        previous = event.kind
    table = Table(rows, colWidths=[CONTENT_W], hAlign="LEFT", repeatRows=1)
    edge = _player_edge(turn.heading)
    commands: list[tuple] = [
        ("VALIGN", (0, 0), (-1, -1), "TOP"),
        ("BACKGROUND", (0, 0), (-1, -1), white),
        ("BACKGROUND", (0, 0), (-1, 0), _player_band(turn.heading)),
        ("BOX", (0, 0), (-1, -1), 0.7, BORDER),
        ("LINEBEFORE", (0, 0), (0, -1), 4, edge),
        ("LINEBELOW", (0, 0), (-1, 0), 1.25, edge),
        ("LEFTPADDING", (0, 0), (-1, -1), PAD_L),
        ("RIGHTPADDING", (0, 0), (-1, -1), PAD_R),
        ("TOPPADDING", (0, 0), (-1, 0), 8),
        ("BOTTOMPADDING", (0, 0), (-1, 0), 7),
    ]
    previous = None
    for index, kind in enumerate(kinds):
        if index == 0:
            continue
        top, bottom = _pads(kind, previous)
        commands.append(("TOPPADDING", (0, index), (-1, index), top))
        commands.append(("BOTTOMPADDING", (0, index), (-1, index), bottom))
        previous = kind
    if len(rows) > 1:
        commands.append(("BOTTOMPADDING", (0, -1), (-1, -1), 10))
    table.setStyle(TableStyle(commands))
    table.spaceBefore = 16
    table.spaceAfter = 2
    return table


def _outcome_band(game: Game, styles: dict[str, ParagraphStyle], art: ArtCatalog | None):
    if not game.outcome:
        return None
    lowered = game.outcome.lower()
    if "not usable" in lowered or "throw" in lowered:
        bg, edge = HexColor("#f8ece8"), HexColor("#a15a4a")
    elif "draw" in lowered or "no winner" in lowered:
        bg, edge = HexColor("#f3efe8"), HexColor("#8a8174")
    else:
        bg, edge = HexColor("#e7f3e6"), HexColor("#2f7a45")
    body = _sentence(game.outcome, styles["outcome"], "outcome", art, CONTENT_W - 22)
    band = Table([[body]], colWidths=[CONTENT_W], hAlign="LEFT")
    band.setStyle(
        TableStyle(
            [
                ("BACKGROUND", (0, 0), (-1, -1), bg),
                ("BOX", (0, 0), (-1, -1), 0.6, edge),
                ("LINEBEFORE", (0, 0), (0, -1), 4, edge),
                ("LEFTPADDING", (0, 0), (-1, -1), 10),
                ("RIGHTPADDING", (0, 0), (-1, -1), 10),
                ("TOPPADDING", (0, 0), (-1, -1), 7),
                ("BOTTOMPADDING", (0, 0), (-1, -1), 7),
                ("VALIGN", (0, 0), (-1, -1), "TOP"),
            ]
        )
    )
    band.spaceBefore = 6
    band.spaceAfter = 2
    return band


def _mulligan_card(text: str, styles: dict[str, ParagraphStyle], art: ArtCatalog | None) -> Table:
    body = _sentence(text, styles["mulligan"], "mulligan", art, CONTENT_W - 20)
    card = Table([[body]], colWidths=[CONTENT_W], hAlign="LEFT")
    card.setStyle(
        TableStyle(
            [
                ("BACKGROUND", (0, 0), (-1, -1), white),
                ("BOX", (0, 0), (-1, -1), 0.6, BORDER),
                ("LEFTPADDING", (0, 0), (-1, -1), 10),
                ("RIGHTPADDING", (0, 0), (-1, -1), 10),
                ("TOPPADDING", (0, 0), (-1, -1), 7),
                ("BOTTOMPADDING", (0, 0), (-1, -1), 7),
                ("VALIGN", (0, 0), (-1, -1), "TOP"),
            ]
        )
    )
    card.spaceBefore = 8
    card.spaceAfter = 2
    return card


def _game_story(game: Game, styles: dict[str, ParagraphStyle], first: bool, art: ArtCatalog | None) -> list:
    story: list = []
    if not first:
        story.append(Spacer(1, 16))
        story.append(CondPageBreak(160))
    story.append(HRFlowable(width="100%", thickness=0.6, color=RULE, spaceBefore=2, spaceAfter=8))
    story.append(Paragraph(_rich_text(game.header, art), styles["header"]))
    outcome = _outcome_band(game, styles, art)
    if outcome is not None:
        story.append(outcome)
    for line in game.extra:
        story.append(Spacer(1, 3))
        story.append(Paragraph(_rich_text(line, art), styles["extra"]))
    if game.mulligan:
        story.append(_mulligan_card(game.mulligan, styles, art))
    for turn in game.turns:
        # A turn header does not start in the last inch of a page.
        story.append(CondPageBreak(108))
        story.append(_turn_table(turn, styles, art))
    return story


def _draw_page(canvas, doc) -> None:
    canvas.saveState()
    canvas.setFillColor(PAPER)
    canvas.rect(0, 0, PAGE_W, PAGE_H, fill=1, stroke=0)
    canvas.setFillColor(MUTED)
    canvas.setFont(FONT, 8)
    canvas.drawString(LEFT, PAGE_H - 32, "Play-by-play")
    canvas.drawRightString(PAGE_W - RIGHT, PAGE_H - 32, str(doc.page))
    canvas.setStrokeColor(RULE)
    canvas.setLineWidth(0.6)
    canvas.line(LEFT, PAGE_H - 40, PAGE_W - RIGHT, PAGE_H - 40)
    canvas.restoreState()


def write_pdf(
    games: list[Game],
    path: str | Path,
    title: str | None = None,
    art: ArtCatalog | None = None,
) -> None:
    """Write ``games`` in the glance layout. This is the only layout."""
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
        story.append(Paragraph(_xml_runs(title), styles["title"]))
        story.append(Spacer(1, 2))
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
    section. A card picture sits beside the line it explains, inside the
    margin, and off the words.
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


def _combat_gap_problems(page_index: int, rows) -> list[str]:
    """The damage sentence stays above the next section, with air between them.

    A wrapped damage sentence is still that sentence. The next section is the
    first line that sits clear of it, or a conquer, a hold, or the next turn.
    """
    found = []
    for index, (text, box) in enumerate(rows):
        if text != "Took damage" and not text.startswith("No combat damage"):
            continue
        prev_bottom = box[3]
        for next_text, next_box in rows[index + 1 :]:
            gap = next_box[1] - prev_bottom
            hard = (
                next_text in ("Who fought", "Conquer", "Hold", "Hide")
                or next_text.startswith("Turn ")
                or next_text.startswith("Runes at ")
            )
            if hard or gap > 4.5:
                if gap < 6:
                    found.append(
                        f"page {page_index + 1}: combat damage is {gap:.1f}pt from {next_text!r}"
                    )
                break
            prev_bottom = max(prev_bottom, next_box[3])
    return found


def _art_problems(page_index: int, page, rows) -> list[str]:
    """A card picture stays inside the margin, off the words, and beside its line."""
    found = []
    boxes = []
    for block in page.get_text("dict")["blocks"]:
        if block.get("type") != 1:
            continue
        boxes.append(tuple(block["bbox"]))
    for box in boxes:
        x0, y0, x1, y1 = box
        if x0 < LEFT - 2 or x1 > page.rect.width - RIGHT + 2:
            found.append(
                f"page {page_index + 1}: a card picture runs past the margin ({x0:.1f},{x1:.1f})"
            )
        for text, text_box in rows:
            if _intersects(box, text_box):
                found.append(f"page {page_index + 1}: a card picture overlaps {text!r}")
                break
        beside = False
        for _text, text_box in rows:
            vertical = min(y1, text_box[3]) - max(y0, text_box[1])
            if vertical > 1 and text_box[0] >= x1 - 1:
                beside = True
                break
        if not beside:
            found.append(f"page {page_index + 1}: a card picture is not beside its line")
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
  P1 plays Tasty Faefolk (P1) to base for 8 energy, 1 power, and Accelerate.
  Runes at end: 4.
  Score 1-0.
Turn 2 - P2 (Wuju Bladesman - Starter)
  Runes at start: 3.
  P2 hides Blade of the Exile.
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
    if kinds != ["move", "result", "play", "combat", "play", "play"]:
        problems.append(f"turn 1 kinds {kinds}")
    combat = next(event.text for event in turn.events if event.kind == "combat")
    fought, damage = _split_combat(combat)
    if fought + " " + (damage or "") != combat:
        problems.append("combat split dropped or reordered words")
    if "for 2 energy." not in _cost_markup(turn.events[2].text).replace("\u00a0", " "):
        problems.append("play cost was dropped")
    hide = next(event.text for event in games[0].turns[1].events if event.kind == "hide")
    if "hides" not in hide:
        problems.append("hide line was dropped")
    with tempfile.TemporaryDirectory() as tmp:
        tmp_path = Path(tmp)
        portrait = tmp_path / "card.png"
        landscape = tmp_path / "battlefield.png"
        _write_swatch(portrait, (80, 112))
        _write_swatch(landscape, (112, 80))
        # The fixture names these. Real exports use the card classes' pictures.
        # Monument is a battlefield, so its picture is the landscape file.
        art = ArtCatalog(
            [
                ("Charm", str(portrait)),
                ("Discipline", str(portrait)),
                ("Defiant Dance", str(portrait)),
                ("Fiora, Grand Duelist", str(portrait)),
                ("Monument of the Returned King", str(landscape)),
                ("Tasty Faefolk", str(portrait)),
                ("Blade of the Exile", str(portrait)),
            ]
        )
        path = tmp_path / "layout.pdf"
        write_pdf(games, path, title="Layout check", art=art)
        problems.extend(layout_problems(path))
        import pymupdf

        doc = pymupdf.open(path)
        text = "".join(page.get_text() for page in doc).replace("\u00a0", " ")
        images = sum(len(page.get_images()) for page in doc)
        aspects = _drawn_aspects(doc)
        doc.close()
        if images < 1:
            problems.append("no card pictures were drawn")
        if not any(aspect < 0.85 for aspect in aspects):
            problems.append("a battlefield picture was not drawn landscape")
        if not any(aspect > 1.2 for aspect in aspects):
            problems.append("a unit picture was not drawn portrait")
        for aspect in aspects:
            if min(abs(aspect - 0.714), abs(aspect - 1.4)) > 0.08:
                problems.append(f"a card picture aspect {aspect:.2f} is not the file's")
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
        "(P1)",
        "and Accelerate.",
        "Turn 2 - P2 (Wuju Bladesman - Starter)",
        "P2 hides Blade of the Exile.",
        "from face down",
        "ignoring its cost.",
        "P2 holds Monument of the Returned King. P2 controls it.",
        "Conquer",
        "Hold",
    ):
        if line not in text:
            problems.append(f"missing {line!r}")
    for label in text.splitlines():
        if label.strip() in ("Play", "Move", "Result", "Event", "Predict"):
            problems.append(f"bullet label {label.strip()!r} is back")
    return problems


def _drawn_aspects(doc) -> list[float]:
    found = []
    for page in doc:
        for block in page.get_text("dict")["blocks"]:
            if block.get("type") != 1:
                continue
            x0, y0, x1, y1 = block["bbox"]
            width = x1 - x0
            height = y1 - y0
            if width > 1 and height > 1:
                found.append(height / width)
    return found


def _write_swatch(path: Path, size: tuple[int, int] = (80, 112)) -> None:
    from PIL import Image as PilImage

    PilImage.new("RGB", size, (176, 138, 86)).save(path, "PNG")


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
        help="Render a fixture and check that turns, combat, and pictures stay apart.",
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
