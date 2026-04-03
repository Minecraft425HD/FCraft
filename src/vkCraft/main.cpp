#include <cstdlib>
#include "VkCraft.h"

/*
 * CRT memory-leak debugging (Windows only, opt-in):
 *
 * #define _CRTDBG_MAP_ALLOC
 * #include <crtdbg.h>
 * #ifdef _DEBUG
 * #define DEBUG_NEW new(_NORMAL_BLOCK, __FILE__, __LINE__)
 * #define new DEBUG_NEW
 * #endif
 *
 * Usage in main():
 *   _CrtSetReportMode(_CRT_ERROR, _CRTDBG_MODE_DEBUG);
 *   _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
 *   // ... at the end:
 *   _CrtDumpMemoryLeaks();
 */

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
