#pragma once

#include <boost/thread/condition.hpp>
#include <boost/thread/mutex.hpp>
#include <cstdint>
#include <memory>
#include <vector>
#include "common.h"
// Controller class
// 1. Receive the sensor values from sensor memory
// 2. Process them to find the duty cycle
// 3. Calculate the needed register values for each fan and write to register
// memory Assumptions: necessary error checking for fan_count and sensor_count
// has aleady been taken care by the GUI.
// Controller runs 2 threads, Main thread will process the latest received
// Sensor values and send the results to shared register memory
// sensor_thread watches and receives sensor shared memory changes

namespace fan_controller {
namespace controller {

class Controller {
  // Configuration stays constant once initialized
  const InputConfiguration config_;

  // Latest set temperature values from GUI in degree celcius
  std::vector<Sensor> received_sensor_values_;

  bool sensors_changed_ = false;

  // Mutex for the above two data structures
  boost::mutex received_sensor_data_mutex;

  // Last set FAN PWM counts from controller
  std::vector<u_int32_t> registers_;

  // PWM corresponding to 100 percent duty cycle for each fan
  const std::vector<u_int32_t> max_pwm_values_;

  // To Signal new data processing
  boost::condition_variable new_sensor_data_cond_;

  // Current known max temperature
  float max_temp_ = TEMP_HUNDRED_PERCENT_CUTOFF;

 public:
  Controller(const InputConfiguration config);

  // Runs in a thread and monitor sensor value changes
  void ReceiveSensors();

  // Calculate new max_temp_, trigger CalculateRegisters,
  // send the latest register values to shared memory
  void ProcessSensors();

  // Updates the PWM values in registers_ based on max_temp_
  void CalculateRegisters();
};
bool StartController(const InputConfiguration config);

}  // namespace controller
}  // namespace fan_controller
