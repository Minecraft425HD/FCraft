#include <cstdlib>
#include <iostream>
#include "VkCraft.h"
#include "MenuRenderer.h"

int main()
{
    // -----------------------------------------------------------------------
    // Phase 1: Main menu (OpenGL window, no Vulkan yet)
    // -----------------------------------------------------------------------
    if (!glfwInit()) {
        std::cerr << "vkCraft: glfwInit failed\n";
        return EXIT_FAILURE;
    }

    // OpenGL 2.1 context for the menu renderer (immediate-mode GL)
    glfwWindowHint(GLFW_CLIENT_API,        GLFW_OPENGL_API);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
    glfwWindowHint(GLFW_RESIZABLE,         GLFW_FALSE);

    GLFWwindow* menuWin = glfwCreateWindow(854, 480, "FCraft", nullptr, nullptr);
    if (!menuWin) {
        std::cerr << "vkCraft: could not create menu window\n";
        glfwTerminate();
        return EXIT_FAILURE;
    }
    glfwMakeContextCurrent(menuWin);
    glfwSwapInterval(1);

    MenuRenderer menu;
    menu.init(menuWin);

    while (!menu.isInGame() && !glfwWindowShouldClose(menuWin)) {
        glfwPollEvents();
        menu.renderFrame();
    }

    WorldSettings ws = menu.settings;
    glfwDestroyWindow(menuWin);
    // Do NOT call glfwTerminate -- VkCraft needs GLFW.

    if (glfwWindowShouldClose(menuWin)) {
        glfwTerminate();
        return EXIT_SUCCESS;
    }

    // -----------------------------------------------------------------------
    // Phase 2: Vulkan game with the settings chosen in the menu
    // -----------------------------------------------------------------------
    VkCraft app;
    app.worldSeed      = (int)ws.seed;
    app.renderDistance = ws.renderDistance;

    try {
        app.run();
    }
    catch (const std::runtime_error& error) {
        std::cerr << "vkCraft: " << error.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
