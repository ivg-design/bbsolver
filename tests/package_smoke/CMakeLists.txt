cmake_minimum_required(VERSION 3.20)

project(bbsolver_package_smoke LANGUAGES CXX)

option(
  BBSOLVER_PACKAGE_SMOKE_LINK_CORE
  "Build and link a small executable against bbsolver::core."
  ON)

find_package(bbsolver CONFIG REQUIRED)

if(NOT TARGET bbsolver::bbsolver)
  message(FATAL_ERROR "bbsolver package did not export bbsolver::bbsolver")
endif()

if(NOT TARGET bbsolver::core)
  message(FATAL_ERROR "bbsolver package did not export bbsolver::core")
endif()

if(BBSOLVER_PACKAGE_SMOKE_LINK_CORE)
  add_executable(bbsolver_package_smoke main.cpp)
  target_link_libraries(bbsolver_package_smoke PRIVATE bbsolver::core)
endif()
