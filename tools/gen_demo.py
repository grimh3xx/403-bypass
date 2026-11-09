#!/usr/bin/env python3
"""Render the terminal-style demo art for 403-bypass into assets/ (dark + light).

Reuses the grimh3xx/grimh3xx brand toolchain (JBM font embedding) from the
sibling `github-profile` repo so the artwork matches the rest of the profile.

Usage: python3 tools/gen_demo.py
"""
import sys
from pathlib import Path
from xml.sax.saxutils import escape

ROOT = Path(__file__).resolve().parent.parent
OUT = ROOT / "docs"

BRAND = Path.home() / "github-profile" / "scripts"
sys.path.insert(0, str(BRAND))
try:
    from brand import finish  # type: ignore
except Exception:  # pragma: no cover
    def finish(svg, faces=None):
        return svg

MONO = "'JBM','JetBrains Mono',ui-monospace,Consolas,Menlo,monospace"

THEMES = {
    "": {
        "bg": "#0B0F17", "bg2": "#0E1420", "bar": "#111826", "border": "#1E2A3D",
        "text": "#C5CFDD", "muted": "#8B98AD", "title": "#F1F5FB",
        "cyan": "#22D3EE", "blue": "#3B82F6",
        "red": "#F87171", "amber": "#FBBF24", "green": "#34D399",
    },
    "-light": {
        "bg": "#FFFFFF", "bg2": "#F6F8FB", "bar": "#EEF2F7", "border": "#D8DEE8",
        "text": "#334155", "muted": "#5B6678", "title": "#0F172A",
        "cyan": "#0891B2", "blue": "#2563EB",
        "red": "#DC2626", "amber": "#B45309", "green": "#059669",
    },
}

BANNER = r"""  _ _  ___ ____    _
 | | || _ \__ /   | |__ _  _ _ __  __ _ ___ ___
 |_  _||  _/|_ \   | '_ \ || | '_ \/ _` (_-<(_-<
   |_| |_| |___/   |_.__/\_, | .__/\__,_/__//__/
                        |__/|_|""".split("\n")


def seg(t, c=None, b=False):
    return (t, c, b)


def build_lines(th):
    lines = [
        [seg("$ ", "green", True),
         seg("403-bypass -u https://target.tld/admin -H 'Cookie: sess=…' -mc 200,302")],
        [],
    ]
    lines += [[seg(b, "cyan", True)] for b in BANNER]
    lines += [
        [seg("  403/401 bypass toolkit · authorized use only", "muted")],
        [],
        [seg("  baseline status="), seg("403", "red", True), seg("  0B")],
        [seg("  calib    status="), seg("404", "amber", True),
         seg("  233B   (path aleatorio, anti falso-positivo)", "muted")],
        [seg("  cod  tecnica   metodo   payload", "muted")],
        [seg("  200", "green", True), seg("  verb      OPTIONS  metodo OPTIONS      "),
         seg("<== BYPASS?", "green", True)],
        [seg("       curl -sk -X OPTIONS -H 'Cookie: sess=…' "
             "'https://target.tld/admin'", "muted")],
        [seg("  302", "cyan", True), seg("  path      GET      /admin/..;/         "),
         seg("<== BYPASS?", "green", True)],
        [seg("  403", "red", True), seg("  header-ip GET      X-Forwarded-For: 127.0.0.1")],
        [seg("  98 testes executados, "), seg("2 bypass(es) provavel(is)", "green", True)],
    ]
    return lines


