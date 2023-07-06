//
// Created by yu on 5/2/21.
//

#ifndef INC_2021_TEMPLATE_LOG_H
#define INC_2021_TEMPLATE_LOG_H

#include "basic.h"

static std::string file_debug = "debug.log";
[[maybe_unused]] static std::string file_log = "log.log";

static std::string get_time() {
  time_t current_time;
  current_time = time(nullptr);
  std::string time = ctime(&current_time);
  time = time.substr(0, time.size() - 1);
  return time;
}

static std::string print_stack_trace() {
  std::string str;
  llvm::raw_string_ostream dump(str);
  llvm::sys::PrintStackTrace(dump);
  return str;
}

// number: 0(debug), 1(info), 2(warning), 3(error)
static void yhao_log(int64_t type, const std::string &message) {
  if (type < DEBUG_LEVEL) {
    return;
  }
  switch (type) {
    case -1: {
#if ERR
      std::cerr << "low debug: " << get_time() << ": " << message << std::endl;
#else
      std::ofstream log(file_debug, std::ios_base::app);
      log << "low debug: " << get_time() << ": " << message << std::endl;
      log.close();
#endif
      break;
    }
    case 0: {
#if ERR
      std::cerr << "debug: " << get_time() << ": " << message << std::endl;
#else
      std::ofstream log(file_debug, std::ios_base::app);
      log << "debug: " << get_time() << ": " << message << std::endl;
      log.close();
#endif
      break;
    }
    case 1: {
#if ERR
      std::cerr << "info: " << get_time() << " : " << message << std::endl;
#else
      std::ofstream log(file_debug, std::ios_base::app);
      log << "info: " << get_time() << " : " << message << std::endl;
      log.close();
#endif
      break;
    }
    case 2: {
#if ERR
      std::cerr << "warning: " << get_time() << " : " << message << std::endl;
#else
      std::ofstream log(file_debug, std::ios_base::app);
      log << "warning: " << get_time() << " : " << message << std::endl;
      log.close();
#endif
      break;
    }
    case 3: {
#if ERR
      std::cerr << "error: " << get_time() << " : " << message << std::endl;
      std::cerr << print_stack_trace() << std::endl;

#else
      std::ofstream log(file_debug, std::ios_base::app);
      log << "error: " << get_time() << " : " << message << std::endl;
      log << print_stack_trace() << std::endl;
      log.close();
#endif
      break;
    }
    default: {
#if ERR
      std::cerr << get_time() << ": " << message << std::endl;
#else
      std::ofstream log(file_debug, std::ios_base::app);
      log << get_time() << ": " << message << std::endl;
      log.close();
#endif
    }
  }
}

static void yhao_start_log() {
#if ERR
  std::cerr << "info: " << get_time() << std::endl;
#else
  std::ofstream log(file_debug);
  log << "info: " << get_time() << std::endl;
  log.close();
#endif
}

[[maybe_unused]] static void start_log(char **argv) {
  llvm::sys::PrintStackTraceOnErrorSignal(argv[0]);
  yhao_start_log();
}

#endif  // INC_2021_TEMPLATE_LOG_H
