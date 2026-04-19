"""
Demo: Compare single-stroke extraction across multiple Chinese font styles.

Pipeline per glyph:
  1. Render TTF glyph (white on black)
  2. Zhang-Suen thinning → 1px skeleton
  3. Trace & simplify with RDP

Output: grid image — rows = fonts, columns = characters.
Each cell shows original glyph (gray) with single-stroke overlay (colored).
"""

import numpy as np
import cv2
from PIL import Image, ImageDraw, ImageFont
import os

CHARS = "永你好中龙"
GLYPH_SIZE = 200
FONT_SIZE = 170
OUTPUT_PATH = os.path.join(os.path.dirname(__file__), "single_stroke_fonts_compare.png")

FONTS = [
    ("C:/Windows/Fonts/simhei.ttf", "SimHei (黑体)"),
    ("C:/Windows/Fonts/simkai.ttf", "KaiTi (楷体)"),
    ("C:/Windows/Fonts/simsun.ttc", "SimSun (宋体)"),
    ("C:/Windows/Fonts/simfang.ttf", "FangSong (仿宋)"),
    ("C:/Windows/Fonts/msyh.ttc", "YaHei (雅黑)"),
]

NEIGHBORS = [(-1, -1), (-1, 0), (-1, 1), (0, -1), (0, 1), (1, -1), (1, 0), (1, 1)]


def render_glyph(char, font, size):
    img = Image.new("L", (size, size), 0)
    draw = ImageDraw.Draw(img)
    bbox = draw.textbbox((0, 0), char, font=font)
    w, h = bbox[2] - bbox[0], bbox[3] - bbox[1]
    x = (size - w) // 2 - bbox[0]
    y = (size - h) // 2 - bbox[1]
    draw.text((x, y), char, fill=255, font=font)
    return np.array(img)


def thin_glyph(binary):
    _, bw = cv2.threshold(binary, 128, 255, cv2.THRESH_BINARY)
    return cv2.ximgproc.thinning(bw, thinningType=cv2.ximgproc.THINNING_ZHANGSUEN)


def trace_skeleton(skeleton):
    h, w = skeleton.shape
    visited = np.zeros((h, w), dtype=bool)
    paths = []
    skel_pts = set(zip(*np.where(skeleton > 0)))

    def get_neighbors(r, c):
        result = []
        for dr, dc in NEIGHBORS:
            nr, nc = r + dr, c + dc
            if 0 <= nr < h and 0 <= nc < w and (nr, nc) in skel_pts and not visited[nr, nc]:
                result.append((nr, nc))
        return result

    def degree(r, c):
        return sum(1 for dr, dc in NEIGHBORS if (r+dr, c+dc) in skel_pts)

    endpoints = [(r, c) for r, c in skel_pts if degree(r, c) == 1]
    for start in endpoints + list(skel_pts):
        r, c = start
        if visited[r, c]:
            continue
        path = [(c, r)]
        visited[r, c] = True
        current = (r, c)
        while True:
            neighbors = get_neighbors(*current)
            if not neighbors:
                break
            next_pt = neighbors[0]
            visited[next_pt[0], next_pt[1]] = True
            path.append((next_pt[1], next_pt[0]))
            current = next_pt
        if len(path) >= 2:
            paths.append(np.array(path, dtype=np.float32))
    return paths


def simplify_paths(paths, epsilon=2.0):
    result = []
    for path in paths:
        if len(path) < 3:
            result.append(path)
            continue
        pts = path.reshape(-1, 1, 2)
        approx = cv2.approxPolyDP(pts, epsilon, closed=False)
        result.append(approx.reshape(-1, 2))
    return result


def draw_paths(img, paths, color, thickness=2):
    for path in paths:
        pts = path.astype(np.int32)
        for i in range(len(pts) - 1):
            cv2.line(img, tuple(pts[i]), tuple(pts[i+1]), color, thickness, cv2.LINE_AA)


