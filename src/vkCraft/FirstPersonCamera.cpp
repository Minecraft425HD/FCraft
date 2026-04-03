#include "FirstPersonCamera.h"

FirstPersonCamera::FirstPersonCamera()
{
	position.z = 5.0f;
}

void FirstPersonCamera::update(GLFWwindow *window, double time)
{
	double moveSpeed = 10.0 * time;
	double lookSpeed = 1.0 * time;

	double pi    = glm::pi<double>();
	double pid2  = pi / 2.0;
		
	if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS)
	{
		moveSpeed *= 3.0f;
	}

	// Camera move
	if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
	{
		position.z -= moveSpeed * std::cos(orientation.x);
		position.x -= moveSpeed * std::sin(orientation.x);
	}
	if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
	{
		position.z += moveSpeed * std::cos(orientation.x);
		position.x += moveSpeed * std::sin(orientation.x);
	}

	float lateral = orientation.x + (float)pid2;

	if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
	{
		position.z += moveSpeed * std::cos(lateral);
		position.x += moveSpeed * std::sin(lateral);
	}
	if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
	{
		position.z -= moveSpeed * std::cos(lateral);
		position.x -= moveSpeed * std::sin(lateral);
	}

	if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS)
	{
		position.y += moveSpeed;
	}
	if (glfwGetKey(window, GLFW_KEY_C) == GLFW_PRESS)
	{
		position.y -= moveSpeed;
	}

	double x, y;
	glfwGetCursorPos(window, &x, &y);
	delta.x = x - last.x;
	delta.y = y - last.y;
	last.x = x;
	last.y = y;

	// Update mouse look
	if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS)
	{
		orientation.x -= delta.x * lookSpeed;
		orientation.y -= delta.y * lookSpeed;
	}

	// Camera orientation via arrow keys
	if (glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS)
	{
		orientation.x += lookSpeed;
	}
	if (glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS)
	{
		orientation.x -= lookSpeed;
	}
	if (glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS)
	{
		orientation.y += lookSpeed;
	}
	if (glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS)
	{
		orientation.y -= lookSpeed;
	}

	// Clamp vertical look to ±90°
	const float pitchLimit = glm::half_pi<float>();
	if (orientation.y < -pitchLimit)
	{
		orientation.y = -pitchLimit;
	}
	else if (orientation.y > pitchLimit)
	{
		orientation.y = pitchLimit;
	}

	updateMatrix();
}

void FirstPersonCamera::updateMatrix()
{
	matrix = glm::translate(glm::mat4(1.0f), position);
	matrix = glm::rotate(matrix, orientation.x, glm::vec3(0.0f, 1.0f, 0.0f));
	matrix = glm::rotate(matrix, orientation.y, glm::vec3(1.0f, 0.0f, 0.0f));
	matrix = glm::inverse(matrix);
}
