#include "bbsolver/domain.hpp"

int main() {
  bbsolver::SolverConfig config;
  return config.tolerance > 0.0 ? 0 : 1;
}
