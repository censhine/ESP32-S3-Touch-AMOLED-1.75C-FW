#!/usr/bin/env python3
"""Host regression tests: python3 -m unittest discover -s firmware/components/MusicPlayer/tests."""

import ctypes
import io
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
import unittest
import wave


class Watch(ctypes.Structure):
    _fields_ = [("request_tick", ctypes.c_uint32), ("waiting_for_start", ctypes.c_bool)]


class MusicPlayerSupportTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.workspace = tempfile.TemporaryDirectory()
        cls.root = Path(cls.workspace.name)
        source = Path(__file__).resolve().parents[1] / "gui_music/music_player_support.c"
        library = cls.root / "music_player_support.so"
        link_flags = ["-dynamiclib"] if sys.platform == "darwin" else ["-shared", "-fPIC"]
        subprocess.run(["cc", "-std=c11", "-Wall", "-Wextra", "-Werror", *link_flags,
                        str(source), "-o", str(library)], check=True)
        cls.lib = ctypes.CDLL(str(library))
        cls.lib.music_playback_watch_start.argtypes = [ctypes.POINTER(Watch), ctypes.c_uint32]
        cls.lib.music_playback_watch_finished.argtypes = [ctypes.POINTER(Watch), ctypes.c_uint32,
                                                         ctypes.c_bool, ctypes.c_bool]
        cls.lib.music_playback_watch_finished.restype = ctypes.c_bool
        cls.lib.music_wav_duration_seconds.argtypes = [ctypes.c_char_p]
        cls.lib.music_wav_duration_seconds.restype = ctypes.c_uint32

    @classmethod
    def tearDownClass(cls):
        cls.workspace.cleanup()

    def start(self, now=0):
        watch = Watch()
        self.lib.music_playback_watch_start(ctypes.byref(watch), now)
        return watch

    def finished(self, watch, now, active=False, shutdown=False):
        return self.lib.music_playback_watch_finished(ctypes.byref(watch), now, active, shutdown)

    def test_queued_start_is_not_mistaken_for_eof(self):
        watch = self.start(100)
        for now in (100, 200, 400):
            self.assertFalse(self.finished(watch, now))
        self.assertFalse(self.finished(watch, 500, active=True))
        self.assertTrue(self.finished(watch, 2400))

    def test_pause_and_resume_keep_session_alive_then_eof_stops(self):
        watch = self.start()
        # PLAYING and PAUSE both retain an active player session.
        self.assertFalse(self.finished(watch, 100, active=True))
        self.assertFalse(self.finished(watch, 10000, active=True))
        self.lib.music_playback_watch_start(ctypes.byref(watch), 10000)
        self.assertFalse(self.finished(watch, 10100, active=True))
        self.assertTrue(self.finished(watch, 11000))

    def test_queued_switch_can_stay_playing_without_idle_transition(self):
        watch = self.start()
        self.assertFalse(self.finished(watch, 100, active=True))
        self.lib.music_playback_watch_start(ctypes.byref(watch), 200)
        self.assertFalse(self.finished(watch, 300, active=True))
        self.assertTrue(self.finished(watch, 400))

    def test_short_or_invalid_file_finishes_even_when_active_poll_was_missed(self):
        watch = self.start(50)
        self.assertFalse(self.finished(watch, 1049))
        self.assertTrue(self.finished(watch, 1050))

    def test_shutdown_is_immediate_and_tick_wrap_is_safe(self):
        self.assertTrue(self.finished(self.start(), 1, shutdown=True))
        watch = self.start(0xFFFFFF00)
        self.assertFalse(self.finished(watch, 0x000002E7))
        self.assertTrue(self.finished(watch, 0x000002E8))

    def duration(self, data):
        path = self.root / "fixture.wav"
        path.write_bytes(data)
        return self.lib.music_wav_duration_seconds(bytes(path))

    @staticmethod
    def wav(frames=88200, channels=2, sample_rate=44100):
        output = io.BytesIO()
        with wave.open(output, "wb") as wav:
            wav.setnchannels(channels)
            wav.setsampwidth(2)
            wav.setframerate(sample_rate)
            wav.writeframes(b"\0" * frames * channels * 2)
        return output.getvalue()

    def test_two_second_fixture_has_real_duration(self):
        self.assertEqual(self.duration(self.wav()), 2)
        self.assertEqual(self.duration(self.wav(frames=1)), 1)
        self.assertEqual(self.duration(self.wav(frames=24000, channels=1, sample_rate=24000)), 1)

    def test_odd_metadata_chunk_is_skipped_with_padding(self):
        data = self.wav()
        body = b"WAVE" + b"JUNK" + struct.pack("<I", 3) + b"abc\0" + data[12:]
        self.assertEqual(self.duration(b"RIFF" + struct.pack("<I", len(body)) + body), 2)

    def test_unknown_and_truncated_files_do_not_invent_duration(self):
        for data in (b"ID3 music", b"", self.wav()[:40], self.wav()[:-1]):
            with self.subTest(size=len(data)):
                self.assertEqual(self.duration(data), 0)
        self.assertEqual(self.lib.music_wav_duration_seconds(b"/no/such/music.wav"), 0)

    def test_invalid_pcm_format_and_chunk_lengths_are_rejected(self):
        original = self.wav()
        for offset, replacement in ((20, b"\x03\x00"), (22, b"\0\0"),
                                    (32, b"\0\0"), (28, b"\0\0\0\0"),
                                    (40, b"\xff\xff\xff\xff")):
            data = bytearray(original)
            data[offset:offset + len(replacement)] = replacement
            with self.subTest(offset=offset):
                self.assertEqual(self.duration(data), 0)


if __name__ == "__main__":
    unittest.main()
