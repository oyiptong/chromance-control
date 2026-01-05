#pragma once

#include <stddef.h>
#include <string.h>

#include "effect.h"

namespace chromance {
namespace core {

template <size_t MaxEffects>
class EffectRegistry {
 public:
  bool add(IEffect* effect) {
    if (effect == nullptr || effect->id() == nullptr) {
      return false;
    }
    if (count_ >= MaxEffects) {
      return false;
    }
    if (find(effect->id()) != nullptr) {
      return false;
    }
    effects_[count_++] = effect;
    return true;
  }

  size_t count() const { return count_; }

  IEffect* at(size_t i) const {
    if (i >= count_) {
      return nullptr;
    }
    return effects_[i];
  }

  IEffect* find(const char* id) const {
    if (id == nullptr) {
      return nullptr;
    }
    for (size_t i = 0; i < count_; ++i) {
      if (effects_[i] != nullptr && effects_[i]->id() != nullptr &&
          strcmp(effects_[i]->id(), id) == 0) {
        return effects_[i];
      }
    }
    return nullptr;
  }

 private:
  IEffect* effects_[MaxEffects] = {};
  size_t count_ = 0;
};

}  // namespace core
}  // namespace chromance

