#pragma once

#include <stddef.h>
#include <string.h>

#include "effect_descriptor.h"
#include "effect_v2.h"

namespace chromance {
namespace core {

template <size_t MaxEffects>
class EffectCatalog final {
 public:
  bool add(const EffectDescriptor& d, IEffectV2* e) {
    if (e == nullptr || !d.id.valid() || d.slug == nullptr || d.display_name == nullptr) {
      return false;
    }
    if (count_ >= MaxEffects) {
      return false;
    }
    if (find_by_id(d.id) != nullptr) {
      return false;
    }
    if (find_by_slug(d.slug) != nullptr) {
      return false;
    }
    entries_[count_++] = Entry{d, e};
    return true;
  }

  size_t count() const { return count_; }

  const EffectDescriptor* descriptor_at(size_t i) const {
    if (i >= count_) {
      return nullptr;
    }
    return &entries_[i].d;
  }

  IEffectV2* effect_at(size_t i) const {
    if (i >= count_) {
      return nullptr;
    }
    return entries_[i].e;
  }

  IEffectV2* find_by_id(EffectId id) const {
    if (!id.valid()) {
      return nullptr;
    }
    for (size_t i = 0; i < count_; ++i) {
      if (entries_[i].d.id == id) {
        return entries_[i].e;
      }
    }
    return nullptr;
  }

  const EffectDescriptor* descriptor_by_id(EffectId id) const {
    if (!id.valid()) {
      return nullptr;
    }
    for (size_t i = 0; i < count_; ++i) {
      if (entries_[i].d.id == id) {
        return &entries_[i].d;
      }
    }
    return nullptr;
  }

  const EffectDescriptor* descriptor_by_slug(const char* slug) const {
    if (slug == nullptr) {
      return nullptr;
    }
    for (size_t i = 0; i < count_; ++i) {
      if (entries_[i].d.slug != nullptr && strcmp(entries_[i].d.slug, slug) == 0) {
        return &entries_[i].d;
      }
    }
    return nullptr;
  }

  IEffectV2* find_by_slug(const char* slug) const {
    if (slug == nullptr) {
      return nullptr;
    }
    for (size_t i = 0; i < count_; ++i) {
      if (entries_[i].d.slug != nullptr && strcmp(entries_[i].d.slug, slug) == 0) {
        return entries_[i].e;
      }
    }
    return nullptr;
  }

 private:
  struct Entry {
    EffectDescriptor d;
    IEffectV2* e;
  };

  Entry entries_[MaxEffects] = {};
  size_t count_ = 0;
};

}  // namespace core
}  // namespace chromance

