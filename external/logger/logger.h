#pragma once

#include <cstdarg>
#include <cstdio>
#include <vector>

// Logs a debug message
#define LOG_DEBUG(...) \
  ::fan_controller::logger::Log(__FILE__, __LINE__, ::fan_controller::logger::Severity::DEBUG, __VA_ARGS__)

// Logs an informational message
#define LOG_INFO(...) \
  ::fan_controller::logger::Log(__FILE__, __LINE__, ::fan_controller::logger::Severity::INFO, __VA_ARGS__)

// Logs a warning
#define LOG_WARNING(...) \
  ::fan_controller::logger::Log(__FILE__, __LINE__, ::fan_controller::logger::Severity::WARNING, __VA_ARGS__)

// Logs an error
#define LOG_ERROR(...) \
  ::fan_controller::logger::Log(__FILE__, __LINE__, ::fan_controller::logger::Severity::ERROR, __VA_ARGS__)

namespace fan_controller {
namespace logger {

// Indicates the level of severity for a log message
enum class Severity {
  // A utility case which can be used for `SetSeverity` to disable all severity levels.
  NONE = -2,
  // A utility case which can be used for `Redirect` and `SetSeverity`
  ALL = -1,
  // The five different log severities in use from most severe to least severe.
  PANIC = 0,  // Need to start at 0
  ERROR,
  WARNING,
  INFO,
  DEBUG,
  // A utility case representing the number of log levels used internally.
  COUNT
};

// Function which is used for logging. It can be changed to intercept the logged messages.
extern void (*LoggingFunction)(const char* file, int line, Severity severity, const char* log);

// Redirects the output for a given log severity.
void Redirect(std::FILE* file, Severity severity = Severity::ALL);

// Sets global log severity thus effectively disabling all logging with lower severity
void SetSeverity(Severity severity);

// Converts the message and argument into a string and pass it to LoggingFunction.
template<typename... Args>
void Log(const char* file, int line, Severity severity, const char* txt, ...) {
  va_list args1;
  va_start(args1, txt);
  va_list args2;
  va_copy(args2, args1);
  std::vector<char> buf(1 + std::vsnprintf(NULL, 0, txt, args1));
  va_end(args1);
  std::vsnprintf(buf.data(), buf.size(), txt, args2);
  va_end(args2);
  LoggingFunction(file, line, severity, buf.data());
}

}  // namespace logger
}  // namespace fan_controller
