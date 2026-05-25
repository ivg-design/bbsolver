cmake_minimum_required(VERSION 3.20)

project(bbsolver VERSION 1.0.0 LANGUAGES CXX)

include(FetchContent)
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

option(BBSOLVER_BUILD_TESTS "Build bbsolver tests" OFF)
option(BBSOLVER_BUILD_CLI "Build the bbsolver command-line executable" ON)
option(
  BBSOLVER_USE_SYSTEM_DEPS
  "Prefer find_package dependencies before FetchContent/archive fallback."
  ON)
option(
  BBSOLVER_REQUIRE_SYSTEM_DEPS
  "Require dependencies from find_package and disable FetchContent/archive fallback."
  OFF)
option(
  BBSOLVER_THIRD_PARTY_ARCHIVE_FALLBACK
  "Fall back to solver/third_party/archive dependency archives when remote downloads fail."
  ON)
option(
  BBSOLVER_FORCE_THIRD_PARTY_ARCHIVES
  "Use solver/third_party/archive dependency archives instead of remote dependency URLs."
  OFF)
option(
  BBSOLVER_ENABLE_REMOTE_PROBES
  "Probe remote dependency archives before using local fallback archives."
  ON)

set(
  BBSOLVER_THIRD_PARTY_ARCHIVE_DIR
  "${CMAKE_CURRENT_LIST_DIR}/third_party/archive"
  CACHE PATH
  "Local backup archive directory for bbsolver third-party dependencies.")
set(
  BBSOLVER_FETCH_TIMEOUT_SECONDS
  "120"
  CACHE STRING
  "Timeout in seconds for each remote third-party dependency download attempt.")
set(
  BBSOLVER_REMOTE_PROBE_TIMEOUT_SECONDS
  "8"
  CACHE STRING
  "Timeout in seconds for probing remote dependency archive availability.")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
if(PROJECT_IS_TOP_LEVEL)
  set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
  set(CMAKE_EXPORT_COMPILE_COMMANDS ON CACHE BOOL "Export compile_commands.json for clangd")
endif()

set(BBSOLVER_PROTOCOL_DIR "${CMAKE_CURRENT_LIST_DIR}/protocol")
set(BBSOLVER_GENERATED_DIR "${CMAKE_BINARY_DIR}/generated")
set(BBSOLVER_GENERATED_NS_DIR "${BBSOLVER_GENERATED_DIR}/bbsolver")
set(BBSOLVER_INSTALL_CMAKEDIR
  "${CMAKE_INSTALL_LIBDIR}/cmake/bbsolver"
  CACHE STRING
  "Install destination for bbsolver CMake package files.")
set(BBSOLVER_INSTALL_THIRD_PARTY_ARCHIVE_DIR
  "${CMAKE_INSTALL_DATADIR}/bbsolver/third_party/archive")

function(bbsolver_fetch_header_only_target target_name include_dir)
  if(NOT TARGET "${target_name}")
    add_library("${target_name}" INTERFACE IMPORTED)
    set_target_properties("${target_name}" PROPERTIES
      INTERFACE_INCLUDE_DIRECTORIES "${include_dir}")
  endif()
endfunction()

