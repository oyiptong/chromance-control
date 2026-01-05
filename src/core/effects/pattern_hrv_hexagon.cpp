#include "pattern_hrv_hexagon.h"

namespace chromance {
namespace core {

const uint8_t HrvHexagonEffect::kHexSegs[HrvHexagonEffect::kHexCount]
                                       [HrvHexagonEffect::kHexSegCount] = {
    {1, 2, 4, 5, 9, 12, 3, 6, 7},
    {12, 13, 15, 23, 26, 28, 14, 24, 25},
    {28, 29, 31, 36, 39, 40, 30, 37, 38},
    {8, 9, 11, 15, 18, 20, 10, 16, 17},
    {20, 21, 26, 31, 34, 35, 27, 32, 33},
    {6, 7, 10, 14, 17, 24, 9, 12, 15},
    {24, 25, 27, 30, 33, 37, 26, 28, 31},
    {16, 17, 19, 22, 27, 32, 18, 20, 21},
};

}  // namespace core
}  // namespace chromance

