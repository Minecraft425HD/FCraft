#include "QueueFamilyIndices.h"

bool QueueFamilyIndices::isComplete()
{
	return graphicsFamily != UINT32_MAX && presentFamily != UINT32_MAX;
}