function(bbsolver_dependency_urls out_var dependency_name archive_name remote_url)
  set(local_archive "${BBSOLVER_THIRD_PARTY_ARCHIVE_DIR}/${archive_name}")
  if(BBSOLVER_FORCE_THIRD_PARTY_ARCHIVES)
    if(NOT EXISTS "${local_archive}")
      message(
        FATAL_ERROR
        "BBSOLVER_FORCE_THIRD_PARTY_ARCHIVES is ON, but ${dependency_name} "
        "backup archive is missing: ${local_archive}")
    endif()
    set(selected_urls "${local_archive}")
    message(
      STATUS
      "bbsolver dependency ${dependency_name}: using forced local archive ${local_archive}")
  elseif(BBSOLVER_THIRD_PARTY_ARCHIVE_FALLBACK AND EXISTS "${local_archive}")
    find_program(BBSOLVER_CURL_EXECUTABLE curl)
    if(BBSOLVER_ENABLE_REMOTE_PROBES AND BBSOLVER_CURL_EXECUTABLE)
      execute_process(
        COMMAND "${BBSOLVER_CURL_EXECUTABLE}"
          --location
          --head
          --fail
          --silent
          --show-error
          --max-time "${BBSOLVER_REMOTE_PROBE_TIMEOUT_SECONDS}"
          "${remote_url}"
        RESULT_VARIABLE remote_probe_result
        OUTPUT_QUIET
        ERROR_QUIET)
    elseif(BBSOLVER_ENABLE_REMOTE_PROBES)
      set(remote_probe_result 0)
    else()
      set(remote_probe_result 1)
    endif()
    if(remote_probe_result EQUAL 0)
      set(selected_urls "${remote_url}")
    else()
      set(selected_urls "${local_archive}")
      message(
        WARNING
        "bbsolver dependency ${dependency_name}: remote archive unavailable; "
        "using local backup archive ${local_archive}")
    endif()
  else()
    set(selected_urls "${remote_url}")
    if(BBSOLVER_THIRD_PARTY_ARCHIVE_FALLBACK)
      message(
        WARNING
        "bbsolver dependency ${dependency_name}: backup archive missing; "
        "remote-only fetch will be used. Expected: ${local_archive}")
    endif()
  endif()
  set("${out_var}" ${selected_urls} PARENT_SCOPE)
endfunction()

if(BBSOLVER_USE_SYSTEM_DEPS AND NOT BBSOLVER_FORCE_THIRD_PARTY_ARCHIVES)
  find_package(nlohmann_json 3.11 QUIET NO_CMAKE_PACKAGE_REGISTRY)
