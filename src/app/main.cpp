#include <exception>
#include <iostream>
#include <string>

#include "bbsolver/app/cli_options.hpp"
#include "bbsolver/verify/verify_dump_commands.hpp"
#include "bbsolver/solve/solve_command.hpp"

int main(int argc, char** argv) {
  try {
    if (argc == 1 || std::string(argv[1]) == "--help" ||
        std::string(argv[1]) == "-h") {
      bbsolver::PrintUsage(std::cout);
      return 0;
    }
    if (std::string(argv[1]) == "--version") {
      std::cout << bbsolver::kBbsolverVersion << '\n';
      return 0;
    }

    const std::string command = argv[1];
    if (command == "solve") {
      return bbsolver::RunSolve(argc, argv);
    }
    if (command == "verify") {
      return bbsolver::RunVerifyCommand(argc, argv);
    }
    if (command == "dump") {
      return bbsolver::RunDumpCommand(argc, argv);
    }

    bbsolver::PrintUsage(std::cerr);
    return 2;
  } catch (const std::exception& e) {
    std::cerr << "bbsolver: " << e.what() << '\n';
    return 1;
  }
}
