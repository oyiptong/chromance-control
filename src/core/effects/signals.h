#pragma once

#include <stdint.h>

namespace chromance {
namespace core {

// Runtime modulation signals (audio/BPM/etc). Defaults represent "not provided".
struct Signals {
  bool has_bpm = false;
  float bpm = 0.0f;

  bool has_energy = false;
  float energy_01 = 0.0f;  // 0..1

  bool has_beat_phase = false;
  float beat_phase_01 = 0.0f;  // 0..1
};

// Raw inputs that may feed a ModulationProvider. Kept minimal for now.
struct ModulationInputs {
  bool has_bpm = false;
  float bpm = 0.0f;

  bool has_energy = false;
  float energy_01 = 0.0f;
};

}  // namespace core
}  // namespace chromance

