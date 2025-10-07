import statistics
import wave
from pathlib import Path

from syrupy import SnapshotAssertion

from pymicro_features import MicroFrontend

BYTES_PER_CHUNK = 160 * 2  # 10ms @ 16Khz (16-bit mono)

_DIR = Path(__file__).parent


def read_wav(file_name: str) -> bytes:
    """Return audio bytes from WAV file (16Khz, 16-bit mono)."""
    with wave.open(str(_DIR / file_name), "rb") as wav_file:
        assert wav_file.getframerate() == 16000
        assert wav_file.getsampwidth() == 2
        assert wav_file.getnchannels() == 1

        return wav_file.readframes(wav_file.getnframes())


def test_zeros() -> None:
    frontend = MicroFrontend()
    audio = bytes(BYTES_PER_CHUNK)

    # Need at least 30ms of audio before we get features
    output = frontend.process_samples(audio)
    assert output.samples_read == 160
    assert not output.features

    output = frontend.process_samples(audio)
    assert output.samples_read == 160
    assert not output.features

    output = frontend.process_samples(audio)
    assert output.samples_read == 160
    assert len(output.features) == 40

    # Now we get features for each 10ms chunk
    output = frontend.process_samples(audio)
    assert output.samples_read == 160
    assert len(output.features) == 40


def test_silence(snapshot: SnapshotAssertion) -> None:
    frontend = MicroFrontend()
    audio = read_wav("silence.wav")
    features = []

    i = 0
    while (i + BYTES_PER_CHUNK) < len(audio):
        chunk = audio[i : i + BYTES_PER_CHUNK]
        output = frontend.process_samples(chunk)
        features.extend(output.features)
        i += BYTES_PER_CHUNK

    assert features == snapshot()


def test_speech(snapshot: SnapshotAssertion) -> None:
    frontend = MicroFrontend()
    audio = read_wav("speech.wav")
    features = []

    i = 0
    while (i + BYTES_PER_CHUNK) < len(audio):
        chunk = audio[i : i + BYTES_PER_CHUNK]
        output = frontend.process_samples(chunk)
        features.extend(output.features)
        i += BYTES_PER_CHUNK

    assert features == snapshot()

def test_reset() -> None:
    frontend = MicroFrontend()
    audio = read_wav("speech.wav")

    features1 = []
    i = 0
    while (i + BYTES_PER_CHUNK) < len(audio):
        chunk = audio[i : i + BYTES_PER_CHUNK]
        output = frontend.process_samples(chunk)
        features1.extend(output.features)
        i += BYTES_PER_CHUNK

    # Without reset features will depend on previous state
    features2 = []
    i = 0
    while (i + BYTES_PER_CHUNK) < len(audio):
        chunk = audio[i : i + BYTES_PER_CHUNK]
        output = frontend.process_samples(chunk)
        features2.extend(output.features)
        i += BYTES_PER_CHUNK

    assert features1 != features2

    # With reset
    frontend.reset()
    features3 = []
    i = 0
    while (i + BYTES_PER_CHUNK) < len(audio):
        chunk = audio[i : i + BYTES_PER_CHUNK]
        output = frontend.process_samples(chunk)
        features3.extend(output.features)
        i += BYTES_PER_CHUNK

    assert features1 == features3
