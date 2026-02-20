cmake_minimum_required(VERSION 3.15...3.26)

project(potato_solver LANGUAGES CXX)

if (NOT SKBUILD)
  message(WARNING "\
  This CMake file is meant to be executed using 'scikit-build-core'. Running
  it directly will almost certainly not produce the desired result. If you
  are a user trying to install this package, use:
  =====================================================================
   $ pip install .
  =====================================================================
  If you are a developer, install build deps once and use:
  =====================================================================
   $ pip install nanobind scikit-build-core[pyproject]
   $ pip install --no-build-isolation -ve .
  =====================================================================
  You may optionally add -Ceditable.rebuild=true to auto-rebuild when
  C++ files change.")
endif()

find_package(Python 3.8
  REQUIRED COMPONENTS Interpreter Development.Module
  OPTIONAL_COMPONENTS Development.SABIModule)

find_package(nanobind CONFIG REQUIRED)

nanobind_add_module(
  _potato_solver_impl

  STABLE_ABI
  NB_STATIC

  src/potato_solver/bindings.cc
  src/potato_solver/flood.cc
  src/potato_solver/solver.cc
)

target_include_directories(_potato_solver_impl PRIVATE src)
target_compile_features(_potato_solver_impl PRIVATE cxx_std_20)

target_compile_options(_potato_solver_impl PRIVATE
  $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:-O3 -march=native -ffast-math>
  $<$<CXX_COMPILER_ID:MSVC>:/O2 /arch:AVX2>
)

install(TARGETS _potato_solver_impl LIBRARY DESTINATION potato_solver)
