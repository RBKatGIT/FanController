// This code is adopted from the sample example files from
// https://github.com/ocornut/imgui Dear ImGui: standalone example application
// for SDL2 + OpenGL (SDL is a cross-platform general purpose library for
// handling windows, inputs, OpenGL/Vulkan/Metal graphics context creation,
// etc.) If you are new to Dear ImGui, read documentation from the docs/ folder
// + read the top of imgui.cpp. Read online:
// https://github.com/ocornut/imgui/tree/master/docs

#include "gui_wrapper.h"
#include <math.h>
#include <stdio.h>
#include <sys/msg.h>
#include <boost/interprocess/mapped_region.hpp>
#include <boost/interprocess/shared_memory_object.hpp>
#include <boost/thread.hpp>
#include <boost/thread/mutex.hpp>
#include <iostream>
#include "../imgui/backends/imgui_impl_opengl3.h"
#include "../imgui/backends/imgui_impl_sdl.h"
#include "../imgui/imgui.h"
#include "../logger/logger.h"
#if defined(IMGUI_IMPL_OPENGL_ES2)
#include <SDL_opengles2.h>
#else
#include <SDL_opengl.h>
#endif
using namespace boost::interprocess;
namespace fan_controller {
namespace gui {

GUIWrapper::GUIWrapper(const InputConfiguration config) : config_(config) {
  sensor_values_.resize(config.sensor_count_);
  for (auto i : config_.max_pwm_values) registers_.push_back(i);
}

GUIWrapper &GUIWrapper::GetInstance(const InputConfiguration config) {
  static GUIWrapper instance(config);
  return instance;
}

bool GUIWrapper::InitGUI() {
  // Setup SDL
  // (Some versions of SDL before <2.0.10 appears to have performance/stalling
  // issues on a minority of Windows systems, depending on whether
  // SDL_INIT_GAMECONTROLLER is enabled or disabled.. updating to latest version
  // of SDL is recommended!)
  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_GAMECONTROLLER) !=
      0) {
    LOG_ERROR("Error: %s\n", SDL_GetError());
    return false;
  }

  // Decide GL+GLSL versions
#if defined(IMGUI_IMPL_OPENGL_ES2)
  // GL ES 2.0 + GLSL 100
  const char *glsl_version = "#version 100";
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
#elif defined(__APPLE__)
  // GL 3.2 Core + GLSL 150
  const char *glsl_version = "#version 150";
  SDL_GL_SetAttribute(
      SDL_GL_CONTEXT_FLAGS,
      SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG);  // Always required on Mac
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);
#else
  // GL 3.0 + GLSL 130
  const char *glsl_version = "#version 130";
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
#endif

  // Create window with graphics context
  SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
  SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
  SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
  SDL_WindowFlags window_flags = (SDL_WindowFlags)(
      SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
  window_ = SDL_CreateWindow("Fan Controller", SDL_WINDOWPOS_CENTERED,
                             SDL_WINDOWPOS_CENTERED, 1280, 720, window_flags);
  gl_context_ = SDL_GL_CreateContext(window_);
  SDL_GL_MakeCurrent(window_, gl_context_);
  SDL_GL_SetSwapInterval(1);  // Enable vsync

  // Setup Dear ImGui context
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();

  // Setup Dear ImGui style
  ImGui::StyleColorsDark();

  // Setup Platform/Renderer backends
  ImGui_ImplSDL2_InitForOpenGL(window_, gl_context_);
  ImGui_ImplOpenGL3_Init(glsl_version);
  return true;
}

void GUIWrapper::StopGui() {
  // Cleanup
  ImGui_ImplOpenGL3_Shutdown();
  ImGui_ImplSDL2_Shutdown();
  ImGui::DestroyContext();

  SDL_GL_DeleteContext(gl_context_);
  SDL_DestroyWindow(window_);
  SDL_Quit();
}

void GUIWrapper::ShowOutputWindow() {
  if (show_output_window_) {
    ImGui::Begin(
        "Output Window",
        &show_output_window_);  // Pass a pointer to our bool variable (the
                                // window will have a closing button that will
                                // clear the bool when clicked)
    ImGui::Text("Registers");
    ImGui::NewLine();
    ImGui::NewLine();
    UpdateRegisterValues();
    ImGui::End();
  }
}

void GUIWrapper::SendSensorValues() {
  struct shm_remove {
    shm_remove() { shared_memory_object::remove(sensor_memory_name.c_str()); }
    ~shm_remove() { shared_memory_object::remove(sensor_memory_name.c_str()); }
  } remover;
  // Create a shared memory object.
  shared_memory_object shm(open_or_create  // only create
                           ,
                           sensor_memory_name.c_str()  // name
                           ,
                           read_write  // read-write mode
  );
  shm.truncate(sizeof(sensor_shared_memory_buffer));
  // Map the whole shared memory in this process
  mapped_region region(shm, read_write);
  // Get the address of the mapped region
  void *addr = region.get_address();
  // Construct the shared structure in memory
  sensor_shared_memory_buffer *data = new (addr) sensor_shared_memory_buffer;

  // Send data to controller if sensor has changed
  while (!done_) {
    {
      boost::mutex::scoped_lock scoped_lock(new_sensor_data_mutex_);
      while (changed_sensor_ids_.empty())
        new_sensor_data_cond_.wait(scoped_lock);
      data->mutex.wait();
      for (auto id : changed_sensor_ids_) {
        data->sensors[id].value_ = sensor_values_[id].value_;
        LOG_INFO("Sending %d: %f", id, sensor_values_[id].value_);
      }
      data->mutex.post();
      data->nstored.post();
      changed_sensor_ids_.clear();
    }
  }
}

