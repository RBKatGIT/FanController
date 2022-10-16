#include "common.h"
#include <iostream>

InputConfiguration::InputConfiguration(uint32_t fan_count, uint32_t sensor_count)
    : fan_count_(fan_count), sensor_count_(sensor_count) {}