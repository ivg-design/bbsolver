#pragma once

#include <memory>

namespace bbsolver {

class SolveParallelRuntimeScope {
 public:
  explicit SolveParallelRuntimeScope(int resolved_parallel_jobs);
  ~SolveParallelRuntimeScope();

  SolveParallelRuntimeScope(const SolveParallelRuntimeScope&) = delete;
  SolveParallelRuntimeScope& operator=(const SolveParallelRuntimeScope&) =
      delete;

 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace bbsolver
