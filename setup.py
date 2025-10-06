from pathlib import Path

# Available at setup time due to pyproject.toml
from pybind11.setup_helpers import Pybind11Extension, build_ext
from setuptools import setup

_DIR = Path(__file__).parent
_FRONTEND_DIR = _DIR / "tensorflow" / "lite" / "experimental" / "microfrontend" / "lib"
_KISSFFT_DIR = _DIR / "kissfft"
_INCLUDE_DIR = _DIR

__version__ = "1.0.0"

sources = []
sources.extend(
    _FRONTEND_DIR / f
    for f in [
        "kiss_fft_int16.cc",
        "fft.cc",
        "fft_util.cc",
        "filterbank.cc",
        "filterbank_util.cc",
        "frontend.cc",
        "frontend_util.cc",
        "log_lut.cc",
        "log_scale.cc",
        "log_scale_util.cc",
        "noise_reduction.cc",
        "noise_reduction_util.cc",
        "pcan_gain_control.cc",
        "pcan_gain_control_util.cc",
        "window.cc",
        "window_util.cc",
    ]
)
sources.append(_KISSFFT_DIR / "kiss_fft.cc")
sources.append(_KISSFFT_DIR / "tools" / "kiss_fftr.cc")

flags = ["-DFIXED_POINT=16"]
ext_modules = [
    Pybind11Extension(
        name="micro_features_cpp",
        language="c++",
        cxx_std=17,
        extra_compile_args=flags,
        sources=sorted([str(p) for p in sources] + [str(_DIR / "python.cpp")]),
        define_macros=[("VERSION_INFO", __version__)],
        include_dirs=[str(_INCLUDE_DIR), str(_KISSFFT_DIR)],
    ),
]


setup(
    name="pymicro_features",
    version=__version__,
    author="Michael Hansen",
    author_email="mike@rhasspy.org",
    url="https://github.com/rhasspy/pymicro-features",
    description="Speech features using TFLite Micro audio frontend",
    long_description="",
    packages=["pymicro_features"],
    ext_modules=ext_modules,
    zip_safe=False,
    python_requires=">=3.7",
    classifiers=["License :: OSI Approved :: Apache Software License"],
    extras_require={"dev": ["pytest", "pybind11", "syrupy", "build"]},
)
