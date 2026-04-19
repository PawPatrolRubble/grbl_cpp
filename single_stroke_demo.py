"""
Demo: Convert normal Chinese font glyphs to single-stroke centerline paths.

Pipeline:
  1. Render TTF glyph at high resolution (white on black)
  2. Zhang-Suen thinning → 1px skeleton
  3. Trace skeleton pixels into polyline paths
  4. Simplify paths with Ramer-Douglas-Peucker

Output: side-by-side comparison image showing original vs single-stroke for several characters.
"""

import numpy as np
import cv2
from PIL import Image, ImageDraw, ImageFont
from collections import deque
import os

# ─── Settings ───────────────────────────────────────────────────
CHARS = "永你好中国龙"  # test characters ('永' has all 8 stroke types)
GLYPH_SIZE = 256         # render resolution per glyph
FONT_SIZE = 220          # TTF font size
OUTPUT_PATH = os.path.join(os.path.dirname(__file__), "single_stroke_demo.png")

# Try common Chinese fonts on Windows
FONT_CANDIDATES = [
    "C:/Windows/Fonts/simhei.ttf",    # SimHei (黑体)
    "C:/Windows/Fonts/simsun.ttc",     # SimSun (宋体)
    "C:/Windows/Fonts/msyh.ttc",       # Microsoft YaHei
    "C:/Windows/Fonts/simkai.ttf",     # KaiTi (楷体)
]


def find_font():
    for path in FONT_CANDIDATES:
        if os.path.exists(path):
            return path
    raise FileNotFoundError("No Chinese font found. Install SimHei or SimSun.")


def render_glyph(char: str, font: ImageFont.FreeTypeFont, size: int) -> np.ndarray:
    """Render a single character to a binary numpy array (white glyph on black)."""
    img = Image.new("L", (size, size), 0)
    draw = ImageDraw.Draw(img)
    bbox = draw.textbbox((0, 0), char, font=font)
    w, h = bbox[2] - bbox[0], bbox[3] - bbox[1]
    x = (size - w) // 2 - bbox[0]
    y = (size - h) // 2 - bbox[1]
    draw.text((x, y), char, fill=255, font=font)
    return np.array(img)


def thin_glyph(binary: np.ndarray) -> np.ndarray:
    """Apply Zhang-Suen thinning to get 1px skeleton."""
    # Threshold to pure binary
    _, bw = cv2.threshold(binary, 128, 255, cv2.THRESH_BINARY)
    # Thinning
    skeleton = cv2.ximgproc.thinning(bw, thinningType=cv2.ximgproc.THINNING_ZHANGSUEN)
    return skeleton


def trace_skeleton(skeleton: np.ndarray) -> list:
    """Trace skeleton pixels into polyline paths using connected-component walking."""
    h, w = skeleton.shape
    visited = np.zeros((h, w), dtype=bool)
    paths = []

    # Find all skeleton pixels
    skel_pts = set(zip(*np.where(skeleton > 0)))

    # 8-connected neighbors
    NEIGHBORS = [(-1, -1), (-1, 0), (-1, 1), (0, -1), (0, 1), (1, -1), (1, 0), (1, 1)]

    def get_neighbors(r, c):
        result = []
        for dr, dc in NEIGHBORS:
            nr, nc = r + dr, c + dc
            if 0 <= nr < h and 0 <= nc < w and (nr, nc) in skel_pts and not visited[nr, nc]:
                result.append((nr, nc))
        return result

    # Find endpoints and junctions
    def degree(r, c):
        count = 0
        for dr, dc in NEIGHBORS:
            nr, nc = r + dr, c + dc
            if (nr, nc) in skel_pts:
                count += 1
        return count

    # Start tracing from endpoints (degree 1) first, then any unvisited pixel
    endpoints = [(r, c) for r, c in skel_pts if degree(r, c) == 1]
    start_candidates = endpoints + list(skel_pts)

    for start in start_candidates:
        r, c = start
        if visited[r, c]:
            continue

        # Trace a path from this point
        path = [(c, r)]  # store as (x, y)
        visited[r, c] = True
        current = (r, c)

        while True:
            neighbors = get_neighbors(*current)
            if not neighbors:
                break
            # Pick the neighbor (prefer continuing in same direction)
            next_pt = neighbors[0]
            visited[next_pt[0], next_pt[1]] = True
            path.append((next_pt[1], next_pt[0]))  # (x, y)
            current = next_pt

        if len(path) >= 2:
            paths.append(np.array(path, dtype=np.float32))

    return paths


