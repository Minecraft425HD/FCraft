#pragma once

#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <cmath>

#include "Geometry.h"

class FirstPersonCamera
{
public:
	glm::vec3 position;
	glm::vec2 orientation;
	glm::vec2 last;
	glm::vec2 delta;
	glm::mat4 matrix;

	/**
	 * Tracks whether last cursor position has been initialized.
	 * Prevents a large delta jump on the very first frame or after
	 * the window regains focus.
	 */
	bool lastInitialized = false;

	FirstPersonCamera();

	void update(GLFWwindow *window, double time);
	void updateMatrix();
};
