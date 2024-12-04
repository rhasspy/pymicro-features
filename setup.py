import sys
from pathlib import Path

# Available at setup time due to pyproject.toml
from pybind11.setup_helpers import Pybind11Extension
from setuptools import setup


# Omit `-std=c++` flags if we're building C source files with Clang
if sys.platform == "darwin":
    from distutils import unixccompiler

    class PatchedUnixCCompiler(unixccompiler.UnixCCompiler):
        def _compile(
            self, obj, src, ext, cc_args, extra_postargs, pp_opts, *args, **kwargs
        ):
            if self.compiler[0] == "clang" and ext == ".c":
                extra_postargs = [a for a in extra_postargs if "-std=c++" not in a]

            return super()._compile(
                obj, src, ext, cc_args, extra_postargs, pp_opts, *args, **kwargs
            )

    unixccompiler.UnixCCompiler = PatchedUnixCCompiler


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
        "filterbank.c",
        "filterbank_util.c",
        "frontend.c",
        "frontend_util.c",
        "log_lut.c",
        "log_scale.c",
        "log_scale_util.c",
        "noise_reduction.c",
        "noise_reduction_util.c",
        "pcan_gain_control.c",
        "pcan_gain_control_util.c",
        "window.c",
        "window_util.c",
    ]
)
sources.append(_KISSFFT_DIR / "kiss_fft.c")
sources.append(_KISSFFT_DIR / "tools" / "kiss_fftr.c")

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
    classifiers=["License :: OSI Approved :: MIT License"],
)
