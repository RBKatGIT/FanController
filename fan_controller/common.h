#pragma once

#include <boost/interprocess/sync/interprocess_semaphore.hpp>
#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

// Change below values for testing
const uint32_t MAX_FAN_COUNT = 5;
const uint32_t MAX_SENSOR_COUNT = 5;

// Below this cut off temp in degree celcius fans run at 20% duty cycle
const int TEMP_TWENTY_PERCENT_CUTOFF = 25;

// Above this cut off temp in degree celcius fans run at 100% duty cycle
const float TEMP_HUNDRED_PERCENT_CUTOFF = 75.0f;

// Deliberately set to higher cutoff so if a sensor is faulty we kick
// in at 100 % duty cycle
const int DEFAULT_SENSOR_DEGREE = TEMP_HUNDRED_PERCENT_CUTOFF;

// Name for sensor shared memory
const std::string sensor_memory_name = "SensorShared";

// Name for register shared memory
const std::string register_memory_name = "RegisterShared";

// Configuration that main will pass to controller and GUI at start up
struct InputConfiguration {
  uint32_t fan_count_;
  uint32_t sensor_count_;
  std::vector<u_int32_t> max_pwm_values;
  InputConfiguration(uint32_t fan_count, uint32_t sensor_count);
};

struct Sensor {
  int32_t id_ = -1;
  float value_ = TEMP_HUNDRED_PERCENT_CUTOFF;
  Sensor(int32_t id = -1, float value = TEMP_HUNDRED_PERCENT_CUTOFF)
      : id_(id), value_(value) {}
};

// Shared memory buffer for sending sensor data from GUI to controller
struct sensor_shared_memory_buffer {
  uint32_t sensor_count = MAX_SENSOR_COUNT;
  sensor_shared_memory_buffer()
      : mutex(1), nempty(MAX_SENSOR_COUNT), nstored(0) {}

  // Semaphores to protect and synchronize access
  boost::interprocess::interprocess_semaphore mutex, nempty, nstored;

  // Items to fill
  Sensor sensors[MAX_SENSOR_COUNT];
};

// Shared memory buffer for sending fan register values from controller to
// GUI
struct register_shared_memory_buffer {
  uint32_t fan_count = MAX_FAN_COUNT;
  register_shared_memory_buffer()
      : mutex(1), nempty(MAX_FAN_COUNT), nstored(0) {}

  // Semaphores to protect and synchronize access
  boost::interprocess::interprocess_semaphore mutex, nempty, nstored;

  // Items to fill
  u_int32_t registers[MAX_FAN_COUNT];
};
