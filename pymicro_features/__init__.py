"""Speech features using TFLite Micro audio frontend."""

from dataclasses import dataclass
from typing import List

# pylint: disable=no-name-in-module
from micro_features_cpp import create_frontend, process_samples, reset_frontend


@dataclass
class MicroFrontendOutput:
    """Output from ProcessSamples."""

    features: List[float]
    samples_read: int


class MicroFrontend:
    """TFLite Micro audio frontend."""

    def __init__(self) -> None:
        """Initialize frontend."""
        self._frontend = create_frontend()

    def process_samples(self, audio: bytes) -> MicroFrontendOutput:
        """Process 16Khz 16-bit audio samples."""
        features, samples_read = process_samples(self._frontend, audio)
        return MicroFrontendOutput(features, samples_read)

    def reset(self) -> None:
        """Reset state."""
        reset_frontend(self._frontend)
