#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include <iostream>

#include "tensorflow/lite/experimental/microfrontend/lib/frontend.h"
#include "tensorflow/lite/experimental/microfrontend/lib/frontend_util.h"

#define STRINGIFY(x) #x
#define MACRO_STRINGIFY(x) STRINGIFY(x)

static const uint8_t FEATURES_STEP_SIZE = 10;
static const uint8_t PREPROCESSOR_FEATURE_SIZE = 40;
static const uint8_t FEATURE_DURATION_MS = 30;
static const uint16_t AUDIO_SAMPLE_FREQUENCY = 16000;
static const uint16_t SAMPLES_PER_CHUNK =
    FEATURES_STEP_SIZE * (AUDIO_SAMPLE_FREQUENCY / 1000);

// 16-bit mono
static const uint16_t BYTES_PER_CHUNK = SAMPLES_PER_CHUNK * 2;

#define FLOAT32_SCALE 0.0390625

namespace py = pybind11;

// ----------------------------------------------------------------------------

struct ProcessOutput {
  std::vector<float> features;
  std::size_t samples_read;
};

class MicroFrontend {
private:
  FrontendConfig frontend_config;
  FrontendState frontend_state;

public:
  MicroFrontend();

  std::unique_ptr<ProcessOutput> ProcessSamples(py::bytes audio);
};

MicroFrontend::MicroFrontend() {
  this->frontend_config.window.size_ms = FEATURE_DURATION_MS;
  this->frontend_config.window.step_size_ms = FEATURES_STEP_SIZE;
  this->frontend_config.filterbank.num_channels = PREPROCESSOR_FEATURE_SIZE;
  this->frontend_config.filterbank.lower_band_limit = 125.0;
  this->frontend_config.filterbank.upper_band_limit = 7500.0;
  this->frontend_config.noise_reduction.smoothing_bits = 10;
  this->frontend_config.noise_reduction.even_smoothing = 0.025;
  this->frontend_config.noise_reduction.odd_smoothing = 0.06;
  this->frontend_config.noise_reduction.min_signal_remaining = 0.05;
  this->frontend_config.pcan_gain_control.enable_pcan = 1;
  this->frontend_config.pcan_gain_control.strength = 0.95;
  this->frontend_config.pcan_gain_control.offset = 80.0;
  this->frontend_config.pcan_gain_control.gain_bits = 21;
  this->frontend_config.log_scale.enable_log = 1;
  this->frontend_config.log_scale.scale_shift = 6;

  FrontendPopulateState(&(this->frontend_config), &(this->frontend_state),
                        AUDIO_SAMPLE_FREQUENCY);
}

std::unique_ptr<ProcessOutput> MicroFrontend::ProcessSamples(py::bytes audio) {
  auto output = std::make_unique<ProcessOutput>();

  py::buffer_info buffer_input(py::buffer(audio).request());
  int16_t *ptr_input = static_cast<int16_t *>(buffer_input.ptr);

  struct FrontendOutput frontend_output =
      FrontendProcessSamples(&(this->frontend_state), ptr_input,
                             SAMPLES_PER_CHUNK, &(output->samples_read));

  for (std::size_t i = 0; i < frontend_output.size; ++i) {
    output->features.push_back((float)frontend_output.values[i] *
                               FLOAT32_SCALE);
  }

  return output;
}

// ----------------------------------------------------------------------------

PYBIND11_MODULE(micro_features_cpp, m) {
  m.doc() = R"pbdoc(
        Speech features using TFLite Micro audio frontend
        -----------------------

        .. currentmodule:: micro_features_cpp

        .. autosummary::
           :toctree: _generate

           MicroFrontend
    )pbdoc";

  py::class_<MicroFrontend>(m, "MicroFrontend")
      .def(py::init<>())
      .def("ProcessSamples", &MicroFrontend::ProcessSamples);

  py::class_<ProcessOutput>(m, "ProcessOutput")
      .def_readonly("samples_read", &ProcessOutput::samples_read)
      .def_readonly("features", &ProcessOutput::features);

#ifdef VERSION_INFO
  m.attr("__version__") = MACRO_STRINGIFY(VERSION_INFO);
#else
  m.attr("__version__") = "dev";
#endif
}
