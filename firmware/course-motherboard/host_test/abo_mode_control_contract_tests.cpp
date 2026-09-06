#include <cassert>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

namespace {

std::string read_source(const char* relative_path) {
  std::ifstream input(std::string("../") + relative_path, std::ios::binary);
  assert(input.good());
  std::ostringstream contents;
  contents << input.rdbuf();
  return contents.str();
}

void require(bool condition, const char* message, bool* all_passed) {
  if (condition) return;
  std::cerr << "RED: " << message << '\n';
  *all_passed = false;
}

void host_link_exposes_web_mode_command() {
  const auto header = read_source("abo_host/include/abo_host/host_link.h");
  const auto source = read_source("abo_host/host_link.cpp");
  assert(header.find("set_mode_request_handler") != std::string::npos);
  assert(source.find("std::strcmp(command, \"set_mode\")") !=
         std::string::npos);
  assert(source.find("mode_not_standby") != std::string::npos);
}

void runtime_and_app_bind_web_mode_command() {
  const auto runtime = read_source("main/platform/performance_runtime.h");
  const auto app = read_source("main/abo_app_main.cpp");
  assert(runtime.find("bool set_mode(") != std::string::npos);
  assert(app.find("set_mode_request_handler") != std::string::npos);
}

void host_link_exposes_web_volume_command(bool* all_passed) {
  const auto header = read_source("abo_host/include/abo_host/host_link.h");
  const auto source = read_source("abo_host/host_link.cpp");
  require(header.find("VolumeRequestHandler") != std::string::npos,
          "HostLink exposes a volume request handler", all_passed);
  require(header.find("set_volume_request_handler") != std::string::npos,
          "HostLink registers a volume request handler", all_passed);
  require(source.find("std::strcmp(command, \"set_volume\")") !=
              std::string::npos,
          "HostLink parses the set_volume command", all_passed);
}

void runtime_and_app_bind_web_volume_command(bool* all_passed) {
  const auto runtime = read_source("main/platform/performance_runtime.h");
  const auto app = read_source("main/abo_app_main.cpp");
  require(runtime.find("bool set_volume(") != std::string::npos,
          "PerformanceRuntime exposes volume control", all_passed);
  require(app.find("set_volume_request_handler") != std::string::npos,
          "app_main binds the volume request handler", all_passed);
}

}  // namespace

int main() {
  host_link_exposes_web_mode_command();
  runtime_and_app_bind_web_mode_command();
  bool all_passed = true;
  host_link_exposes_web_volume_command(&all_passed);
  runtime_and_app_bind_web_volume_command(&all_passed);
  return all_passed ? 0 : 1;
}
