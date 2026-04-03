#include "Camera.h"

Camera::Camera(double _fov, double _near, double _far)
{
	fov = _fov;
	near = _near;
	far = _far;
}

void Camera::updateMatrix()
{
	// glm::mat4(1.0f) is the correct way to create an identity matrix
	matrix = glm::translate(glm::mat4(1.0f), position);
	matrix = glm::rotate(matrix, rotation.x, glm::vec3(1.0f, 0.0f, 0.0f));
	matrix = glm::rotate(matrix, rotation.z, glm::vec3(0.0f, 0.0f, 1.0f));
	matrix = glm::rotate(matrix, rotation.y, glm::vec3(0.0f, 1.0f, 0.0f));
	matrix = glm::scale(matrix, scale);
	matrix = glm::inverse(matrix);
}

void Camera::updateProjectionMatrix(float width, float height)
{
	aspect = width / height;
	projection = glm::perspective(glm::radians(fov), aspect, near, far);

	// Flip Y axis: Vulkan NDC has Y pointing down, OpenGL has Y pointing up
	projection[1][1] *= -1;
}