def simplify_paths(paths: list, epsilon: float = 1.5) -> list:
    """Simplify polylines with Ramer-Douglas-Peucker algorithm."""
    simplified = []
    for path in paths:
        if len(path) < 3:
            simplified.append(path)
            continue
        # cv2.approxPolyDP expects specific shape
        pts = path.reshape(-1, 1, 2)
        approx = cv2.approxPolyDP(pts, epsilon, closed=False)
        simplified.append(approx.reshape(-1, 2))
    return simplified


def draw_paths_on_image(img: np.ndarray, paths: list, color=(0, 200, 255), thickness=2):
    """Draw polyline paths on a color image."""
    for path in paths:
        pts = path.astype(np.int32)
        for i in range(len(pts) - 1):
            cv2.line(img, tuple(pts[i]), tuple(pts[i + 1]), color, thickness, cv2.LINE_AA)
        # Draw small circles at path endpoints
        if len(pts) >= 1:
            cv2.circle(img, tuple(pts[0]), 3, (0, 255, 0), -1, cv2.LINE_AA)
            cv2.circle(img, tuple(pts[-1]), 3, (0, 0, 255), -1, cv2.LINE_AA)


def main():
    font_path = find_font()
    print(f"Using font: {font_path}")
    font = ImageFont.truetype(font_path, FONT_SIZE)

    n = len(CHARS)
    # Layout: 4 rows x n columns
    # Row 0: Original glyph
    # Row 1: Skeleton (thinned)
    # Row 2: Traced paths (colored)
    # Row 3: Simplified paths on clean background
    rows = 4
    margin = 4
    cell = GLYPH_SIZE + margin
    canvas_w = cell * n + margin
    canvas_h = cell * rows + margin + 40  # extra for labels
    canvas = np.zeros((canvas_h, canvas_w, 3), dtype=np.uint8)

    # Row labels
    labels = ["Original", "Skeleton", "Traced Paths", "Simplified"]

    for i, char in enumerate(CHARS):
        print(f"Processing '{char}'...")
        x0 = margin + i * cell
        glyph = render_glyph(char, font, GLYPH_SIZE)

        # Row 0: Original
        y0 = margin + 30
        glyph_rgb = cv2.cvtColor(glyph, cv2.COLOR_GRAY2BGR)
        canvas[y0:y0 + GLYPH_SIZE, x0:x0 + GLYPH_SIZE] = glyph_rgb

        # Thin
        skeleton = thin_glyph(glyph)

        # Row 1: Skeleton
        y1 = y0 + cell
        skel_rgb = cv2.cvtColor(skeleton, cv2.COLOR_GRAY2BGR)
        canvas[y1:y1 + GLYPH_SIZE, x0:x0 + GLYPH_SIZE] = skel_rgb

        # Trace paths
        paths = trace_skeleton(skeleton)
        total_pts = sum(len(p) for p in paths)

        # Row 2: Traced paths
        y2 = y1 + cell
        traced_img = np.zeros((GLYPH_SIZE, GLYPH_SIZE, 3), dtype=np.uint8)
        draw_paths_on_image(traced_img, paths, color=(0, 200, 255), thickness=2)
        canvas[y2:y2 + GLYPH_SIZE, x0:x0 + GLYPH_SIZE] = traced_img

        # Simplify
        simplified = simplify_paths(paths, epsilon=2.0)
        simp_pts = sum(len(p) for p in simplified)

        # Row 3: Simplified
        y3 = y2 + cell
        simp_img = np.zeros((GLYPH_SIZE, GLYPH_SIZE, 3), dtype=np.uint8)
        draw_paths_on_image(simp_img, simplified, color=(255, 180, 0), thickness=2)
        canvas[y3:y3 + GLYPH_SIZE, x0:x0 + GLYPH_SIZE] = simp_img

        print(f"  '{char}': {len(paths)} paths, {total_pts} pts -> {simp_pts} pts simplified")

    # Add row labels on the left side
    for r, label in enumerate(labels):
        y = margin + 30 + r * cell + GLYPH_SIZE // 2
        cv2.putText(canvas, label, (margin + 5, margin + 25 + r * cell - 2),
                    cv2.FONT_HERSHEY_SIMPLEX, 0.5, (180, 180, 180), 1, cv2.LINE_AA)

    # Add column labels (characters)
    for i, char in enumerate(CHARS):
        x = margin + i * cell + GLYPH_SIZE // 2 - 8
        # Can't render Chinese in OpenCV easily, use index
        cv2.putText(canvas, f"[{i}]", (x, margin + 20),
                    cv2.FONT_HERSHEY_SIMPLEX, 0.5, (180, 180, 180), 1, cv2.LINE_AA)

    cv2.imwrite(OUTPUT_PATH, canvas)
    print(f"\nOutput saved to: {OUTPUT_PATH}")
    print(f"Characters: {CHARS}")


if __name__ == "__main__":
    main()
