#include "input/window/window.hpp"
#include "utility/fixed_frequency_loop/fixed_frequency_loop.hpp"

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <iostream>
#include <vector>
#include <iostream>

struct Vec3 {
    float x = 0;
    float y = 0;
    float z = 0;

    std::string to_string() const {
        std::ostringstream oss;
        oss << "Vec3(" << x << ", " << y << ", " << z << ")";
        return oss.str();
    }

    Vec3 operator+(const Vec3 &other) const { return {x + other.x, y + other.y, z + other.z}; }

    Vec3 &operator+=(const Vec3 &other) {
        x += other.x;
        y += other.y;
        z += other.z;
        return *this;
    }

    Vec3 operator*(float scalar) const { return {x * scalar, y * scalar, z * scalar}; }
};

#include <sstream>
#include <string>

struct StateUpdateData {
    bool up_pressed = false;
    bool down_pressed = false;
    bool left_pressed = false;
    bool right_pressed = false;
    double dt = 0.0;

    // Convert pressed keys into a movement vector
    Vec3 get_delta_pos() const {
        Vec3 movement;
        if (up_pressed)
            movement.y += 1.0f;
        if (down_pressed)
            movement.y -= 1.0f;
        if (left_pressed)
            movement.x -= 1.0f;
        if (right_pressed)
            movement.x += 1.0f;
        return movement * static_cast<float>(dt);
    }

    std::string to_string() const {
        std::ostringstream oss;
        oss << "StateUpdateData("
            << "up=" << up_pressed << ", down=" << down_pressed << ", left=" << left_pressed
            << ", right=" << right_pressed << ", dt=" << dt << ", delta_pos=" << get_delta_pos().to_string() << ")";
        return oss.str();
    }
};

struct State {
    Vec3 position;
};

State state;
StateUpdateData state_update_data{};

struct KeyCallbackArgs {

    int key;
    int scancode;
    int action;
    int mods;
};

std::optional<KeyCallbackArgs> glfwPollEvents_produced_function_call;
void user_key_callback(GLFWwindow *window, int key, int scancode, int action, int mods) {
    // Print for debugging
    std::cout << "[User] Key " << key << " action " << action << " mods " << mods << "\n";

    glfwPollEvents_produced_function_call = KeyCallbackArgs{key, scancode, action, mods};

    bool pressed = (action == GLFW_PRESS || action == GLFW_REPEAT);

    switch (key) {
    case GLFW_KEY_W:
    case GLFW_KEY_UP:
        state_update_data.up_pressed = pressed;
        break;
    case GLFW_KEY_S:
    case GLFW_KEY_DOWN:
        state_update_data.down_pressed = pressed;
        break;
    case GLFW_KEY_A:
    case GLFW_KEY_LEFT:
        state_update_data.left_pressed = pressed;
        break;
    case GLFW_KEY_D:
    case GLFW_KEY_RIGHT:
        state_update_data.right_pressed = pressed;
        break;
    default:
        break;
    }
}

enum class Mode { RECORDING, PLAYBACK };

struct GLFWPlayback {

    GLFWPlayback() {}

    Mode mode = Mode::RECORDING;

    GLFWPlayback(const std::vector<KeyCallbackArgs> &key_callback_call_history)
        : key_callback_call_history(key_callback_call_history), poll_events_call_number(0) {}

    std::vector<std::optional<KeyCallbackArgs>> glfwPollEvents_produced_function_call_history;
    size_t glfwPollEvents_produced_function_call_history_playback_idx = 0;

    // this function call produces other function calls, we need to reproduce those function calls that were generated.
    void glfwPollEvents_() {
        if (mode == Mode::RECORDING) {
            std::cout << "doing poll events now" << std::endl;
            glfwPollEvents();
            // causes callbacks to run and then
            glfwPollEvents_produced_function_call_history.push_back(glfwPollEvents_produced_function_call);
            glfwPollEvents_produced_function_call = std::nullopt; // reset for next time its called
        } else if (mode == Mode::PLAYBACK) {
            auto args = glfwPollEvents_produced_function_call_history
                [glfwPollEvents_produced_function_call_history_playback_idx];
            if (args) {
                user_key_callback(nullptr, args->key, args->scancode, args->action, args->mods);
            }
            glfwPollEvents_produced_function_call_history_playback_idx++;
        }
    }
    std::vector<KeyCallbackArgs> key_callback_call_history;
    size_t poll_events_call_number;

    std::vector<bool> glfw_window_should_close_values;
    size_t playback_idx = 0;
    bool glfwWindowShouldClose_(GLFWwindow *window) {
        if (mode == Mode::RECORDING) {
            int should_close = glfwWindowShouldClose(window);
            glfw_window_should_close_values.push_back(should_close);
            return should_close;
        } else if (mode == Mode::PLAYBACK) {
            int should_close = glfw_window_should_close_values[playback_idx];
            playback_idx++;
            return should_close;
        }
        return false;
    }
};

struct FixedFrequencyLoopPlaybackSystem {

    std::vector<double> dt_history;

    void start(const std::function<void(double)> &rate_limited_func,
               const std::function<bool()> &termination_condition_func) {

        int i = 0;
        while (i < dt_history.size()) {
            std::cout << "i: " << i << " dths " << dt_history.size() << std::endl;
            double dt = dt_history[i];
            rate_limited_func(dt);
            i++;
        }
    }
};

struct glfwWindowShouldClosePlayback {};

int main() {
    if (!glfwInit())
        return -1;

    GLFWwindow *window = glfwCreateWindow(640, 480, "GLFW Input Recorder/Playback", nullptr, nullptr);
    if (!window) {
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window);
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cout << "Failed to initialize GLAD" << std::endl;
        throw std::runtime_error("failed to initialize GLAD");
    }
    glfwSwapInterval(1);

    // Set recording callbacks
    glfwSetKeyCallback(window, user_key_callback);
    // glfwSetMouseButtonCallback(window, recording_mouse_callback);

    int max_ticks = 200;
    bool recording_done = false;

    FixedFrequencyLoop ffl;
    FixedFrequencyLoopPlaybackSystem fflps;
    GLFWPlayback glfw_playback;

    size_t current_tick = 0;

    Mode mode = Mode::RECORDING;

    auto tick = [&](double dt) {
        glViewport(0, 0, 640, 480);
        glClear(GL_COLOR_BUFFER_BIT);

        glfw_playback.glfwPollEvents_();

        if (mode == Mode::RECORDING) {
            fflps.dt_history.push_back(dt);
        }
        state_update_data.dt = dt;

        std::cout << "on tick: " << current_tick << " the state before updating was was: " << state.position.to_string()
                  << std::endl;
        std::cout << "on tick: " << current_tick << " the state update data was: " << state_update_data.to_string()
                  << std::endl;
        state.position += state_update_data.get_delta_pos();
        std::cout << "on tick: " << current_tick << " the state after updating was was: " << state.position.to_string()
                  << std::endl;

        recording_done = current_tick == 300;

        // glClear(GL_COLOR_BUFFER_BIT); this causes a segfault rn, not sure why, but i don't need it anyways
        glfwSwapBuffers(window);
        current_tick++;
    };
    auto term = [&]() -> bool { return glfw_playback.glfwWindowShouldClose_(window) || recording_done; };
    ffl.start(tick, term);

    // reset the internal state.
    current_tick = 0;
    state = State{};
    state_update_data = StateUpdateData{};

    mode = Mode::PLAYBACK;
    glfw_playback.mode = Mode::PLAYBACK;
    fflps.start(tick, term);

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
