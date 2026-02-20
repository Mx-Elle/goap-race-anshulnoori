#include "flood.h"

#include <cstring>

namespace potato_solver {

void FloodFill(const Grid &g, uint64_t state, uint16_t start_pos,
               FloodResult &result) noexcept {
  result.Clear();
  const int height = Rows(g);
  const uint64_t oob_mask = g.oob_mask;

  const int start_row = start_pos >> 6;
  const int start_col = start_pos & 63;

  const int target_row = g.target_pos >> 6;
  const uint64_t target_bit = uint64_t(1) << (g.target_pos & 63);

  uint64_t visited[kGridH] = {};
  visited[start_row] = uint64_t(1) << start_col;

  uint64_t blocked_masks[kGridH];
  for (int row = 0; row < height; ++row)
    blocked_masks[row] = BlockedRow(g, state, row);

  uint64_t f1[kGridH] = {};
  uint64_t f2[kGridH] = {};
  f1[start_row] = uint64_t(1) << start_col;
  uint64_t *curr_f = f1;
  uint64_t *next_f = f2;

  uint64_t active_rows = uint64_t(1) << start_row;
  uint64_t curr_written = active_rows;

  const uint64_t row_mask =
      (height == 64) ? ~uint64_t(0) : (uint64_t(1) << height) - 1;

  for (uint16_t step = 1; step <= static_cast<uint16_t>(height * 64); ++step) {
    uint64_t next_active_rows = 0;
    uint64_t next_written = 0;

    const uint64_t work_mask =
        (active_rows | (active_rows << 1) | (active_rows >> 1)) & row_mask;

    uint64_t wm = work_mask;
    while (wm) {
      const int row = __builtin_ctzll(wm);
      wm &= wm - 1;

      const uint64_t cur = curr_f[row];
      uint64_t spread = ((cur << 1) | (cur >> 1)) & oob_mask;
      if (row > 0)
        spread |= curr_f[row - 1];
      if (row < height - 1)
        spread |= curr_f[row + 1];

      const uint64_t new_bits = spread & ~blocked_masks[row] & ~visited[row];

      if (new_bits) [[likely]] {
        visited[row] |= new_bits;

        uint64_t btn_hits = new_bits & g.button_masks[row];
        while (btn_hits) {
          const int btn_col = __builtin_ctzll(btn_hits);
          result.buttons[result.button_count++] = {
              static_cast<uint16_t>((row << 6) | btn_col), step};
          btn_hits &= btn_hits - 1;
        }

        const uint64_t non_btn = new_bits & ~g.button_masks[row];
        if (non_btn) [[likely]] {
          next_f[row] |= non_btn;
          next_active_rows |= uint64_t(1) << row;
          next_written |= uint64_t(1) << row;
        }

        if (row == target_row && (non_btn & target_bit)) [[unlikely]] {
          result.target_reached = true;
          result.target_cost = step;
        }
      }
    }

    if (!next_active_rows) [[unlikely]]
      break;
    if (result.target_reached) [[unlikely]]
      break;

    std::swap(curr_f, next_f);

    uint64_t to_clear = curr_written;
    while (to_clear) {
      const int row = __builtin_ctzll(to_clear);
      next_f[row] = 0;
      to_clear &= to_clear - 1;
    }

    active_rows = next_active_rows;
    curr_written = next_written;
  }
}

} // namespace potato_solver
