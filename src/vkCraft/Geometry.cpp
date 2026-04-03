#include "Geometry.h"

void Geometry::generate(){}

bool Geometry::hasBuffers()
{
	return vertexBuffer != VK_NULL_HANDLE && indexBuffer != VK_NULL_HANDLE;
}

void Geometry::applyTransformationMatrix(glm::mat4 *matrix)
{
	for (int i = 0; i < (int)vertices.size(); i++)
	{
		glm::vec4 result = *matrix * glm::vec4(vertices[i].pos, 1.0f);
			
		vertices[i].pos.x = result.x;
		vertices[i].pos.y = result.y;
		vertices[i].pos.z = result.z;
	}
}

void Geometry::merge(Geometry *geometry)
{
	int initialSize = (int)vertices.size();

	for (int i = 0; i < (int)geometry->vertices.size(); i++)
	{
		vertices.push_back(geometry->vertices[i]);
	}

	for (int i = 0; i < (int)geometry->indices.size(); i++)
	{
		indices.push_back(geometry->indices[i] + initialSize);
	}
}

void Geometry::dispose(VkDevice &device)
{
	vkDestroyBuffer(device, vertexBuffer, nullptr);
	vkFreeMemory(device, vertexBufferMemory, nullptr);
	vkDestroyBuffer(device, indexBuffer, nullptr);
	vkFreeMemory(device, indexBufferMemory, nullptr);

	// Null out handles so a second dispose() call is a safe no-op
	// (vkDestroy* on VK_NULL_HANDLE is defined to do nothing).
	vertexBuffer = VK_NULL_HANDLE;
	vertexBufferMemory = VK_NULL_HANDLE;
	indexBuffer = VK_NULL_HANDLE;
	indexBufferMemory = VK_NULL_HANDLE;
}
