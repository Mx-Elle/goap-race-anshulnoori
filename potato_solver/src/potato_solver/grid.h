#ifndef POTATO_SOLVER_SRC_POTATO_SOLVER_GRID_H_
#define POTATO_SOLVER_SRC_POTATO_SOLVER_GRID_H_

#include <cstdint>

namespace potato_solver {

inline constexpr int kGridW = 64;
inline constexpr int kGridH = 64;
inline constexpr int kGridArea = kGridW * kGridH;
inline constexpr int kMaxColors = 8;
inline constexpr uint8_t kNoButton = 0xFF;

struct alignas(64) Grid {
  uint64_t initial_active[kGridH];
  uint64_t color_masks[kGridH][kMaxColors];
  uint64_t button_masks[kGridH];
  uint16_t h_map[kGridArea];
  uint8_t cell_color[kGridArea]; // cell pos → color id
  uint64_t oob_mask;
  uint16_t target_pos;
  uint32_t dims; // bits [15:0] = rows, bits [31:16] = cols
};

inline int Rows(const Grid &g) noexcept {
  return static_cast<int>(g.dims & 0xFFFF);
}

inline int Cols(const Grid &g) noexcept {
  return static_cast<int>(g.dims >> 16);
}

} // namespace potato_solver

#endif // POTATO_SOLVER_SRC_POTATO_SOLVER_GRID_H_
