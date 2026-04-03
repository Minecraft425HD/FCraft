#include <cstdlib>
#include "VkCraft.h"

int main()
{
	VkCraft app;
	
	try
	{
		app.run();
	}
	catch (const std::runtime_error &error)
	{
		std::cerr << "vkCraft: " << error.what() << std::endl;
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
