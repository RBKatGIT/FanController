#include "controller.h"
#include <boost/interprocess/mapped_region.hpp>
#include <boost/interprocess/shared_memory_object.hpp>
#include <boost/thread.hpp>
#include "../logger/logger.h"
using namespace boost::interprocess;

namespace fan_controller {
namespace controller {

void Controller::CalculateRegisters() {
  // First calculate duty cycle factor and duy cycle offset.
  // All cases above or equl to TEMP_HUNDRED_PERCENT_CUTOFF
  // corresponds to 100 % PWM value
  float duty_cycle_base = 100.0f;
  float duty_cycle_offset = 0;
  if (TEMP_TWENTY_PERCENT_CUTOFF >= max_temp_)
    duty_cycle_base = 20.0f;  //  <=  25° corresponds to 20 % duty cycle
  else if (TEMP_HUNDRED_PERCENT_CUTOFF > max_temp_ &&
           TEMP_TWENTY_PERCENT_CUTOFF < max_temp_) {
    // 0% duty cycle change should be linearly interpolated between 25° and 75°
    // This means in that range every degree increase will correspond to
    // 80%/(75°-25°) = (80/50) = 1.6 duty cycle/degree change.
    duty_cycle_base = 20.0f;  // 100 % duty cycle
    duty_cycle_offset = ((max_temp_ - 25) * 1.6);
  }

  float duty_cyle_percentile = duty_cycle_base + duty_cycle_offset;
  LOG_INFO("New Duty cycle percentile:  %f\n", duty_cyle_percentile);
  {
    for (uint32_t i = 0; i < config_.fan_count_; i++) {
      registers_[i] =
          std::ceil((duty_cyle_percentile / 100) * config_.max_pwm_values[i]);
    }
  }
}

void Controller::ProcessSensors() {
  struct shm_remove {
    shm_remove() { shared_memory_object::remove(register_memory_name.c_str()); }
    ~shm_remove() {
      shared_memory_object::remove(register_memory_name.c_str());
    }
  } remover;
  // Create a shared memory object.
  shared_memory_object shm(open_or_create  // only create
                           ,
                           register_memory_name.c_str()  // name
                           ,
                           read_write  // read-write mode
  );
  shm.truncate(sizeof(register_shared_memory_buffer));
  // Map the whole shared memory in this process
  mapped_region region(shm, read_write);
  // Get the address of the mapped region
  void* addr = region.get_address();
  // Construct the shared structure in memory
  register_shared_memory_buffer* data =
      new (addr) register_shared_memory_buffer;
  while (true) {
    float new_max_temp = 0;
    {
      boost::mutex::scoped_lock scoped_lock(received_sensor_data_mutex);
      // Find the new max temperature
      new_max_temp = received_sensor_values_[0].value_;
      while (!sensors_changed_) new_sensor_data_cond_.wait(scoped_lock);
      new_max_temp = received_sensor_values_[0].value_;
      // New data has come lets process it
      sensors_changed_ = false;
      // Find the maximum temp value among all the sensors
      for (uint32_t i = 0; i < config_.sensor_count_; i++)
        new_max_temp =
            std::max(new_max_temp, received_sensor_values_[i].value_);
    }
    if (new_max_temp != max_temp_) {
      LOG_INFO("Old Max: %f, New max: : %f, update registers..\n", max_temp_,
               new_max_temp);
      max_temp_ = new_max_temp;

      // Now that we ave a new max temperature calculate the PWM
      // values that are needed for each register
      CalculateRegisters();

      // And Send them to shared memory for GUI
      data->nempty.wait();

      data->mutex.wait();
      for (uint32_t i = 0; i < config_.fan_count_; i++) {
        data->registers[i] = registers_[i];
        LOG_INFO("Sending Register %d: %d", i, registers_[i]);
      }
      data->mutex.post();
      data->nstored.post();
    } else {
      LOG_INFO("No register Update needed\n");
      LOG_INFO("Old Max: %f, New max: : %f\n", max_temp_, new_max_temp);
    }
  }
}

Controller::Controller(const InputConfiguration config) : config_(config) {
  registers_.resize(config.fan_count_);
  received_sensor_values_.resize(config.sensor_count_);
}

void Controller::ReceiveSensors() {
  // Create a shared memory object.
  shared_memory_object shm(open_only  // only create
                           ,
                           sensor_memory_name.c_str()  // name
                           ,
                           read_write  // read-write mode
  );
  // Map the whole shared memory in this process
  mapped_region region(shm  // What to map
                       ,
                       read_write  // Map it as read-write
  );
  // Get the address of the mapped region
  void* addr = region.get_address();

  // Obtain the shared structure
  sensor_shared_memory_buffer* data =
      static_cast<sensor_shared_memory_buffer*>(addr);

  // Below loop will be blocked untill we have data from GUI
  // this is ok, since this thread has nothing else to do :-)
  while (true) {
    // Read the sensor values
    data->nstored.wait();
    data->mutex.wait();
    {
      boost::mutex::scoped_lock scoped_lock(received_sensor_data_mutex);
      for (uint32_t i = 0; i < config_.sensor_count_; i++) {
        if (data->sensors[i].value_ != received_sensor_values_[i].value_) {
          received_sensor_values_[i].value_ = data->sensors[i].value_;
          LOG_INFO("Received sensor values %d:%f\n", i,
                   received_sensor_values_[i].value_);
          sensors_changed_ = true;
        }
      }
    }
    data->mutex.post();
    data->nempty.post();
    if (sensors_changed_) {
      new_sensor_data_cond_.notify_one();
    }
  }
}

bool StartController(const InputConfiguration config) {
  Controller* controller = new Controller(config);
  // Lets wait a second so GUI gets a chance to create the shared memory
  sleep(1);
  // Create input thread which will watch out for Sensor changes
  boost::thread* sensor_thread =
      new boost::thread(boost::bind(&Controller::ReceiveSensors, controller));
  controller->ProcessSensors();
  sensor_thread->join();
  return true;
}

}  // namespace controller
}  // namespace fan_controller