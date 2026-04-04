#include "VkCraft.h"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

VkResult VkCraft::CreateDebugReportCallbackEXT(VkInstance instance, const VkDebugReportCallbackCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugReportCallbackEXT* pCallback)
{
	auto func = (PFN_vkCreateDebugReportCallbackEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugReportCallbackEXT");

	if (func != nullptr)
	{
		return func(instance, pCreateInfo, pAllocator, pCallback);
	}
	else
	{
		return VK_ERROR_EXTENSION_NOT_PRESENT;
	}
}

void VkCraft::DestroyDebugReportCallbackEXT(VkInstance instance, VkDebugReportCallbackEXT callback, const VkAllocationCallbacks* pAllocator)
{
	auto func = (PFN_vkDestroyDebugReportCallbackEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugReportCallbackEXT");

	if (func != nullptr)
	{
		func(instance, callback, pAllocator);
	}
}

void VkCraft::run()
{
	initialize();
	loop();
	cleanup();
}

void VkCraft::loop()
{
	while (!glfwWindowShouldClose(window))
	{
		glfwPollEvents();
		update();
		render();
	}

	vkDeviceWaitIdle(device.logical);
}

void VkCraft::update()
{
	double actual = glfwGetTime();
	delta = actual - time;
	time = actual;

	//Model matrix
	model.updateMatrix();

	//First person camera
	camera.update(window, delta);
	camera.updateProjectionMatrix((float)swapChainExtent.width, (float)swapChainExtent.height);

	glm::ivec3 index = world->getIndex(camera.position);

	if (index != cameraIndex)
	{
		double t = glfwGetTime();

		// ── Step 1: graph traversal (main thread, fast) ──────────────────────
		// Build the full neighbour graph for the render distance and register
		// every node in world's flat registry so worker threads can look them
		// up via world->getBlock() without graph traversal.
		std::vector<ChunkNode*> chunkNodes = world->collectNodes(camera.position, renderDistance);

		// ── Step 2: parallel DATA generation ────────────────────────────────
		// Each ChunkNode owns its own chunk.data – no shared writes.
		// generateData() is idempotent (atomic CAS guards double-execution).
		{
			std::vector<std::future<void>> futures;
			futures.reserve(chunkNodes.size());

			for (ChunkNode *node : chunkNodes)
			{
				if (node->state.load() < ChunkNode::DATA)
				{
					futures.push_back(threadPool->enqueue([node]() {
						node->generateData();
					}));
				}
			}
			// Wait for all data to be ready before meshing.
			for (auto &f : futures) f.get();
		}

		// ── Step 3: parallel GEOMETRY generation ────────────────────────────
		// All nodes now have DATA. generateGeometry() calls world->getBlock()
		// which is thread-safe (reads from registry with a mutex).
		// generateGeometry() is also idempotent (mutex + atomic state guard).
		{
			std::vector<std::future<void>> futures;
			futures.reserve(chunkNodes.size());

			for (ChunkNode *node : chunkNodes)
			{
				if (node->state.load() < ChunkNode::GEOMETRY)
				{
					futures.push_back(threadPool->enqueue([node, this]() {
						node->generateGeometry(world);
					}));
				}
			}
			for (auto &f : futures) f.get();
		}

		// ── Step 4: GPU upload (main thread only) ────────────────────────────
		// Vulkan buffer creation must happen on the thread that owns the device.
		world->geometries.clear();
		for (ChunkNode *node : chunkNodes)
		{
			createGeometryBuffers(node->geometry);
			world->geometries.push_back(node->geometry);
		}

		std::cout << "VkCraft: Loaded " << chunkNodes.size() << " chunks in "
		          << (glfwGetTime() - t) << "s  ("
		          << threadPool->threadCount() << " worker threads)\n";

		// ── Step 5: Rebuild draw commands ────────────────────────────────────
		recreateRenderingCommandBuffers();
		cameraIndex = index;
	}

	//Update UBO
	uniformBuf.model = model.matrix;
	uniformBuf.view = camera.matrix;
	uniformBuf.projection = camera.projection;

	//Copy UBO data
	void* data;
	vkMapMemory(device.logical, uniformBufferMemory, 0, sizeof(uniformBuf), 0, &data);
	memcpy(data, &uniformBuf, sizeof(uniformBuf));
	vkUnmapMemory(device.logical, uniformBufferMemory);
}

void VkCraft::render()
{
	// Wait for the previous frame using this slot to finish.
	// This ensures imageAvailableSemaphores[currentFrame] is no longer pending
	// before we pass it to vkAcquireNextImageKHR again.
	vkWaitForFences(device.logical, 1, &inFlightFences[currentFrame], VK_TRUE, std::numeric_limits<uint64_t>::max());

	uint32_t imageIndex;
	VkResult result = vkAcquireNextImageKHR(device.logical, swapChain, std::numeric_limits<uint64_t>::max(), imageAvailableSemaphores[currentFrame], VK_NULL_HANDLE, &imageIndex);

	if (result == VK_ERROR_OUT_OF_DATE_KHR)
	{
		recreateSwapChain();
		return;
	}
	else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
	{
		throw std::runtime_error("vkCraft: Failed to acquire swap chain image!");
	}

	// Reset fence only after confirmed we will submit work this frame
	vkResetFences(device.logical, 1, &inFlightFences[currentFrame]);

	//Create list of semaphores to wait for and signal to
	VkSemaphore waitSemaphores[] = { imageAvailableSemaphores[currentFrame] };
	// Index by imageIndex (not currentFrame): each swapchain image has its own
	// renderFinished semaphore so we don't reuse it while the presentation engine
	// still holds a reference from a previous present of that image.
	VkSemaphore signalSemaphores[] = { renderFinishedSemaphores[imageIndex] };

	//Wait stages
	VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };

	//Submit information (which semaphores to wait for, wait stages etc)
	VkSubmitInfo submitInfo = {};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.waitSemaphoreCount = 1;
	submitInfo.pWaitSemaphores = waitSemaphores;
	submitInfo.pWaitDstStageMask = waitStages;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &renderCommandBuffers[imageIndex];
	submitInfo.signalSemaphoreCount = 1;
	submitInfo.pSignalSemaphores = signalSemaphores;

	//Submit render request to queue
	if (vkQueueSubmit(graphicsQueue, 1, &submitInfo, inFlightFences[currentFrame]) != VK_SUCCESS)
	{
		throw std::runtime_error("vkCraft: Failed to submit draw command buffer");
	}

	//Present info (waits for signal semaphore)
	VkPresentInfoKHR presentInfo = {};
	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	presentInfo.waitSemaphoreCount = 1;
	presentInfo.pWaitSemaphores = signalSemaphores;

	VkSwapchainKHR swapChains[] = { swapChain };
	presentInfo.swapchainCount = 1;
	presentInfo.pSwapchains = swapChains;
	presentInfo.pImageIndices = &imageIndex;
	presentInfo.pResults = nullptr;

	//Present the frame
	result = vkQueuePresentKHR(presentQueue, &presentInfo);

	if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR)
	{
		recreateSwapChain();
	}
	else if (result != VK_SUCCESS)
	{
		throw std::runtime_error("vkCraft: Failed to present swap chain image!");
	}

	currentFrame = (currentFrame + 1) % CONCURRENT_FRAMES;
}

