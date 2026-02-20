#include <nanobind/nanobind.h>
#include <nanobind/ndarray.h>
#include <nanobind/stl/pair.h>
#include <nanobind/stl/vector.h>

#include "solver.h"

namespace nb = nanobind;
using namespace nb::literals;

using U8Array2D  = nb::ndarray<uint8_t,  nb::shape<-1, -1>, nb::c_contig>;
using I32Array2D = nb::ndarray<int32_t,  nb::shape<-1, -1>, nb::c_contig>;

NB_MODULE(_potato_solver_impl, m) {
  nb::class_<potato_solver::Solver>(m, "Solver")
      .def(
          "__init__",
          [](potato_solver::Solver *self, U8Array2D walls, U8Array2D active,
             U8Array2D buttons, I32Array2D wall_colors, I32Array2D button_colors,
             int target_y, int target_x) {
            const int height = static_cast<int>(walls.shape(0));
            const int width  = static_cast<int>(walls.shape(1));
            new (self) potato_solver::Solver(
                walls.data(), active.data(), buttons.data(),
                wall_colors.data(), button_colors.data(),
                target_y, target_x, height, width);
          },
          "walls"_a, "active"_a, "buttons"_a, "wall_colors"_a,
          "button_colors"_a, "target_y"_a, "target_x"_a)
      .def("solve", &potato_solver::Solver::Solve, "start_y"_a, "start_x"_a)
      .def("version", [] { return "potato_solver v0.2.0"; });
}