def process_glyph(char, font):
    glyph = render_glyph(char, font, GLYPH_SIZE)
    skeleton = thin_glyph(glyph)
    paths = trace_skeleton(skeleton)
    simplified = simplify_paths(paths, epsilon=2.0)

    # Create composite: dim original + bright single-stroke
    cell = np.zeros((GLYPH_SIZE, GLYPH_SIZE, 3), dtype=np.uint8)
    # Original as dim background
    gray = (glyph * 0.25).astype(np.uint8)
    cell[:, :, 0] = gray
    cell[:, :, 1] = gray
    cell[:, :, 2] = gray
    # Overlay single-stroke paths
    draw_paths(cell, simplified, color=(0, 220, 255), thickness=2)

    n_paths = len(simplified)
    n_pts = sum(len(p) for p in simplified)
    return cell, n_paths, n_pts


def main():
    available_fonts = [(p, n) for p, n in FONTS if os.path.exists(p)]
    n_fonts = len(available_fonts)
    n_chars = len(CHARS)

    margin = 4
    label_w = 160  # width for font name labels
    cell = GLYPH_SIZE + margin
    canvas_w = label_w + cell * n_chars + margin
    canvas_h = 30 + cell * n_fonts + margin
    canvas = np.zeros((canvas_h, canvas_w, 3), dtype=np.uint8)

    # Column headers
    for j, char in enumerate(CHARS):
        x = label_w + margin + j * cell + GLYPH_SIZE // 2 - 5
        cv2.putText(canvas, f"[{j}]", (x, 20),
                    cv2.FONT_HERSHEY_SIMPLEX, 0.5, (180, 180, 180), 1, cv2.LINE_AA)

    for i, (font_path, font_name) in enumerate(available_fonts):
        print(f"Processing font: {font_name}")
        font = ImageFont.truetype(font_path, FONT_SIZE)
        y0 = 30 + i * cell

        # Row label
        cv2.putText(canvas, font_name.split("(")[0].strip(), (5, y0 + GLYPH_SIZE // 2 - 10),
                    cv2.FONT_HERSHEY_SIMPLEX, 0.5, (200, 200, 200), 1, cv2.LINE_AA)
        # Second line: Chinese name
        short = font_name.split("(")[1].rstrip(")") if "(" in font_name else ""
        if short:
            # Render Chinese name using PIL
            label_img = Image.new("RGB", (label_w - 10, 24), (0, 0, 0))
            label_draw = ImageDraw.Draw(label_img)
            try:
                small_font = ImageFont.truetype(font_path, 18)
                label_draw.text((0, 0), short, fill=(180, 180, 180), font=small_font)
            except:
                pass
            label_np = np.array(label_img)
            ly = y0 + GLYPH_SIZE // 2 + 5
            if ly + 24 <= canvas_h and label_w - 10 > 0:
                canvas[ly:ly+24, 5:5+label_w-10] = label_np

        for j, char in enumerate(CHARS):
            x0 = label_w + margin + j * cell
            cell_img, n_paths, n_pts = process_glyph(char, font)
            canvas[y0:y0+GLYPH_SIZE, x0:x0+GLYPH_SIZE] = cell_img
            # Stats overlay
            cv2.putText(cell_img, f"{n_paths}p/{n_pts}pt", (3, GLYPH_SIZE - 5),
                        cv2.FONT_HERSHEY_SIMPLEX, 0.35, (100, 100, 100), 1, cv2.LINE_AA)
            canvas[y0:y0+GLYPH_SIZE, x0:x0+GLYPH_SIZE] = cell_img

    cv2.imwrite(OUTPUT_PATH, canvas)
    print(f"\nOutput saved to: {OUTPUT_PATH}")
    print(f"Characters: {CHARS}")
    print("Fonts tested:", [n for _, n in available_fonts])


if __name__ == "__main__":
    main()