void GUIWrapper::CheckSensorUpdates() {
  {
    boost::mutex::scoped_lock scoped_lock(new_sensor_data_mutex_);
    for (uint32_t i = 0; i < config_.sensor_count_; i++) {
      std::string sensor_name = "Sensor " + std::to_string(i);
      float new_temp = sensor_values_[i].value_;
      // static float new_temp1 = 0.001f;
      ImGui::InputFloat(sensor_name.c_str(), &new_temp, 0.1f, 2.0f, "%.3f");

      if (std::isinf(new_temp)) {
        ImGui::Text("PLEASE ENTER A VALID NUMBER");
      } else if (new_temp != sensor_values_[i].value_) {
        Sensor sensor(i, new_temp);
        sensor_values_[i].value_ = new_temp;
        changed_sensor_ids_.push_back(i);
      }
    }
    if (!changed_sensor_ids_.empty()) new_sensor_data_cond_.notify_one();
  }
}

void GUIWrapper::ReceiveRegisterValues() {
  // We need a small delayto make sre we are opening this shared memory after
  // controller has created it
  sleep(1);
  // Open the shared memory object.
  shared_memory_object shm(open_only  // only create
                           ,
                           register_memory_name.c_str()  // name
                           ,
                           read_write  // read-write mode
  );
  // Map the whole shared memory in this process
  mapped_region region(shm  // What to map
                       ,
                       read_write  // Map it as read-write
  );
  // Get the address of the mapped region
  void *addr = region.get_address();
  // Obtain the shared structure
  register_shared_memory_buffer *data =
      static_cast<register_shared_memory_buffer *>(addr);
  while (true) {
    // Read the register values
    data->nstored.wait();
    data->mutex.wait();
    {
      boost::mutex::scoped_lock scoped_lock(received_register_data_mutex_);
      for (uint32_t i = 0; i < config_.fan_count_; i++) {
        if (data->registers[i] != registers_[i]) {
          registers_[i] = data->registers[i];
          LOG_INFO("Received register values %d:%f\n", i, registers_[i]);
        }
      }
    }
    data->mutex.post();
    data->nempty.post();
  }
}

void GUIWrapper::UpdateRegisterValues() {
  boost::mutex::scoped_lock scoped_lock(received_register_data_mutex_);
  for (uint32_t i = 0; i < config_.fan_count_; i++) {
    ImGui::Text("Register %d = %d", i, registers_[i]);
  }
}

bool GUIWrapper::StartGui() {
  // Main loop

  if (!InitGUI()) {
    LOG_ERROR("GUI initialization failed\n");
    return false;
  }

  // Lets start a thread which will send data to controller when sensor_changed_
  // is true
  boost::thread *sensor_thread =
      new boost::thread(boost::bind(&GUIWrapper::SendSensorValues, this));

  // Lets start a thread to receive register inputs from controller
  boost::thread *register_thread =
      new boost::thread(boost::bind(&GUIWrapper::ReceiveRegisterValues, this));

  // For select GUI color element on screen
  // Set colors for the gui
  ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);
  ImGuiIO &io = ImGui::GetIO();
  (void)io;

  while (!done_) {
    // Poll and handle events (inputs, window resize, etc.)
    // You can read the io.WantCaptureMouse, io.WantCaptureKeyboard flags to
    // tell if dear imgui wants to use your inputs.
    // - When io.WantCaptureMouse is true, do not dispatch mouse input data to
    // your main application, or clear/overwrite your copy of the mouse data.
    // - When io.WantCaptureKeyboard is true, do not dispatch keyboard input
    // data to your main application, or clear/overwrite your copy of the
    // keyboard data. Generally you may always pass all inputs to dear imgui,
    // and hide them from your application based on those two flags.
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
      ImGui_ImplSDL2_ProcessEvent(&event);
      if (event.type == SDL_QUIT) done_ = true;
      if (event.type == SDL_WINDOWEVENT &&
          event.window.event == SDL_WINDOWEVENT_CLOSE &&
          event.window.windowID == SDL_GetWindowID(window_)) {
        LOG_INFO("Closing the app.\n");
        done_ = true;
      }
    }

    // Start the Dear ImGui frame
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplSDL2_NewFrame();
    ImGui::NewFrame();

    // 1. Show the big demo window
    if (show_demo_window_) ImGui::ShowDemoWindow(&show_demo_window_);

    // 2. Show a simple window that we create ourselves. We use a Begin/End pair
    // to created a named window.
    ImGui::Begin("Sensor Inputs");  // Create a window
                                    // and append into it.

    ImGui::Text("Sensor settings");  // Display text

    // bools storing output window open/close state
    ImGui::Checkbox("Output Window", &show_output_window_);

    // Lets Check if sensors have something new to report
    CheckSensorUpdates();

    ImGui::NewLine();
    ImGui::NewLine();
    ImGui::NewLine();
    ImGui::SameLine();
    ImGui::End();

    // Show output window.
    ShowOutputWindow();

    // Rendering
    ImGui::Render();
    glViewport(0, 0, (int)io.DisplaySize.x, (int)io.DisplaySize.y);
    glClearColor(clear_color.x * clear_color.w, clear_color.y * clear_color.w,
                 clear_color.z * clear_color.w, clear_color.w);
    glClear(GL_COLOR_BUFFER_BIT);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    SDL_GL_SwapWindow(window_);
  }
  sensor_thread->interrupt();
  register_thread->interrupt();
  StopGui();
  return true;
}

}  // namespace gui
}  // namespace fan_controller
