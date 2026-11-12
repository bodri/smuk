"""Generate the SMUK precision wordmark as vectors and KiCad library graphics.

Standard library only. Run from any directory with Python 3.
"""
from pathlib import Path

ROOT = Path(__file__).resolve().parent
# Original geometric lettering. Each polygon is a solid foreground shape.
POLYGONS = [
    # Plus and minus: independent of any font or special-character rendering.
    [(20, 12), (28, 12), (28, 22), (38, 22), (38, 30), (28, 30), (28, 40), (20, 40), (20, 30), (10, 30), (10, 22), (20, 22)],
    [(10, 46), (38, 46), (38, 54), (10, 54)],
    # S
    [(70, 12), (96, 12), (96, 20), (72, 20), (72, 28), (90, 28), (96, 34), (96, 46), (90, 52), (64, 52), (64, 44), (88, 44), (88, 36), (70, 36), (64, 30), (64, 18)],
    # M
    [(108, 52), (108, 12), (116, 12), (126, 28), (136, 12), (144, 12), (144, 52), (136, 52), (136, 27), (126, 43), (116, 27), (116, 52)],
    # U
    [(156, 12), (164, 12), (164, 44), (180, 44), (180, 12), (188, 12), (188, 46), (182, 52), (162, 52), (156, 46)],
    # K
    [(200, 12), (208, 12), (208, 28), (224, 12), (235, 12), (215, 32), (235, 52), (224, 52), (208, 36), (208, 52), (200, 52)],
]


def svg(fill):
    paths = []
    for polygon in POLYGONS:
        d = 'M ' + ' L '.join(f'{x} {y}' for x, y in polygon) + ' Z'
        paths.append(f'  <path d="{d}"/>')
    return ('<svg xmlns="http://www.w3.org/2000/svg" width="24.4mm" height="6.4mm" '
            'viewBox="0 0 244 64" role="img" aria-labelledby="title desc">\n'
            '  <title id="title">SMUK precision wordmark</title>\n'
            '  <desc id="desc">A plus-minus symbol beside bold geometric SMUK lettering.</desc>\n'
            f'  <g fill="{fill}">\n' + '\n'.join(paths) + '\n  </g>\n</svg>\n')


def footprint(width):
    scale = width / 244
    polygons = []
    for polygon in POLYGONS:
        pts = ' '.join(f'(xy {(x - 122) * scale:.6f} {(y - 32) * scale:.6f})' for x, y in polygon)
        polygons.append(f'  (fp_poly (pts {pts}) (stroke (width 0) (type solid)) (fill solid) (layer "F.SilkS"))')
    return (f'(footprint "SMUK_Logo_{width:g}mm" (version 20240108) (generator "pcbnew")\n'
            '  (layer "F.Cu")\n'
            '  (descr "Original SMUK precision wordmark; monochrome silkscreen graphic")\n'
            '  (attr board_only exclude_from_pos_files exclude_from_bom)\n'
            '  (fp_text reference "REF**" (at 0 -4) (layer "F.SilkS") hide\n'
            '    (effects (font (size 1 1) (thickness 0.15))))\n'
            '  (fp_text value "SMUK" (at 0 4) (layer "F.Fab") hide\n'
            '    (effects (font (size 1 1) (thickness 0.15))))\n' + '\n'.join(polygons) + '\n)\n')


def symbol():
    shapes = []
    for polygon in POLYGONS:
        # Schematic coordinates have positive Y upward.
        pts = ' '.join(f'(xy {(x - 122) * .1:.6f} {(32 - y) * .1:.6f})' for x, y in polygon + [polygon[0]])
        shapes.append(f'      (polyline (pts {pts}) (stroke (width 0) (type default)) (fill (type outline)))')
    return ('(kicad_symbol_lib (version 20231120) (generator "kicad_symbol_editor")\n'
            '  (symbol "SMUK_Logo" (exclude_from_sim yes) (in_bom no) (on_board no)\n'
            '    (property "Reference" "#LOGO" (at 0 4 0) (effects (font (size 1.27 1.27)) hide))\n'
            '    (property "Value" "SMUK_Logo" (at 0 -4 0) (effects (font (size 1.27 1.27)) hide))\n'
            '    (symbol "SMUK_Logo_0_1"\n' + '\n'.join(shapes) + '\n    )\n  )\n)\n')


if __name__ == '__main__':
    (ROOT / 'smuk-logo.svg').write_text(svg('#000000'))
    (ROOT / 'smuk-logo-white.svg').write_text(svg('#ffffff'))
    library = ROOT / 'SMUK_Logos.pretty'
    library.mkdir(exist_ok=True)
    for width in (24.4, 16):
        (library / f'SMUK_Logo_{width:g}mm.kicad_mod').write_text(footprint(width))
    (ROOT / 'SMUK_Logos.kicad_sym').write_text(symbol())
