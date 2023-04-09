#include "juice_pump_gui.hpp"
#include "juice_pump.hpp"
#include "serial.hpp"
#include <imgui/imgui.h>

namespace ws {

gui::JuicePumpGUIResult gui::render_juice_pump_gui(const JuicePumpGUIParams& params) {
  gui::JuicePumpGUIResult result{};

  for (int i = 0; i < params.num_ports; i++) {
    if (ImGui::Button(params.serial_ports[i].port.c_str())) {
      ws::pump::initialize_pump_system(params.serial_ports[i].port, params.num_pumps);
    }
  }

  if (ImGui::Button("TerminateSystem")) {
    ws::pump::terminate_pump_system();
  }

  const auto enter_flag = ImGuiInputTextFlags_EnterReturnsTrue;

  if (ws::pump::num_initialized_pumps() > 0) {
    bool allow_run = params.allow_automated_run;
    if (ImGui::Checkbox("AllowAutomatedRun", &allow_run)) {
      result.allow_automated_run = allow_run;
    }
  }

  for (int i = 0; i < ws::pump::num_initialized_pumps(); i++) {
    auto pump_handle = ws::pump::ith_pump(i);

    std::string handle_label{"Pump"};
    handle_label += std::to_string(pump_handle.index);

    if (ImGui::TreeNode(handle_label.c_str())) {
      auto desired_pump_state = ws::pump::read_desired_pump_state(pump_handle);
      if (ImGui::InputInt("Address", &desired_pump_state.address)) {
        if (desired_pump_state.address >= 0) {
          ws::pump::set_address(pump_handle, desired_pump_state.address);
        }
      }
      if (ImGui::InputInt("Rate", &desired_pump_state.rate)) {
        if (desired_pump_state.rate >= 0) {
          ws::pump::set_pump_rate(
            pump_handle, desired_pump_state.rate, desired_pump_state.rate_units);
        }
      }
      if (ImGui::InputFloat("Volume", &desired_pump_state.volume, 0.0f, 0.0f, "%0.3f", enter_flag)) {
        if (desired_pump_state.volume >= 0.0f) {
          ws::pump::set_dispensed_volume(
            pump_handle, desired_pump_state.volume, desired_pump_state.volume_units);
        }
      }

      auto canonical_pump_state = ws::pump::read_canonical_pump_state(pump_handle);
      if (canonical_pump_state.connection_open) {
        if (ImGui::Button("Run")) {
          ws::pump::run_dispense_program(pump_handle);
        }
        if (ImGui::Button("Stop")) {
          ws::pump::stop_dispense_program(pump_handle);
        }
      } else {
        ImGui::Text("Connection is closed.");
      }

      ImGui::TreePop();
    }
  }

  return result;
}

}
