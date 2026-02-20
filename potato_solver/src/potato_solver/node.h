#ifndef POTATO_SOLVER_SRC_POTATO_SOLVER_NODE_H_
#define POTATO_SOLVER_SRC_POTATO_SOLVER_NODE_H_

#include <cstdint>

namespace potato_solver {

struct alignas(16) Node {
  uint64_t state;
  uint32_t parent;
  uint16_t pos;
  uint16_t g_cost;
};
static_assert(sizeof(Node) == 16);
static_assert(alignof(Node) == 16);

} // namespace potato_solver

#endif // POTATO_SOLVER_SRC_POTATO_SOLVER_NODE_H_
