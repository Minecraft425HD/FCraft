#pragma once

/*
#define _CRTDBG_MAP_ALLOC
#include <crtdbg.h>
#ifdef _DEBUG
#define DEBUG_NEW new(_NORMAL_BLOCK, __FILE__, __LINE__)
#define new DEBUG_NEW
#endif
*/

#include <stdexcept>
#include <iostream>
#include "VkCraft.h"
#include "MainMenu.h"

int main()
{
	//_CrtSetReportMode(_CRT_ERROR, _CRTDBG_MODE_DEBUG);
	//_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);

	// -----------------------------------------------------------------------
	// Phase 1: Show main menu before initialising the Vulkan engine.
	// We need a temporary GLFW window for the menu input loop.
	// -----------------------------------------------------------------------
	if (!glfwInit())
	{
		std::cerr << "vkCraft: glfwInit failed" << std::endl;
		return EXIT_FAILURE;
	}

	// Create a minimal window just for menu input (no Vulkan surface yet)
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE); // hidden during menu
	GLFWwindow* menuWindow = glfwCreateWindow(800, 600, "FCraft", nullptr, nullptr);
	if (!menuWindow)
	{
		std::cerr << "vkCraft: could not create menu window" << std::endl;
		glfwTerminate();
		return EXIT_FAILURE;
	}

	MainMenu menu;
	menu.generateRandomSeed(); // pre-fill with a random seed

	// Menu loop: run until the player hits "Spielen" -> IN_GAME
	while (!menu.isInGame() && !glfwWindowShouldClose(menuWindow))
	{
		glfwPollEvents();
		menu.render(menuWindow);
	}

	glfwDestroyWindow(menuWindow);
	// Do NOT call glfwTerminate() here -- VkCraft will use GLFW too.

	if (glfwWindowShouldClose(menuWindow))
	{
		// Player chose "Beenden" from menu
		glfwTerminate();
		return EXIT_SUCCESS;
	}

	// -----------------------------------------------------------------------
	// Phase 2: Launch the Vulkan engine with the chosen world settings
	// -----------------------------------------------------------------------
	VkCraft app;
	app.worldSettings = menu.getSettings(); // pass settings into engine

	try
	{
		app.run();
	}
	catch (const std::runtime_error &error)
	{
		std::cerr << "vkCraft: " << error.what() << std::endl;
		return EXIT_FAILURE;
	}

	//_CrtDumpMemoryLeaks();

	return EXIT_SUCCESS;
}
