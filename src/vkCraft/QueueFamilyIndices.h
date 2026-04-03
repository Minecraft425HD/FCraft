#pragma once

#include <cstdint>

class QueueFamilyIndices
{
public:
	uint32_t graphicsFamily = UINT32_MAX;
	uint32_t presentFamily = UINT32_MAX;

	bool isComplete();
};
