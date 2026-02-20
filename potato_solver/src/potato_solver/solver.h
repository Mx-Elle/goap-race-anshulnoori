#ifndef POTATO_SOLVER_SRC_POTATO_SOLVER_SOLVER_H_
#define POTATO_SOLVER_SRC_POTATO_SOLVER_SOLVER_H_

#include <cstdint>
#include <utility>
#include <vector>

#include "grid.h"
#include "node.h"

namespace potato_solver {

struct ParentEntry {
  uint16_t pos;
  uint16_t gen;
};

inline constexpr int kHashCap = 2048;

struct alignas(16) VisitedSlot {
  uint64_t state = 0;
  uint16_t pos = 0;
  uint16_t gen = 0;
  uint16_t g_cost = 0;
  uint16_t _pad = 0;
};
static_assert(sizeof(VisitedSlot) == 16);

class Solver {
public:
  Solver(const uint8_t *walls, const uint8_t *active, const uint8_t *buttons,
         const int32_t *wall_colors, const int32_t *button_colors, int target_y,
         int target_x, int height, int width);

  [[nodiscard]] std::vector<std::pair<int, int>> Solve(int start_y,
                                                       int start_x) const;

  const Grid &GetGrid() const { return grid_; }

private:
  Grid grid_;

  mutable std::vector<Node> pool_;
  mutable std::vector<uint64_t> open_heap_;

  mutable VisitedSlot visited_slots_[kHashCap];
  mutable uint16_t visited_gen_ = 1;

  mutable ParentEntry seg_parent_[kGridArea];
  mutable uint16_t seg_gen_ = 0;

  mutable uint64_t seg_blocked_[kGridH];

  bool VisitedInsertOrUpdate(uint64_t state, uint16_t pos,
                             uint16_t g) const noexcept;
  uint16_t VisitedGet(uint64_t state, uint16_t pos) const noexcept;

  [[nodiscard]] std::vector<uint16_t> Backtrack(uint32_t goal_i) const;

  [[nodiscard]] std::vector<std::pair<int, int>>
  PathSegment(uint16_t from_pos, uint16_t to_pos,
              const uint64_t *blocked) const;
};

} // namespace potato_solver

#endif // POTATO_SOLVER_SRC_POTATO_SOLVER_SOLVER_H_
