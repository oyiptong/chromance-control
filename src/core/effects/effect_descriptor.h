#pragma once

#include "effect_id.h"

namespace chromance {
namespace core {

struct EffectDescriptor {
  EffectId id;
  const char* slug;         // "breathing", "index_walk", ...
  const char* display_name; // "Breathing", ...
  const char* description;  // optional (can be nullptr)
};

}  // namespace core
}  // namespace chromance