void VkCraft::initialize()
{
	//Create window
	glfwInit();
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
	window = glfwCreateWindow(1024, 600, "VkCraft", nullptr, nullptr);

	//Create instance
	createInstance();

	//Prepare debug callbacks
	setupDebugCallback();

	//Create window surface
	createSurface();

	//Pick a device
	pickPhysicalDevice();
	createLogicalDevice();

	//Create swap chain
	createSwapChain();

	//Image views
	createImageViews();

	//Create render pass
	createRenderPass();

	//Graphics pipeline
	createDescriptorSetLayout();
	createGraphicsPipeline();

	//Command pool
	createCommandPool(&commandPool);
	createCommandPool(&renderCommandPool);

	//Depth buffer
	createDepthResources();

	//Framebuffers
	createFramebuffers();

	//Texture data
	createTextureImage("texture/minecraft.png");
	texture.createSampler(device.logical, textureSampler);

	//Create uniform buffer
	VkDeviceSize bufferSize = sizeof(UniformBufferObject);
	BufferUtils::createBuffer(device, bufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, uniformBuffer, uniformBufferMemory);

	//Descriptors
	createDescriptorPool();
	createDescriptorSet();

	// Initialise ChunkWorld with the seed set by main.cpp (via menu)
	world = new ChunkWorld(worldSeed);

	//Command buffers
	createRenderingCommandBuffers();

	//Semaphores
	createSyncObjects();

	// ── Thread pool ───────────────────────────────────────────────────────────
	// Use all hardware threads minus one (keep one for the main/render thread).
	// Fall back to 1 worker if only a single core is detected.
	unsigned int hw = std::thread::hardware_concurrency();
	unsigned int workers = (hw > 1) ? (hw - 1) : 1;
	threadPool = std::make_unique<ThreadPool>(workers);
	std::cout << "VkCraft: ThreadPool started with " << workers << " worker threads "
	          << "(hardware_concurrency=" << hw << ")\n";
}
