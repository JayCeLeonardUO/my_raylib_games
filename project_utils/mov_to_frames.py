#!/usr/bin/env python3
"""Extract frames from a .mov (or any video) to numbered PNGs with alpha keying."""
import argparse
import os
import numpy as np
import imageio.v3 as iio


def load_mask(mask_path, width, height):
    """Load a mask image and return it as a single-channel (H, W) uint8 array."""
    mask = iio.imread(mask_path)
    # If RGBA, drop alpha and use RGB luminance
    if mask.ndim == 3 and mask.shape[2] == 4:
        mask = mask[:, :, :3]
    # Convert RGB to grayscale via luminance
    if mask.ndim == 3:
        mask = np.dot(mask[:, :, :3], [0.2989, 0.5870, 0.1140]).astype(np.uint8)
    # Resize if mask dimensions don't match the frame
    if mask.shape[0] != height or mask.shape[1] != width:
        from PIL import Image
        mask_img = Image.fromarray(mask, mode="L")
        mask_img = mask_img.resize((width, height), Image.LANCZOS)
        mask = np.array(mask_img)
    return mask


def alpha_from_key_color(frame, key_color):
    """Derive alpha from distance to a key color. Key color = transparent, far from key = opaque."""
    rgb = frame[:, :, :3].astype(np.float32)
    key = np.array(key_color, dtype=np.float32)
    # Euclidean distance from key color, max possible = sqrt(255^2 * 3) ≈ 441.7
    dist = np.sqrt(np.sum((rgb - key) ** 2, axis=2))
    max_dist = np.sqrt(3.0) * 255.0
    alpha = np.clip(dist / max_dist * 255.0, 0, 255).astype(np.uint8)
    return alpha


def apply_alpha(frame, mask):
    """Return an RGBA frame with the mask applied as the alpha channel."""
    h, w = frame.shape[:2]
    if frame.shape[2] == 4:
        rgba = frame.copy()
    else:
        rgba = np.dstack([frame, np.full((h, w), 255, dtype=np.uint8)])
    rgba[:, :, 3] = mask
    return rgba


def parse_color(s):
    """Parse a color string like '255,255,255' into an (R, G, B) tuple."""
    parts = [int(x.strip()) for x in s.split(",")]
    if len(parts) != 3 or not all(0 <= c <= 255 for c in parts):
        raise argparse.ArgumentTypeError(f"Invalid color: '{s}'. Use R,G,B format (e.g. 255,255,255)")
    return tuple(parts)


def main():
    parser = argparse.ArgumentParser(description="Extract video frames to PNGs with alpha keying")
    parser.add_argument("input", help="Path to .mov or video file")
    parser.add_argument("-o", "--output", help="Output directory (default: assets/animations/<stem>)")
    parser.add_argument("--skip", type=int, default=1, help="Keep every Nth frame (default: 1 = all)")
    parser.add_argument(
        "--mask",
        help="Path to a mask image whose brightness becomes the alpha channel. "
             "White = opaque, black = transparent. Overrides color keying.",
    )
    parser.add_argument(
        "--key-color", type=parse_color, default=(255, 255, 255),
        help="Color to key out as transparent (default: 255,255,255 = white). "
             "Pixels matching this color become transparent; pixels far from it become opaque.",
    )
    parser.add_argument(
        "--no-key", action="store_true",
        help="Disable color keying. Output fully opaque frames.",
    )
    args = parser.parse_args()

    stem = os.path.splitext(os.path.basename(args.input))[0]
    out_dir = args.output or os.path.join("assets", "animations", stem)
    os.makedirs(out_dir, exist_ok=True)

    frames = iio.imread(args.input, plugin="pyav")
    saved = 0
    ext_mask = None  # external mask, loaded once
    for i, frame in enumerate(frames):
        if i % args.skip != 0:
            continue
        h, w = frame.shape[:2]

        if args.mask:
            # External mask image: load once, reuse for all frames
            if ext_mask is None:
                ext_mask = load_mask(args.mask, w, h)
                print(f"Using mask image: {args.mask}")
            mask = ext_mask
        elif args.no_key:
            mask = np.full((h, w), 255, dtype=np.uint8)
        else:
            # Default: key out the key color (white by default)
            mask = alpha_from_key_color(frame, args.key_color)

        out_path = os.path.join(out_dir, f"frame_{saved:04d}.png")
        iio.imwrite(out_path, apply_alpha(frame, mask))
        saved += 1

    mode = "mask image" if args.mask else ("no keying (opaque)" if args.no_key else f"key color {args.key_color}")
    print(f"Extracted {saved} frames (RGBA, {mode}) to {out_dir}/")


if __name__ == "__main__":
    main()
