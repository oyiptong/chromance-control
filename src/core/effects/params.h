#pragma once

#include <stdint.h>

#include "../types.h"

namespace chromance {
namespace core {

enum class ParamType : uint8_t { U8, I16, U16, Bool, Enum, ColorRgb };

struct ParamId {
  uint16_t value;

  constexpr ParamId() : value(0) {}
  constexpr explicit ParamId(uint16_t v) : value(v) {}
  constexpr bool valid() const { return value != 0; }
};

struct ParamDescriptor {
  ParamId id;
  const char* name;         // "dot_count"
  const char* display_name; // "Dot Count"
  ParamType type;

  // storage mapping:
  uint16_t offset;  // byte offset into effect's config blob
  uint8_t size;     // bytes (1/2/3/4)

  // validation / UI hints:
  int32_t min;
  int32_t max;
  int32_t step;
  int32_t def;

  // Fixed-point scale factor for float-exposed numeric params.
  // scale=1 => integer UI, raw is stored as-is.
  // scale>1 => UI value = raw / scale, firmware stores raw scaled integer.
  uint16_t scale;
};

struct EffectConfigSchema {
  const ParamDescriptor* params;
  uint8_t param_count;
};

union ParamUnion {
  uint8_t u8;
  int16_t i16;
  uint16_t u16;
  bool b;
  uint8_t e;      // enum as u8
  Rgb color_rgb;  // 3 bytes

  constexpr ParamUnion() : u8(0) {}
};

struct ParamValue {
  ParamType type;
  ParamUnion v;

  constexpr ParamValue() : type(ParamType::U8), v() {}
};

}  // namespace core
}  // namespace chromance
