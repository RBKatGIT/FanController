#pragma once

#include <SDL.h>
#include <boost/thread/condition.hpp>
#include <boost/thread/mutex.hpp>
#include <memory>
#include <mutex>
#include <vector>
#include "common.h"

// GUIWrapper created the GUI screen keeps checking for any updates on Sensor
// vaues and receves and update register values
// We strictly need one and only GUI- Make it singleton
// This has three threads, one will run the GUI on it
// sensor_thread - sends out values to sensor shared memory on a change
// register_thread - received data from register shared memory
namespace fan_controller {
namespace gui {

class GUIWrapper {
  // Configuration stays constant once initialized
  const InputConfiguration config_;

  // Latest set temperature values from GUI in degree celcius
  std::vector<Sensor> sensor_values_;

  // Last received FAN PWM counts from controller
  std::vector<u_int32_t> registers_;

  // Refer to imgui documention to to know more on windows
  SDL_Window* window_;

  // Refer to imgui documention to to know more gl_context
  SDL_GLContext gl_context_;

  // Window with the register values can be hiden if wanted
  bool show_output_window_ = true;

  // On/Off flag for the input window
  bool show_demo_window_ = false;

  // Set to send data to controller if sensor changed at gui
  // Start with a true for the first trigger at start up
  std::vector<int> changed_sensor_ids_;

  // Mutex for the above variable
  boost::mutex new_sensor_data_mutex_, received_register_data_mutex_;

  // Use a conditional variable to signal send to controller
  boost::condition_variable new_sensor_data_cond_;

  // Flag to denote if we need to exit
  bool done_ = false;

  // Private Constructor
  GUIWrapper(const InputConfiguration config);

  // Set up all SDL for GUI- Return true on success
  bool InitGUI();

  // Show window with Register outputs
  void ShowOutputWindow();

  // Display sensor inputs on screen
  void CheckSensorUpdates();

  // Receive register values from register shared memory
  void ReceiveRegisterValues();

  // Display latest received register values on screen
  void UpdateRegisterValues();

 public:
  GUIWrapper(GUIWrapper const& copy) = delete;             // Not Implemented
  GUIWrapper& operator=(GUIWrapper const& copy) = delete;  // Not Implemented
  static GUIWrapper& GetInstance(const InputConfiguration config);

  // Starts main loop for GUI which will checks and send the sensor inputs
  // Receive the register outputs and put in on GUI
  bool StartGui();
  // Clean up GUI stuff
  void StopGui();
  // Copies new sensor value from GUI to shared memory
  void SendSensorValues();
};

}  // namespace gui
}  // namespace fan_controller
