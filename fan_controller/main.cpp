
#include <fcntl.h>
#include <pthread.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>
#include <chrono>
#include <iostream>
#include <mutex>
#include <thread>
#include "../logger/logger.h"
#include "controller.h"
#include "gui_wrapper.h"
// Check if the fan and sensor count entered is a valid one
// Input is valid onlt if there is a vaid fan count and sensor count
// MAX values defined in common.h
bool IsValidInput(int argc, char *argv[]) {
  if (argc != 3) {
    LOG_ERROR("Usage: fan_controller sensor_count fan_count \n");
    return false;
  }
  if (*argv[1] == '-') {
    LOG_ERROR("Please enter a positive number for the fan and sensor counts\n");
    return false;
  }
  if (!(isdigit(*argv[1]) && isdigit(*argv[1]))) return false;
  uint32_t sensor_count = atoi(argv[1]);
  uint32_t fan_count = atoi(argv[2]);
  if (fan_count > MAX_FAN_COUNT || fan_count <= 0) {
    LOG_ERROR("Please enter valid fan count between 1 and %d\n", MAX_FAN_COUNT);
    return false;
  }
  if (sensor_count > MAX_SENSOR_COUNT || sensor_count <= 0) {
    LOG_ERROR("Please enter valid sensor count between 1 and %d\n",
              MAX_SENSOR_COUNT);
    return false;
  }
  return true;
}

bool isNumeric(std::string const &str) {
  return !str.empty() &&
         str.find_first_not_of("0123456789") == std::string::npos;
}

int main(int argc, char *argv[]) {
  // Check fan count and sensor count validity
  if (!IsValidInput(argc, argv)) {
    exit(-1);
  }
  uint32_t sensor_count = atoi(argv[1]);
  uint32_t fan_count = atoi(argv[2]);
  InputConfiguration configuration(fan_count, sensor_count);

  // Get maximum PWM values from user
  for (uint32_t i = 1; i <= fan_count; i++) {
    std::string in_string;
    LOG_INFO("Enter Max PWM for Fan %d\n", i);
    std::cin >> in_string;
    u_int32_t input_pwm;
    while (true) {
      try {
        if (!isNumeric(in_string) || std::stoi(in_string) < 0) {
          LOG_ERROR("Please enter a positive number");
          std::cin >> in_string;
          continue;
        }
      } catch (std::invalid_argument &e) {
        LOG_ERROR("Please enter a valid number");
        std::cin >> in_string;
        continue;
      } catch (std::out_of_range &e) {
        LOG_ERROR("Please enter a valid number");
        std::cin >> in_string;
        continue;
      }
      input_pwm = stoi(in_string);
      break;
    }

    configuration.max_pwm_values.push_back(input_pwm);
  }

  // Keep parent process ID to trigger a stop when the GUI stops
  pid_t ppid = getpid();
  pid_t pid = fork();
  if (pid == 0) {
    // Lets create GUI in the child process
    ::fan_controller::gui::GUIWrapper::GetInstance(configuration).StartGui();
    // If GUI is closed stop the parent proc
    // Right now no handler implemented there so this will just kill the parent
    // proc.
    kill(ppid, SIGINT);
  } else {
    // Controller runs in the parent process
    ::fan_controller::controller::StartController(configuration);
  }
  return 1;
}
