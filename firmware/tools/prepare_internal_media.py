#!/usr/bin/env python3
"""Create small synthetic fixtures for the 1.75C internal media filesystem."""

import argparse
from pathlib import Path

from generate_sd_test_media import write_wav, write_photos, write_mjpeg_pcm_avi


def prepare(destination: Path) -> None:
    for name in ("music", "pictures", "video", "recordings", "history", "diagnostics"):
        (destination / name).mkdir(parents=True, exist_ok=True)
    # Original synthetic fixtures; no third-party music or images.
    write_wav(destination / "music" / "channel-test.wav", seconds=2)
    write_photos(destination / "pictures")
    write_mjpeg_pcm_avi(destination / "video" / "color-test.avi")
    (destination / "README.txt").write_text(
        "1.75C internal media\n"
        "music: MP3 or WAV; pictures: baseline JPEG; video: MJPEG AVI.\n"
        "recordings, history and diagnostics contain user-created data.\n"
        "These bundled audio, image and video fixtures are synthetic.\n",
        encoding="utf-8",
    )


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("destination", type=Path)
    args = parser.parse_args()
    prepare(args.destination)
