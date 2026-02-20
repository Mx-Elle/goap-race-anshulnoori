#ifndef POTATO_SOLVER_SRC_POTATO_SOLVER_FLOOD_H_
#define POTATO_SOLVER_SRC_POTATO_SOLVER_FLOOD_H_

#include <array>
#include <cstdint>
#include <utility>

#include "grid.h"

namespace potato_solver {

[[nodiscard]] inline uint64_t BlockedRow(const Grid &g, uint64_t state,
                                         int row) noexcept {
  uint64_t blocked = g.initial_active[row];
  uint64_t ws = state;
  while (ws) {
    blocked ^= g.color_masks[row][__builtin_ctzll(ws)];
    ws &= ws - 1;
  }
  return blocked;
}

struct FloodResult {
  std::array<std::pair<uint16_t, uint16_t>, kMaxColors> buttons;
  int button_count = 0;
  uint16_t target_cost = 0;
  bool target_reached = false;

  void Clear() {
    button_count = 0;
    target_cost = 0;
    target_reached = false;
  }
};

void FloodFill(const Grid &g, uint64_t state, uint16_t start_pos,
               FloodResult &result) noexcept;

} // namespace potato_solver

#endif // POTATO_SOLVER_SRC_POTATO_SOLVER_FLOOD_H_
