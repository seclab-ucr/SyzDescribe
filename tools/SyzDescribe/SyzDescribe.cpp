//
// Created by yhao on 3/3/21.
//

#include "../../lib/ManagerLib/Manager.h"
#include "../../lib/ToolLib/basic.h"
#include "../../lib/ToolLib/log.h"

llvm::cl::opt<std::string> config(
    "config", llvm::cl::desc("The configuration json file."),
    llvm::cl::value_desc("file name"), llvm::cl::init("./config.json"));

int main(int argc, char **argv) {
  llvm::cl::ParseCommandLineOptions(argc, argv, "");
  start_log(argv);

  auto manager = new sd::Manager();
  manager->setup(config);
  manager->analysis();

  return 0;
}
