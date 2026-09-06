#include <cassert>
#include <fstream>
#include <sstream>
#include <string>

namespace {

std::string read_source(const char* path) {
  std::ifstream input(path, std::ios::binary);
  assert(input.good());
  std::ostringstream contents;
  contents << input.rdbuf();
  return contents.str();
}

std::string between(const std::string& source,
                    const char* begin_marker,
                    const char* end_marker) {
  const auto begin = source.find(begin_marker);
  assert(begin != std::string::npos);
  const auto end = source.find(end_marker, begin + std::string(begin_marker).size());
  assert(end != std::string::npos);
  return source.substr(begin, end - begin);
}

void release_entry_is_explicit_and_transport_isolated() {
  const auto root_cmake = read_source("CMakeLists.txt");
  const auto main_cmake = read_source("main/CMakeLists.txt");
  const auto defaults = read_source("sdkconfig.defaults");

  assert(root_cmake.find("ABO_PRODUCT") != std::string::npos);
  assert(main_cmake.find("abo_app_main.cpp") != std::string::npos);
  const auto product_sources = between(main_cmake, "if(ABO_PRODUCT", "else()");
  assert(product_sources.find("abo_app_main.cpp") != std::string::npos);
  for (const auto* forbidden : {
           "platform/usb_hid.cpp",
           "platform/ble_hid.cpp",
           "esp_hid",
           "espressif__esp_tinyusb",
           " bt",
       }) {
    assert(product_sources.find(forbidden) == std::string::npos);
  }

  assert(defaults.find("CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG=y") !=
         std::string::npos);
  assert(defaults.find("CONFIG_USJ_ENABLE_USB_SERIAL_JTAG=y") !=
         std::string::npos);
  assert(defaults.find("CONFIG_BT_ENABLED=y") == std::string::npos);
  assert(defaults.find("CONFIG_BT_NIMBLE_ENABLED=y") == std::string::npos);

  const auto app_main = read_source("main/abo_app_main.cpp");
  assert(app_main.find("extern \"C\" void app_main(void)") !=
         std::string::npos);
  assert(app_main.find("host_link") != std::string::npos);
  assert(app_main.find("UsbHidTransport") == std::string::npos);
  assert(app_main.find("BleHidTransport") == std::string::npos);
  for (const auto* retained : {
           "platform/gpio_keys.cpp",
           "platform/led_strip_status.cpp",
           "platform/speaker_output.cpp",
           "platform/peripheral_power.cpp",
       }) {
    assert(product_sources.find(retained) != std::string::npos);
  }
}

}  // namespace

int main() {
  release_entry_is_explicit_and_transport_isolated();
}
