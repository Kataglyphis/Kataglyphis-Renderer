# define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/mat4x4.hpp>

#include <iostream>
#include <stdexcept>
#include <vector>
#include <memory>
#include "MyWindow.h"

#include "VulkanRenderer.h"

int main() {

    int window_width = 1200;
    int window_height = 768;

    std::shared_ptr<MyWindow> main_window = std::make_shared<MyWindow>(window_width, window_height);
    main_window->initialize();

    VulkanRenderer vulkan_renderer{};
    if (vulkan_renderer.init(main_window) == EXIT_FAILURE) {
        return EXIT_FAILURE;
    }

    float angle = 0.0f;
    float delta_time = 0.0f;
    float last_time = 0.0f;

    while (!main_window->get_should_close()) {
    
        //poll all events incoming from user
        glfwPollEvents();

        float now = glfwGetTime();
        delta_time = now - last_time;
        last_time = now;

        angle += 10.f * delta_time;
        if (angle > 360.f) {
            angle = 0.0f;
        }

        vulkan_renderer.update_model(glm::rotate(glm::mat4(1.0f), glm::radians(angle), glm::vec3(0.0f,0.0f,1.0f)));
        vulkan_renderer.draw();
        //main_window->swap_buffers();

    }

    vulkan_renderer.clean_up();

    return EXIT_SUCCESS;

}

