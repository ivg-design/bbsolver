#include "oneapi/tbb/global_control.h"
#include "bbsolver/runtime/solve_parallel_runtime_scope.hpp"

#include <algorithm>
#include <cstddef>
#include <memory>

#ifdef BBSOLVER_HAVE_TBB
#include <tbb/global_control.h>
#endif

namespace bbsolver {

struct SolveParallelRuntimeScope::Impl {
#ifdef BBSOLVER_HAVE_TBB
  explicit Impl(int resolved_parallel_jobs)
      : control(tbb::global_control::max_allowed_parallelism,
                static_cast<std::size_t>(
                    std::max(1, resolved_parallel_jobs))) {}

  tbb::global_control control;
#endif
};

SolveParallelRuntimeScope::SolveParallelRuntimeScope(
    int resolved_parallel_jobs) {
#ifdef BBSOLVER_HAVE_TBB
  impl_ = std::make_unique<Impl>(resolved_parallel_jobs);
#else
  (void)resolved_parallel_jobs;
#endif
}

SolveParallelRuntimeScope::~SolveParallelRuntimeScope() = default;

}  // namespace bbsolver