def render(suffix):
    """SVG de terminal ANIMADO: revela as linhas em sequencia, em loop, com
    cursor piscando — mesmo mecanismo CSS do svg-term (asciinema)."""
    th = THEMES[suffix]
    lines = build_lines(th) + [[seg("$ ", "green", True)]]   # prompt final
    pad_x, bar_h, top, lh, fs = 26, 40, 16, 21, 14.5
    ch = fs * 0.6
    w = 900
    n = len(lines)
    h = bar_h + top + n * lh + top
    ti, hold = 0.20, 2.4            # s por linha + pausa no fim
    dur = n * ti + hold

    # CSS: cada linha segura opacity 0 ate seu instante e fica ate o fim; como
    # todas compartilham a mesma duracao e loop, a animacao reinicia junta.
    css = [".l{opacity:0;animation-duration:%.2fs;"
           "animation-iteration-count:infinite;"
           "animation-timing-function:steps(1,end)}" % dur]
    for i in range(1, n):   # a linha 0 (comando) fica sempre visivel
        p = (i * ti) / dur * 100.0
        p2 = min(p + 0.3, 99.8)
        css.append("@keyframes L%d{0%%{opacity:0}%.2f%%{opacity:0}"
                   "%.2f%%{opacity:1}100%%{opacity:1}}" % (i, p, p2))
    css.append("@keyframes blink{0%%{opacity:1}50%%{opacity:1}"
               "51%%{opacity:0}100%%{opacity:0}}")
    css.append(".cur{animation:blink 1s steps(1) infinite;fill:%s}" % th["cyan"])
    style = "<style>" + "".join(css) + "</style>"

    parts = [
        f'<svg xmlns="http://www.w3.org/2000/svg" width="{w}" height="{int(h)}" '
        f'viewBox="0 0 {w} {int(h)}" role="img" aria-label="403-bypass demo (animado)">',
        style,
        '<defs>'
        f'<linearGradient id="bg" x1="0" y1="0" x2="1" y2="1">'
        f'<stop offset="0" stop-color="{th["bg"]}"/><stop offset="1" stop-color="{th["bg2"]}"/></linearGradient>'
        f'<clipPath id="r"><rect width="{w}" height="{int(h)}" rx="12"/></clipPath>'
        '</defs>',
        f'<g clip-path="url(#r)">',
        f'<rect width="{w}" height="{int(h)}" fill="url(#bg)"/>',
        f'<rect width="{w}" height="{bar_h}" fill="{th["bar"]}"/>',
        f'<circle cx="24" cy="{bar_h/2:.0f}" r="6" fill="#FF5F56"/>',
        f'<circle cx="44" cy="{bar_h/2:.0f}" r="6" fill="#FFBD2E"/>',
        f'<circle cx="64" cy="{bar_h/2:.0f}" r="6" fill="#27C93F"/>',
        f'<text x="{w/2:.0f}" y="{bar_h/2+5:.0f}" text-anchor="middle" font-family="{MONO}" '
        f'font-size="13" fill="{th["muted"]}">403-bypass — zsh</text>',
    ]

    y = bar_h + top + fs
    for i, line in enumerate(lines):
        x = pad_x
        spans = []
        for text, color, bold in line:
            fill = th[color] if color else th["text"]
            weight = ' font-weight="700"' if bold else ""
            spans.append(f'<tspan x="{x:.1f}" fill="{fill}"{weight}>{escape(text)}</tspan>')
            x += len(text) * ch
        cls = "" if i == 0 else f' class="l" style="animation-name:L{i}"'
        parts.append(
            f'<text{cls} y="{y:.1f}" '
            f'font-family="{MONO}" font-size="{fs}" xml:space="preserve">'
            + "".join(spans) + '</text>')
        if i == n - 1:
            parts.append(f'<rect class="cur" x="{x+2:.1f}" y="{y-fs+3:.1f}" '
                         f'width="{ch*0.9:.1f}" height="{fs-1:.1f}"/>')
        y += lh
    parts.append(f'</g><rect x="0.5" y="0.5" width="{w-1}" height="{int(h)-1}" rx="12" '
                 f'fill="none" stroke="{th["border"]}"/>')
    parts.append('</svg>\n')
    return finish("".join(parts), {("JBM", 400), ("JBM", 700)})


if __name__ == "__main__":
    OUT.mkdir(exist_ok=True)
    for suffix in THEMES:
        name = f"demo{suffix}.svg"
        (OUT / name).write_text(render(suffix), encoding="utf-8")
        print("wrote", OUT / name)
