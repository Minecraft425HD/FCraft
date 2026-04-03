#pragma once

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/constants.hpp>

#include <GLFW/glfw3.h>
#include <cmath>

#include "Camera.h"

class FirstPersonCamera : public Camera
{
public:
	glm::dvec2 last = { 0.0, 0.0 };
	glm::dvec2 delta = { 0.0, 0.0 };
	glm::vec3 orientation = { 0.0f, 0.0f, 0.0f };

	/**
	 * Tracks whether last cursor position has been initialized.
	 * Prevents a large camera jump on the very first frame.
	 */
	bool lastInitialized = false;

	FirstPersonCamera();

	void update(GLFWwindow *window, double time);
	void updateMatrix();
};
