#include "solver.h"
#include "flood.h"

#include <algorithm>
#include <cassert>
#include <cstring>
#include <limits>

namespace potato_solver {

static inline uint32_t HashKey(uint64_t state, uint16_t pos) noexcept {
  uint64_t h = state * 6364136223846793005ULL +
               static_cast<uint64_t>(pos) * 2654435761ULL;
  return static_cast<uint32_t>((h ^ (h >> 33)) & (kHashCap - 1));
}

bool Solver::VisitedInsertOrUpdate(uint64_t state, uint16_t pos,
                                   uint16_t new_g) const noexcept {
  uint32_t slot_i = HashKey(state, pos);
  while (true) {
    VisitedSlot &slot = visited_slots_[slot_i];
    if (slot.gen != visited_gen_) [[likely]] {
      slot = {state, pos, visited_gen_, new_g, 0};
      return true;
    }
    if (slot.state == state && slot.pos == pos) {
      if (slot.g_cost <= new_g)
        return false;
      slot.g_cost = new_g;
      return true;
    }
    slot_i = (slot_i + 1) & (kHashCap - 1);
  }
}

uint16_t Solver::VisitedGet(uint64_t state, uint16_t pos) const noexcept {
  uint32_t slot_i = HashKey(state, pos);
  while (true) {
    const VisitedSlot &slot = visited_slots_[slot_i];
    if (slot.gen != visited_gen_)
      return std::numeric_limits<uint16_t>::max();
    if (slot.state == state && slot.pos == pos)
      return slot.g_cost;
    slot_i = (slot_i + 1) & (kHashCap - 1);
  }
}

Solver::Solver(const uint8_t *walls, const uint8_t *active,
               const uint8_t *buttons, const int32_t *wall_colors,
               const int32_t *button_colors, int target_y, int target_x,
               int height, int width) {
  assert(height > 0 && height <= kGridH);
  assert(width > 0 && width <= kGridW);

  std::memset(&grid_, 0, sizeof(grid_));

  grid_.dims =
      static_cast<uint32_t>(height) | (static_cast<uint32_t>(width) << 16);
  grid_.oob_mask = (width == 64) ? ~uint64_t(0) : (uint64_t(1) << width) - 1;
  grid_.target_pos = static_cast<uint16_t>(target_y * 64 + target_x);

  std::memset(grid_.cell_color, kNoButton, sizeof(grid_.cell_color));
  std::memset(visited_slots_, 0, sizeof(visited_slots_));
  std::memset(seg_parent_, 0, sizeof(seg_parent_));
  visited_gen_ = 1;
  seg_gen_ = 1;

  uint64_t perm_walls[kGridH] = {};

  for (int row = 0; row < height; ++row) {
    for (int col = 0; col < width; ++col) {
      const int flat_i = row * width + col;
      const uint64_t bit = uint64_t(1) << col;

      const bool is_wall = walls[flat_i] != 0;
      const bool is_active = active[flat_i] != 0;
      const bool is_button = buttons[flat_i] != 0;
      const int wcolor = wall_colors[flat_i];
      const int bcolor = button_colors[flat_i];

      if (is_wall) {
        if (wcolor >= 0 && wcolor < kMaxColors)
          grid_.color_masks[row][wcolor] |= bit;
        if (is_active)
          grid_.initial_active[row] |= bit;
        if (wcolor == 1)
          perm_walls[row] |= bit;
      }

      if (is_button) {
        grid_.button_masks[row] |= bit;
        const uint16_t pos = static_cast<uint16_t>(row * 64 + col);
        grid_.cell_color[pos] =
            (bcolor >= 0 && bcolor < static_cast<int>(kNoButton))
                ? static_cast<uint8_t>(bcolor)
                : kNoButton;
      }
    }
  }

  static constexpr uint16_t kInfty = std::numeric_limits<uint16_t>::max();
  std::fill(std::begin(grid_.h_map), std::end(grid_.h_map), kInfty);

  uint16_t queue[kGridArea];
  int head = 0, tail = 0;

  grid_.h_map[grid_.target_pos] = 0;
  queue[tail++] = grid_.target_pos;

  while (head != tail) {
    const uint16_t pos = queue[head++];
    const int row = pos >> 6;
    const int col = pos & 63;
    const uint16_t next_dist = grid_.h_map[pos] + 1;

    const int nbr_row[4] = {row - 1, row + 1, row, row};
    const int nbr_col[4] = {col, col, col - 1, col + 1};

    for (int i = 0; i < 4; ++i) {
      const int nr = nbr_row[i], nc = nbr_col[i];
      if (nr < 0 || nr >= height || nc < 0 || nc >= width)
        continue;
      if (perm_walls[nr] & (uint64_t(1) << nc))
        continue;
      const uint16_t nbr_pos = static_cast<uint16_t>(nr * 64 + nc);
      if (grid_.h_map[nbr_pos] != kInfty)
        continue;
      grid_.h_map[nbr_pos] = next_dist;
      queue[tail++] = nbr_pos;
    }
  }
}

static void ComputeBlocked(const Grid &g, uint64_t state,
                           uint64_t *blocked) noexcept {
  const int height = Rows(g);
  for (int row = 0; row < height; ++row)
    blocked[row] = BlockedRow(g, state, row);
}

std::vector<std::pair<int, int>> Solver::Solve(int start_y, int start_x) const {
  const uint16_t start_pos = static_cast<uint16_t>(start_y * 64 + start_x);
  const uint16_t target_pos = grid_.target_pos;

  if (start_pos == target_pos)
    return {{start_y, start_x}};

  pool_.clear();
  pool_.reserve(256);
  open_heap_.clear();
  open_heap_.reserve(256);
  ++visited_gen_;

  const uint16_t start_h = grid_.h_map[start_pos];

  pool_.push_back({0, std::numeric_limits<uint32_t>::max(), start_pos, 0});
  open_heap_.push_back((uint64_t(start_h) << 32) | 0);
  std::push_heap(open_heap_.begin(), open_heap_.end(),
                 std::greater<uint64_t>{});
  VisitedInsertOrUpdate(0, start_pos, 0);

  uint32_t best_node_i = std::numeric_limits<uint32_t>::max();
  uint16_t best_total = std::numeric_limits<uint16_t>::max();

  FloodResult res;
  while (!open_heap_.empty()) {
    std::pop_heap(open_heap_.begin(), open_heap_.end(),
                  std::greater<uint64_t>{});
    const uint64_t top_packed = open_heap_.back();
    open_heap_.pop_back();

    const uint16_t top_f = static_cast<uint16_t>(top_packed >> 32);
    if (top_f >= best_total) [[unlikely]]
      break;

    const uint32_t node_i = static_cast<uint32_t>(top_packed);
    const Node &node = pool_[node_i];

    if (VisitedGet(node.state, node.pos) < node.g_cost) [[unlikely]]
      continue;

    FloodFill(grid_, node.state, node.pos, res);

    if (res.target_reached) [[unlikely]] {
      const uint16_t total = node.g_cost + res.target_cost;
      if (total < best_total) {
        best_total = total;
        best_node_i = node_i;
      }
    }

    for (int bi = 0; bi < res.button_count; ++bi) {
      const auto [btn_pos, step_cost] = res.buttons[bi];
      const uint8_t cid = grid_.cell_color[btn_pos];
      if (cid == kNoButton) [[unlikely]]
        continue;

      const uint64_t new_state = node.state ^ (uint64_t(1) << cid);
      const uint16_t nbr_g = node.g_cost + step_cost;
      const uint16_t nbr_h = grid_.h_map[btn_pos];

      if (nbr_g + nbr_h >= best_total) [[unlikely]]
        continue;
      if (!VisitedInsertOrUpdate(new_state, btn_pos, nbr_g)) [[unlikely]]
        continue;

      const uint32_t new_node_i = static_cast<uint32_t>(pool_.size());
      pool_.push_back({new_state, node_i, btn_pos, nbr_g});

      open_heap_.push_back((uint64_t(nbr_g + nbr_h) << 32) | new_node_i);
      std::push_heap(open_heap_.begin(), open_heap_.end(),
                     std::greater<uint64_t>{});
    }
  }

  if (best_node_i == std::numeric_limits<uint32_t>::max())
    return {};

  const std::vector<uint16_t> waypoints = Backtrack(best_node_i);

  std::vector<std::pair<int, int>> full_path;
  full_path.reserve(1024);

  uint64_t seg_state = 0;
  uint16_t prev_pos = start_pos;

  ComputeBlocked(grid_, seg_state, seg_blocked_);

  for (uint16_t wp : waypoints) {
    auto seg = PathSegment(prev_pos, wp, seg_blocked_);
    if (!seg.empty()) {
      if (!full_path.empty() && full_path.back() == seg.front())
        seg.erase(seg.begin());
      full_path.insert(full_path.end(), seg.begin(), seg.end());
    }
    const uint8_t cid = grid_.cell_color[wp];
    if (cid != kNoButton) {
      seg_state ^= uint64_t(1) << cid;
      ComputeBlocked(grid_, seg_state, seg_blocked_);
    }
    prev_pos = wp;
  }

  auto seg = PathSegment(prev_pos, target_pos, seg_blocked_);
  if (!seg.empty()) {
    if (!full_path.empty() && full_path.back() == seg.front())
      seg.erase(seg.begin());
    full_path.insert(full_path.end(), seg.begin(), seg.end());
  }

  return full_path;
}

std::vector<uint16_t> Solver::Backtrack(uint32_t goal_i) const {
  std::vector<uint16_t> chain;
  uint32_t node_i = goal_i;
  while (node_i != std::numeric_limits<uint32_t>::max()) {
    chain.push_back(pool_[node_i].pos);
    node_i = pool_[node_i].parent;
  }
  std::reverse(chain.begin(), chain.end());
  if (!chain.empty())
    chain.erase(chain.begin());
  return chain;
}

std::vector<std::pair<int, int>>
Solver::PathSegment(uint16_t from_pos, uint16_t to_pos,
                    const uint64_t *blocked) const {
  if (from_pos == to_pos)
    return {{from_pos >> 6, from_pos & 63}};

  const int height = Rows(grid_);
  const int width = Cols(grid_);
  const int to_row = to_pos >> 6;
  const int to_col = to_pos & 63;

  const uint16_t cur_gen = ++seg_gen_;

  uint16_t q[kGridArea];
  int head = 0, tail = 0;

  seg_parent_[from_pos] = {from_pos, cur_gen};
  q[tail++] = from_pos;

  bool found = false;

  auto try_nbr = [&](int nr, int nc) __attribute__((always_inline)) -> bool {
    if (static_cast<unsigned>(nr) >= static_cast<unsigned>(height) ||
        static_cast<unsigned>(nc) >= static_cast<unsigned>(width))
      return false;
    if (blocked[nr] & (uint64_t(1) << nc))
      return false;
    const uint16_t nbr_pos = static_cast<uint16_t>(nr * 64 + nc);
    if (seg_parent_[nbr_pos].gen == cur_gen) [[unlikely]]
      return false;
    const bool is_dest = (nr == to_row && nc == to_col);
    const bool is_button = (grid_.button_masks[nr] & (uint64_t(1) << nc)) != 0;
    if (is_button && !is_dest) [[unlikely]]
      return false;
    seg_parent_[nbr_pos] = {q[head - 1], cur_gen};
    if (is_dest)
      return true;
    q[tail++] = nbr_pos;
    return false;
  };

  while (head != tail && !found) {
    const uint16_t cur_pos = q[head++];
    const int row = cur_pos >> 6;
    const int col = cur_pos & 63;

    if (try_nbr(row - 1, col)) {
      found = true;
      break;
    }
    if (try_nbr(row + 1, col)) {
      found = true;
      break;
    }
    if (try_nbr(row, col - 1)) {
      found = true;
      break;
    }
    if (try_nbr(row, col + 1)) {
      found = true;
      break;
    }
  }

  if (!found)
    return {};

  std::vector<std::pair<int, int>> path;
  uint16_t cur = to_pos;
  while (cur != from_pos) {
    path.emplace_back(cur >> 6, cur & 63);
    cur = seg_parent_[cur].pos;
  }
  path.emplace_back(from_pos >> 6, from_pos & 63);
  std::reverse(path.begin(), path.end());
  return path;
}

} // namespace potato_solver
