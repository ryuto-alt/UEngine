#!/usr/bin/env python3
"""
Texture Atlas Generator for Flipbook Animation
Creates a single sprite sheet from individual frame images.
"""

import os
import sys
import math
from PIL import Image
import argparse

def create_atlas(input_dir: str, output_path: str, columns: int = None):
    """
    Create a texture atlas from images in a directory.

    Args:
        input_dir: Directory containing frame images
        output_path: Output atlas file path
        columns: Number of columns (auto-calculated if None)
    """
    # Get all image files
    extensions = {'.png', '.jpg', '.jpeg', '.bmp', '.tga'}
    files = []
    for f in sorted(os.listdir(input_dir)):
        ext = os.path.splitext(f)[1].lower()
        if ext in extensions:
            files.append(os.path.join(input_dir, f))

    if not files:
        print(f"Error: No image files found in {input_dir}")
        return False

    print(f"Found {len(files)} frames")

    # Load first image to get dimensions
    first_img = Image.open(files[0])
    frame_width, frame_height = first_img.size
    print(f"Frame size: {frame_width}x{frame_height}")

    # Calculate grid dimensions
    num_frames = len(files)
    if columns is None:
        # Auto-calculate: prefer square-ish layout
        columns = math.ceil(math.sqrt(num_frames))
    rows = math.ceil(num_frames / columns)

    print(f"Atlas grid: {columns}x{rows} ({columns * rows} slots for {num_frames} frames)")

    # Create atlas image
    atlas_width = columns * frame_width
    atlas_height = rows * frame_height
    atlas = Image.new('RGBA', (atlas_width, atlas_height), (0, 0, 0, 0))

    # Place frames
    for i, file_path in enumerate(files):
        img = Image.open(file_path).convert('RGBA')

        # Resize if necessary
        if img.size != (frame_width, frame_height):
            print(f"Warning: Resizing {os.path.basename(file_path)} from {img.size} to {frame_width}x{frame_height}")
            img = img.resize((frame_width, frame_height), Image.LANCZOS)

        # Calculate position (top-left to bottom-right, row by row)
        col = i % columns
        row = i // columns
        x = col * frame_width
        y = row * frame_height

        atlas.paste(img, (x, y))

    # Save atlas
    atlas.save(output_path)
    print(f"Created atlas: {output_path}")
    print(f"Atlas size: {atlas_width}x{atlas_height}")
    print(f"Use these settings in particle editor:")
    print(f"  Tiles X: {columns}")
    print(f"  Tiles Y: {rows}")
    print(f"  Frame Count: {num_frames}")

    return True

def main():
    parser = argparse.ArgumentParser(description='Create texture atlas from frame images')
    parser.add_argument('input_dir', help='Directory containing frame images')
    parser.add_argument('-o', '--output', help='Output atlas file path')
    parser.add_argument('-c', '--columns', type=int, help='Number of columns (auto if not specified)')

    args = parser.parse_args()

    if not os.path.isdir(args.input_dir):
        print(f"Error: {args.input_dir} is not a directory")
        return 1

    # Default output path
    if args.output is None:
        dir_name = os.path.basename(os.path.normpath(args.input_dir))
        args.output = os.path.join(args.input_dir, f"{dir_name}_atlas.png")

    if create_atlas(args.input_dir, args.output, args.columns):
        return 0
    return 1

if __name__ == '__main__':
    sys.exit(main())
