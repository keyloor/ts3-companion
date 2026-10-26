"""Genera desktop/icon.ico (estrella dorada sobre cielo azul) para el exe. Requiere Pillow."""
import math, os
from PIL import Image, ImageDraw

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
S = 256


def star(cx, cy, r_out, r_in):
    pts = []
    for i in range(10):
        a = -math.pi / 2 + i * math.pi / 5
        r = r_out if i % 2 == 0 else r_in
        pts.append((cx + r * math.cos(a), cy + r * math.sin(a)))
    return pts


img = Image.new('RGBA', (S, S), (0, 0, 0, 0))
d = ImageDraw.Draw(img)
d.rounded_rectangle((8, 8, S - 8, S - 8), radius=52, fill=(36, 112, 214), outline=(20, 64, 140), width=8)
d.polygon(star(S / 2, S / 2 + 10, 104, 44), fill=(20, 64, 140))          # sombra
d.polygon(star(S / 2, S / 2 + 2, 100, 42), fill=(255, 196, 0), outline=(150, 92, 0), width=6)
img.save(os.path.join(ROOT, 'desktop', 'icon.ico'), sizes=[(16, 16), (24, 24), (32, 32), (48, 48), (64, 64), (128, 128), (256, 256)])
img.resize((192, 192), Image.LANCZOS).save(os.path.join(ROOT, 'app', 'icon.png'))
print('icon.ico + app/icon.png')