endif()
if(NOT TARGET nlohmann_json::nlohmann_json)
  if(BBSOLVER_REQUIRE_SYSTEM_DEPS)
    message(FATAL_ERROR "nlohmann_json 3.11 was not found and BBSOLVER_REQUIRE_SYSTEM_DEPS is ON")
  endif()
  bbsolver_dependency_urls(
    BBSOLVER_NLOHMANN_JSON_URLS
    nlohmann_json
    nlohmann-json-3.11.3.tar.xz
    https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz)
  FetchContent_Declare(
    nlohmann_json
    URL ${BBSOLVER_NLOHMANN_JSON_URLS}
    URL_HASH SHA256=d6c65aca6b1ed68e7a182f4757257b107ae403032760ed6ef121c9d55e81757d
    TIMEOUT ${BBSOLVER_FETCH_TIMEOUT_SECONDS}
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
  set(JSON_BuildTests OFF CACHE INTERNAL "")
  FetchContent_MakeAvailable(nlohmann_json)
endif()

if(BBSOLVER_USE_SYSTEM_DEPS AND NOT BBSOLVER_FORCE_THIRD_PARTY_ARCHIVES)
  find_package(Eigen3 3.4 QUIET NO_MODULE NO_CMAKE_PACKAGE_REGISTRY)
endif()
if(NOT TARGET Eigen3::Eigen)
  if(BBSOLVER_REQUIRE_SYSTEM_DEPS)
    message(FATAL_ERROR "Eigen3 3.4 was not found and BBSOLVER_REQUIRE_SYSTEM_DEPS is ON")
  endif()
  bbsolver_dependency_urls(
    BBSOLVER_EIGEN_URLS
    Eigen
    eigen-3.4.0.tar.gz
    https://gitlab.com/libeigen/eigen/-/archive/3.4.0/eigen-3.4.0.tar.gz)
  FetchContent_Populate(
    eigen
    URL ${BBSOLVER_EIGEN_URLS}
    URL_HASH SHA256=8586084f71f9bde545ee7fa6d00288b264a2b7ac3607b974e54d13e7162c1c72
    TIMEOUT ${BBSOLVER_FETCH_TIMEOUT_SECONDS}
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
  file(MAKE_DIRECTORY "${eigen_BINARY_DIR}")
  file(WRITE "${eigen_BINARY_DIR}/Eigen3Config.cmake"
    "set(Eigen3_VERSION \"3.4.0\")\n"
    "if(NOT TARGET Eigen3::Eigen)\n"
    "  add_library(Eigen3::Eigen INTERFACE IMPORTED)\n"
    "  set_target_properties(Eigen3::Eigen PROPERTIES\n"
    "    INTERFACE_INCLUDE_DIRECTORIES \"${eigen_SOURCE_DIR}\")\n"
    "endif()\n"
    "set(Eigen3_FOUND TRUE)\n")
  file(WRITE "${eigen_BINARY_DIR}/Eigen3ConfigVersion.cmake"
    "set(PACKAGE_VERSION \"3.4.0\")\n"
    "if(PACKAGE_FIND_VERSION VERSION_GREATER PACKAGE_VERSION)\n"
    "  set(PACKAGE_VERSION_COMPATIBLE FALSE)\n"
    "else()\n"
    "  set(PACKAGE_VERSION_COMPATIBLE TRUE)\n"
    "endif()\n"
    "if(PACKAGE_FIND_VERSION VERSION_EQUAL PACKAGE_VERSION)\n"
    "  set(PACKAGE_VERSION_EXACT TRUE)\n"
    "endif()\n")
  set(Eigen3_DIR "${eigen_BINARY_DIR}" CACHE PATH "Path to fetched Eigen3Config.cmake" FORCE)
  if(NOT TARGET Eigen3::Eigen)
    bbsolver_fetch_header_only_target(Eigen3::Eigen "${eigen_SOURCE_DIR}")
  endif()
endif()

if(BBSOLVER_USE_SYSTEM_DEPS AND NOT BBSOLVER_FORCE_THIRD_PARTY_ARCHIVES)
  find_package(Ceres 2.2 QUIET NO_CMAKE_PACKAGE_REGISTRY)
endif()
if(TARGET Ceres::ceres)
  set(BBSOLVER_CERES_TARGET Ceres::ceres)
elseif(TARGET ceres)
  set(BBSOLVER_CERES_TARGET ceres)
else()
  if(BBSOLVER_REQUIRE_SYSTEM_DEPS)
    message(FATAL_ERROR "Ceres 2.2 was not found and BBSOLVER_REQUIRE_SYSTEM_DEPS is ON")
  endif()
  set(MINIGLOG ON CACHE BOOL "" FORCE)
  set(SUITESPARSE OFF CACHE BOOL "" FORCE)
  set(GFLAGS OFF CACHE BOOL "" FORCE)
  set(CXSPARSE OFF CACHE BOOL "" FORCE)
  set(SCHUR_SPECIALIZATIONS OFF CACHE BOOL "" FORCE)
  set(EIGENSPARSE OFF CACHE BOOL "" FORCE)
  set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
  set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
  set(BUILD_BENCHMARKS OFF CACHE BOOL "" FORCE)
  set(PROVIDE_UNINSTALL_TARGET OFF CACHE BOOL "" FORCE)
  set(USE_CUDA OFF CACHE BOOL "" FORCE)
  bbsolver_dependency_urls(
    BBSOLVER_CERES_URLS
    Ceres
    ceres-solver-2.2.0.tar.gz
    https://github.com/ceres-solver/ceres-solver/archive/refs/tags/2.2.0.tar.gz)
  FetchContent_Declare(
    ceres_solver
    URL ${BBSOLVER_CERES_URLS}
    URL_HASH SHA256=12efacfadbfdc1bbfa203c236e96f4d3c210bed96994288b3ff0c8e7c6f350d4
    TIMEOUT ${BBSOLVER_FETCH_TIMEOUT_SECONDS}
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
  FetchContent_Populate(
    ceres_solver
    URL ${BBSOLVER_CERES_URLS}
    URL_HASH SHA256=12efacfadbfdc1bbfa203c236e96f4d3c210bed96994288b3ff0c8e7c6f350d4
    TIMEOUT ${BBSOLVER_FETCH_TIMEOUT_SECONDS}
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
  add_subdirectory(
    "${ceres_solver_SOURCE_DIR}"
    "${ceres_solver_BINARY_DIR}"
    EXCLUDE_FROM_ALL)
  if(TARGET Ceres::ceres)
    set(BBSOLVER_CERES_TARGET Ceres::ceres)
  elseif(TARGET ceres)
    set(BBSOLVER_CERES_TARGET ceres)
  else()
    message(FATAL_ERROR "Ceres target was not created")
  endif()
endif()

if(BBSOLVER_USE_SYSTEM_DEPS AND NOT BBSOLVER_FORCE_THIRD_PARTY_ARCHIVES)
  find_package(TBB 2021 QUIET COMPONENTS tbb NO_CMAKE_PACKAGE_REGISTRY)
endif()
if(TARGET TBB::tbb)
  set(BBSOLVER_TBB_TARGET TBB::tbb)
elseif(TARGET tbb)
  set(BBSOLVER_TBB_TARGET tbb)
else()
  if(BBSOLVER_REQUIRE_SYSTEM_DEPS)
    message(FATAL_ERROR "TBB 2021 was not found and BBSOLVER_REQUIRE_SYSTEM_DEPS is ON")
  endif()
  set(TBB_TEST OFF CACHE BOOL "" FORCE)
  set(TBB_EXAMPLES OFF CACHE BOOL "" FORCE)
  set(TBB_INSTALL OFF CACHE BOOL "" FORCE)
  set(TBBMALLOC_BUILD OFF CACHE BOOL "" FORCE)
  bbsolver_dependency_urls(
    BBSOLVER_ONETBB_URLS
    oneTBB
    onetbb-2021.13.0.tar.gz
    https://github.com/oneapi-src/oneTBB/archive/refs/tags/v2021.13.0.tar.gz)
  FetchContent_Declare(
    onetbb
    URL ${BBSOLVER_ONETBB_URLS}
    URL_HASH SHA256=3ad5dd08954b39d113dc5b3f8a8dc6dc1fd5250032b7c491eb07aed5c94133e1
    TIMEOUT ${BBSOLVER_FETCH_TIMEOUT_SECONDS}
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
  FetchContent_Populate(
    onetbb
    URL ${BBSOLVER_ONETBB_URLS}
    URL_HASH SHA256=3ad5dd08954b39d113dc5b3f8a8dc6dc1fd5250032b7c491eb07aed5c94133e1
    TIMEOUT ${BBSOLVER_FETCH_TIMEOUT_SECONDS}
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
  add_subdirectory(
    "${onetbb_SOURCE_DIR}"
    "${onetbb_BINARY_DIR}"
    EXCLUDE_FROM_ALL)
  if(TARGET TBB::tbb)
    set(BBSOLVER_TBB_TARGET TBB::tbb)
  elseif(TARGET tbb)
    set(BBSOLVER_TBB_TARGET tbb)
  else()
    message(FATAL_ERROR "TBB target was not created")
  endif()
endif()

if(BBSOLVER_USE_SYSTEM_DEPS AND NOT BBSOLVER_FORCE_THIRD_PARTY_ARCHIVES)
  find_package(flatbuffers CONFIG QUIET NO_CMAKE_PACKAGE_REGISTRY)
endif()
if(TARGET flatbuffers::flatbuffers)
  set(BBSOLVER_FLATBUFFERS_TARGET flatbuffers::flatbuffers)
elseif(TARGET flatbuffers)
  set(BBSOLVER_FLATBUFFERS_TARGET flatbuffers)
else()
  if(BBSOLVER_REQUIRE_SYSTEM_DEPS)
    message(FATAL_ERROR "FlatBuffers was not found and BBSOLVER_REQUIRE_SYSTEM_DEPS is ON")
  endif()
  set(FLATBUFFERS_BUILD_TESTS OFF CACHE BOOL "" FORCE)
  set(FLATBUFFERS_INSTALL OFF CACHE BOOL "" FORCE)
  set(FLATBUFFERS_BUILD_FLATC ON CACHE BOOL "" FORCE)
  set(FLATBUFFERS_BUILD_FLATHASH OFF CACHE BOOL "" FORCE)
  set(FLATBUFFERS_BUILD_SHAREDLIB OFF CACHE BOOL "" FORCE)
  bbsolver_dependency_urls(
    BBSOLVER_FLATBUFFERS_URLS
    FlatBuffers
    flatbuffers-24.3.25.tar.gz
    https://github.com/google/flatbuffers/archive/refs/tags/v24.3.25.tar.gz)
  FetchContent_Declare(
    flatbuffers_src
    URL ${BBSOLVER_FLATBUFFERS_URLS}
    URL_HASH SHA256=4157c5cacdb59737c5d627e47ac26b140e9ee28b1102f812b36068aab728c1ed
    TIMEOUT ${BBSOLVER_FETCH_TIMEOUT_SECONDS}
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
  FetchContent_MakeAvailable(flatbuffers_src)
  if(TARGET flatbuffers::flatbuffers)
    set(BBSOLVER_FLATBUFFERS_TARGET flatbuffers::flatbuffers)
  elseif(TARGET flatbuffers)
    set(BBSOLVER_FLATBUFFERS_TARGET flatbuffers)
  else()
    bbsolver_fetch_header_only_target(
      bbsolver_flatbuffers_headers
      "${flatbuffers_src_SOURCE_DIR}/include")
    set(BBSOLVER_FLATBUFFERS_TARGET bbsolver_flatbuffers_headers)
  endif()
endif()

find_program(FLATC_EXECUTABLE NAMES flatc)
if(NOT FLATC_EXECUTABLE)
  if(NOT DEFINED flatbuffers_src_SOURCE_DIR)
    message(FATAL_ERROR "flatc executable not found and no fetched FlatBuffers source is available")
  endif()

  set(BBSOLVER_FLATC_BUILD_DIR "${CMAKE_BINARY_DIR}/flatc-build")
  execute_process(
    COMMAND "${CMAKE_COMMAND}"
      -S "${flatbuffers_src_SOURCE_DIR}"
      -B "${BBSOLVER_FLATC_BUILD_DIR}"
      -DCMAKE_BUILD_TYPE=Release
      -DFLATBUFFERS_BUILD_TESTS=OFF
      -DFLATBUFFERS_INSTALL=OFF
      -DFLATBUFFERS_BUILD_FLATC=ON
      -DFLATBUFFERS_BUILD_FLATHASH=OFF
      -DFLATBUFFERS_BUILD_SHAREDLIB=OFF
    RESULT_VARIABLE BBSOLVER_FLATC_CONFIGURE_RESULT)
  if(NOT BBSOLVER_FLATC_CONFIGURE_RESULT EQUAL 0)
    message(FATAL_ERROR "Failed to configure fetched flatc")
  endif()

  execute_process(
    COMMAND "${CMAKE_COMMAND}" --build "${BBSOLVER_FLATC_BUILD_DIR}" --target flatc --config Release
    RESULT_VARIABLE BBSOLVER_FLATC_BUILD_RESULT)
  if(NOT BBSOLVER_FLATC_BUILD_RESULT EQUAL 0)
    message(FATAL_ERROR "Failed to build fetched flatc")
  endif()

  find_program(FLATC_EXECUTABLE
    NAMES flatc
    HINTS "${BBSOLVER_FLATC_BUILD_DIR}" "${BBSOLVER_FLATC_BUILD_DIR}/Release"
    NO_DEFAULT_PATH)
endif()

if(NOT FLATC_EXECUTABLE)
  message(FATAL_ERROR "flatc executable not found and fetched flatc was not produced")
endif()

file(MAKE_DIRECTORY "${BBSOLVER_GENERATED_NS_DIR}")
execute_process(
  COMMAND "${FLATC_EXECUTABLE}"
    --cpp
    --scoped-enums
    -I "${BBSOLVER_PROTOCOL_DIR}"
    -o "${BBSOLVER_GENERATED_NS_DIR}"
    "${BBSOLVER_PROTOCOL_DIR}/samples.fbs"
    "${BBSOLVER_PROTOCOL_DIR}/keys.fbs"
  WORKING_DIRECTORY "${CMAKE_CURRENT_LIST_DIR}"
  RESULT_VARIABLE BBSOLVER_FLATC_SCHEMA_RESULT)
if(NOT BBSOLVER_FLATC_SCHEMA_RESULT EQUAL 0)
  message(FATAL_ERROR "flatc failed to generate bbsolver protocol headers")
endif()

file(GLOB_RECURSE BBSOLVER_CORE_SOURCES CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_LIST_DIR}/src/*.cpp")
list(FILTER BBSOLVER_CORE_SOURCES EXCLUDE REGEX "/main\\.cpp$")

set(BBSOLVER_BUILD_DEPENDENCY_TARGETS
  nlohmann_json::nlohmann_json
  Eigen3::Eigen
  "${BBSOLVER_CERES_TARGET}"
  "${BBSOLVER_TBB_TARGET}"
  "${BBSOLVER_FLATBUFFERS_TARGET}")

add_library(bbsolver_core STATIC ${BBSOLVER_CORE_SOURCES})
add_library(bbsolver::core ALIAS bbsolver_core)
set_target_properties(bbsolver_core PROPERTIES EXPORT_NAME core)
target_compile_features(bbsolver_core PUBLIC cxx_std_17)
target_include_directories(bbsolver_core
  PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/include>"
    "$<BUILD_INTERFACE:${BBSOLVER_GENERATED_DIR}>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
  PRIVATE
    "${CMAKE_CURRENT_LIST_DIR}/src")
foreach(BBSOLVER_DEPENDENCY_TARGET IN LISTS BBSOLVER_BUILD_DEPENDENCY_TARGETS)
  target_include_directories(bbsolver_core PRIVATE
    "$<TARGET_PROPERTY:${BBSOLVER_DEPENDENCY_TARGET},INTERFACE_INCLUDE_DIRECTORIES>")
  target_compile_definitions(bbsolver_core PRIVATE
    "$<TARGET_PROPERTY:${BBSOLVER_DEPENDENCY_TARGET},INTERFACE_COMPILE_DEFINITIONS>")
  target_compile_options(bbsolver_core PRIVATE
    "$<TARGET_PROPERTY:${BBSOLVER_DEPENDENCY_TARGET},INTERFACE_COMPILE_OPTIONS>")
endforeach()
target_compile_definitions(bbsolver_core PUBLIC BBSOLVER_HAVE_TBB=1)
if(EXISTS "${CMAKE_CURRENT_LIST_DIR}/src/dp/dp_placer.cpp")
  target_compile_definitions(bbsolver_core PUBLIC BBSOLVER_HAVE_DP_PLACER=1)
endif()

# MSVC-only hardening flags applied to bbsolver_core. POSIX compilers
# (gcc/clang) already provide these guarantees by default, so the block
# is fully MSVC-guarded.
#
#   /utf-8              Several solver source files contain UTF-8 characters
#                       in comments (em-dashes "—", "L∞", etc.). Without
#                       /utf-8 MSVC interprets source files using the
#                       system code page (CP-1252 on default Windows
#                       installs) and emits warning C4819 — or, under /WX,
#                       fails the build outright.
#   /bigobj             The Ceres + Eigen + nlohmann_json template stack
#                       can exceed the default 2^16 COFF section limit
#                       in individual TUs (notably segment_fit_ceres.cpp
#                       and segment_fit_unified_spatial.cpp). /bigobj
#                       raises the limit to 2^32 and is the recommended
#                       flag for template-heavy C++ on MSVC.
#   /Zc:__cplusplus     MSVC sets the `__cplusplus` macro to 199711L by
#                       default even under /std:c++17, which silently
#                       breaks portable code that gates on
#                       `__cplusplus >= 201703L`. /Zc:__cplusplus makes
#                       the macro reflect the active C++ standard.
if(MSVC)
  target_compile_options(bbsolver_core PRIVATE /utf-8 /bigobj /Zc:__cplusplus)
endif()

if(BBSOLVER_BUILD_CLI)
  add_executable(bbsolver "${CMAKE_CURRENT_LIST_DIR}/src/app/main.cpp")
  add_executable(bbsolver::bbsolver ALIAS bbsolver)
  set_target_properties(bbsolver PROPERTIES EXPORT_NAME bbsolver)
  target_link_libraries(bbsolver PRIVATE bbsolver_core ${BBSOLVER_BUILD_DEPENDENCY_TARGETS})
  if(MSVC)
    target_compile_options(bbsolver PRIVATE /utf-8 /bigobj /Zc:__cplusplus)
  endif()
endif()

configure_package_config_file(
  "${CMAKE_CURRENT_LIST_DIR}/cmake/bbsolverConfig.cmake.in"
  "${CMAKE_CURRENT_BINARY_DIR}/bbsolverConfig.cmake"
  INSTALL_DESTINATION "${BBSOLVER_INSTALL_CMAKEDIR}")
write_basic_package_version_file(
  "${CMAKE_CURRENT_BINARY_DIR}/bbsolverConfigVersion.cmake"
  VERSION "${PROJECT_VERSION}"
  COMPATIBILITY SameMajorVersion)

install(
  TARGETS bbsolver_core
  EXPORT bbsolverTargets
  LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
  ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
  INCLUDES DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")
if(BBSOLVER_BUILD_CLI)
  install(
    TARGETS bbsolver
    EXPORT bbsolverTargets
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")
endif()
install(
  DIRECTORY "${CMAKE_CURRENT_LIST_DIR}/include/"
  DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
  PATTERN ".DS_Store" EXCLUDE)
install(
  DIRECTORY "${BBSOLVER_GENERATED_NS_DIR}/"
  DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/bbsolver"
  FILES_MATCHING PATTERN "*.h")
install(
  DIRECTORY "${BBSOLVER_PROTOCOL_DIR}/"
  DESTINATION "${CMAKE_INSTALL_DATADIR}/bbsolver/protocol"
  FILES_MATCHING PATTERN "*.fbs")
install(
  DIRECTORY "${CMAKE_CURRENT_LIST_DIR}/docs/"
  DESTINATION "${CMAKE_INSTALL_DOCDIR}"
  FILES_MATCHING PATTERN "*.md")
install(
  DIRECTORY "${CMAKE_CURRENT_LIST_DIR}/examples/"
  DESTINATION "${CMAKE_INSTALL_DATADIR}/bbsolver/examples"
  PATTERN ".DS_Store" EXCLUDE)
install(
  DIRECTORY "${CMAKE_CURRENT_LIST_DIR}/third_party/"
  DESTINATION "${CMAKE_INSTALL_DATADIR}/bbsolver/third_party"
  FILES_MATCHING
    PATTERN "*.md"
    PATTERN "*.tar.gz"
    PATTERN "*.tar.xz"
    PATTERN ".DS_Store" EXCLUDE)
install(
  FILES
    "${CMAKE_CURRENT_LIST_DIR}/README.md"
    "${CMAKE_CURRENT_LIST_DIR}/CHANGELOG.md"
    "${CMAKE_CURRENT_LIST_DIR}/CONTRIBUTING.md"
    "${CMAKE_CURRENT_LIST_DIR}/LICENSE"
  DESTINATION "${CMAKE_INSTALL_DOCDIR}")
install(
  EXPORT bbsolverTargets
  NAMESPACE bbsolver::
  DESTINATION "${BBSOLVER_INSTALL_CMAKEDIR}")
install(
  FILES
    "${CMAKE_CURRENT_BINARY_DIR}/bbsolverConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/bbsolverConfigVersion.cmake"
  DESTINATION "${BBSOLVER_INSTALL_CMAKEDIR}")

if(BBSOLVER_BUILD_TESTS)
  enable_testing()
  file(GLOB BBSOLVER_UNIT_TESTS CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_LIST_DIR}/tests/solver_unit/*.cpp")
  foreach(BBSOLVER_UNIT_TEST_SRC IN LISTS BBSOLVER_UNIT_TESTS)
    get_filename_component(BBSOLVER_UNIT_TEST_NAME
      "${BBSOLVER_UNIT_TEST_SRC}" NAME_WE)
    set(BBSOLVER_UNIT_TEST_LABELS unit)
    if(BBSOLVER_UNIT_TEST_NAME MATCHES "(io|json|schema|verify|dump|cli|solve_command)")
      list(APPEND BBSOLVER_UNIT_TEST_LABELS io cli schema)
    endif()
    if(BBSOLVER_UNIT_TEST_NAME MATCHES "(motion|smooth)")
      list(APPEND BBSOLVER_UNIT_TEST_LABELS motion_smooth)
    endif()
    if(BBSOLVER_UNIT_TEST_NAME MATCHES "(path|shape|corner|vertex|multimode|replacement)")
      list(APPEND BBSOLVER_UNIT_TEST_LABELS path)
    endif()
    if(BBSOLVER_UNIT_TEST_NAME MATCHES "(temporal|segment|dp|static|plain_property)")
      list(APPEND BBSOLVER_UNIT_TEST_LABELS temporal)
    endif()
    if(BBSOLVER_UNIT_TEST_NAME MATCHES "(progress|diagnostic|lifecycle|cancellation)")
      list(APPEND BBSOLVER_UNIT_TEST_LABELS lifecycle)
    endif()
    if(BBSOLVER_UNIT_TEST_NAME STREQUAL "test_replacement_temporal_solver")
      list(APPEND BBSOLVER_UNIT_TEST_LABELS slow)
    endif()
    add_executable("${BBSOLVER_UNIT_TEST_NAME}"
      "${BBSOLVER_UNIT_TEST_SRC}")
    target_link_libraries("${BBSOLVER_UNIT_TEST_NAME}"
      PRIVATE bbsolver_core ${BBSOLVER_BUILD_DEPENDENCY_TARGETS})
    if(MSVC)
      # Test binaries inherit the bbsolver_core MSVC hardening so they
      # share the same UTF-8 interpretation, COFF section limit, and
      # __cplusplus macro behavior.
      target_compile_options("${BBSOLVER_UNIT_TEST_NAME}"
        PRIVATE /utf-8 /bigobj /Zc:__cplusplus)
    endif()
    add_test(
      NAME "${BBSOLVER_UNIT_TEST_NAME}"
      COMMAND "${BBSOLVER_UNIT_TEST_NAME}")
    set_tests_properties("${BBSOLVER_UNIT_TEST_NAME}" PROPERTIES
      LABELS "${BBSOLVER_UNIT_TEST_LABELS}"
      WORKING_DIRECTORY "${CMAKE_CURRENT_LIST_DIR}"
      ENVIRONMENT "BBSOLVER_TEST_SOURCE_DIR=${CMAKE_CURRENT_LIST_DIR}")
  endforeach()

  add_executable(test_package_smoke_source
    "${CMAKE_CURRENT_LIST_DIR}/tests/package_smoke/main.cpp")
  target_link_libraries(test_package_smoke_source
    PRIVATE bbsolver_core ${BBSOLVER_BUILD_DEPENDENCY_TARGETS})
  if(MSVC)
    target_compile_options(test_package_smoke_source
      PRIVATE /utf-8 /bigobj /Zc:__cplusplus)
  endif()
  add_test(NAME test_package_smoke_source COMMAND test_package_smoke_source)
  set_tests_properties(test_package_smoke_source PROPERTIES
    LABELS "package;smoke"
    WORKING_DIRECTORY "${CMAKE_CURRENT_LIST_DIR}"
    ENVIRONMENT "BBSOLVER_TEST_SOURCE_DIR=${CMAKE_CURRENT_LIST_DIR}")
endif()
