#pragma once

#include <optional>

namespace ws {
struct PortDescriptor;
}

namespace ws::gui {

struct JuicePumpGUIParams {
  const ws::PortDescriptor* serial_ports;
  int num_ports;
  int num_pumps;
  bool allow_automated_run;
};

struct JuicePumpGUIResult {
  std::optional<bool> allow_automated_run;
};

JuicePumpGUIResult render_juice_pump_gui(const JuicePumpGUIParams& params);

}