#include <unity.h>

#include "core/effects/modulation_provider.h"

using chromance::core::NullModulationProvider;
using chromance::core::Signals;

void test_null_modulation_provider_returns_defaults() {
  NullModulationProvider p;
  Signals s;
  s.has_bpm = true;
  s.bpm = 120.0f;
  s.has_energy = true;
  s.energy_01 = 1.0f;
  s.has_beat_phase = true;
  s.beat_phase_01 = 0.5f;

  p.get_signals(1234, &s);
  TEST_ASSERT_FALSE(s.has_bpm);
  TEST_ASSERT_EQUAL_FLOAT(0.0f, s.bpm);
  TEST_ASSERT_FALSE(s.has_energy);
  TEST_ASSERT_EQUAL_FLOAT(0.0f, s.energy_01);
  TEST_ASSERT_FALSE(s.has_beat_phase);
  TEST_ASSERT_EQUAL_FLOAT(0.0f, s.beat_phase_01);
}

