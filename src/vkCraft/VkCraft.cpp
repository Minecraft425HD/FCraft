#include "VkCraft.h"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

// ─── Debug Utils helpers ───────────────────────────────────────────────────

VkResult VkCraft::CreateDebugUtilsMessengerEXT(
	VkInstance instance,
	const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo,
	const VkAllocationCallbacks* pAllocator,
	VkDebugUtilsMessengerEXT* pMessenger)
{
	auto func = (PFN_vkCreateDebugUtilsMessengerEXT)
		vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
	if (func != nullptr)
		return func(instance, pCreateInfo, pAllocator, pMessenger);
	return VK_ERROR_EXTENSION_NOT_PRESENT;
}

void VkCraft::DestroyDebugUtilsMessengerEXT(
	VkInstance instance,
	VkDebugUtilsMessengerEXT messenger,
	const VkAllocationCallbacks* pAllocator)
{
	auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)
		vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
	if (func != nullptr)
		func(instance, messenger, pAllocator);
}

void VkCraft::populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo)
{
	createInfo = {};
	createInfo.sType           = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
	createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT
	                           | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
	createInfo.messageType     = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT
	                           | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT
	                           | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
	createInfo.pfnUserCallback = debugCallback;
}

VKAPI_ATTR VkBool32 VKAPI_CALL VkCraft::debugCallback(
	VkDebugUtilsMessageSeverityFlagBitsEXT /*messageSeverity*/,
	VkDebugUtilsMessageTypeFlagsEXT /*messageType*/,
	const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
	void* /*pUserData*/)
{
	std::cerr << "vkCraft validation: " << pCallbackData->pMessage << std::endl;
	return VK_FALSE;
}

// ─── Core lifecycle ────────────────────────────────────────────────────────

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

	glm::ivec3 index = world.getIndex(camera.position);

	//Create geometries buffers
	if (index != cameraIndex)
	{
		double t = glfwGetTime();

		std::vector<Geometry*> geometries = world.getGeometries(camera.position, RENDER_DISTANCE);

		for (int i = 0; i < (int)geometries.size(); i++)
		{
			createGeometryBuffers(geometries[i]);
		}

		std::cout << "VkCraft: Got " << geometries.size() << " geometries." << std::endl;

		recreateRenderingCommandBuffers();
		cameraIndex = index;

		std::cout << "VkCraft: Get world geometries took " << (glfwGetTime() - t) << "s." << std::endl;
	}

	//Update UBO
	uniformBuf.model      = model.matrix;
	uniformBuf.view       = camera.matrix;
	uniformBuf.projection = camera.projection;

	//Copy UBO data
	void* data;
	vkMapMemory(device.logical, uniformBufferMemory, 0, sizeof(uniformBuf), 0, &data);
	memcpy(data, &uniformBuf, sizeof(uniformBuf));
	vkUnmapMemory(device.logical, uniformBufferMemory);
}

void VkCraft::render()
{
	//Wait for fences before starting to draw a frame
	vkWaitForFences(device.logical, 1, &inFlightFences[currentFrame], VK_TRUE, std::numeric_limits<uint64_t>::max());
	vkResetFences(device.logical, 1, &inFlightFences[currentFrame]);

	//Render next image and set image available semaphore down
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

	//Create list of semaphores to wait for and signal to
	VkSemaphore waitSemaphores[]   = { imageAvailableSemaphores[currentFrame] };
	VkSemaphore signalSemaphores[] = { renderFinishedSemaphores[currentFrame] };

	//Wait stages
	VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };

	//Submit information (which semaphores to wait for, wait stages etc)
	VkSubmitInfo submitInfo = {};
	submitInfo.sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.waitSemaphoreCount   = 1;
	submitInfo.pWaitSemaphores      = waitSemaphores;
	submitInfo.pWaitDstStageMask    = waitStages;
	submitInfo.commandBufferCount   = 1;
	submitInfo.pCommandBuffers      = &renderCommandBuffers[imageIndex];
	submitInfo.signalSemaphoreCount = 1;
	submitInfo.pSignalSemaphores    = signalSemaphores;

	//Submit render request to queue
	if (vkQueueSubmit(graphicsQueue, 1, &submitInfo, inFlightFences[currentFrame]) != VK_SUCCESS)
	{
		throw std::runtime_error("vkCraft: Failed to submit draw command buffer");
	}

	//Present info (waits for signal semaphore)
	VkPresentInfoKHR presentInfo = {};
	presentInfo.sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	presentInfo.waitSemaphoreCount = 1;
	presentInfo.pWaitSemaphores    = signalSemaphores;

	VkSwapchainKHR swapChains[]  = { swapChain };
	presentInfo.swapchainCount   = 1;
	presentInfo.pSwapchains      = swapChains;
	presentInfo.pImageIndices    = &imageIndex;
	presentInfo.pResults         = nullptr;

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

	//Prepare debug messenger
	setupDebugMessenger();

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

	//Command buffers
	createRenderingCommandBuffers();

	//Semaphores
	createSyncObjects();
}

void VkCraft::recreateSwapChain()
{
	int width, height;
	glfwGetWindowSize(window, &width, &height);

	if (width == 0 || height == 0)
	{
		return;
	}

	vkDeviceWaitIdle(device.logical);

	//Clean up swap chain
	cleanupSwapChain();

	//Recreate new swap chain
	createCommandPool(&renderCommandPool);
	createSwapChain();
	createImageViews();
	createRenderPass();
	createGraphicsPipeline();
	createDepthResources();
	createFramebuffers();
	createRenderingCommandBuffers();
}

void VkCraft::recreateRenderingCommandBuffers()
{
	vkDeviceWaitIdle(device.logical);

	vkDestroyCommandPool(device.logical, renderCommandPool, nullptr);
	createCommandPool(&renderCommandPool);
	createRenderingCommandBuffers();
}

void VkCraft::cleanupSwapChain()
{
	//Render command pool
	vkDestroyCommandPool(device.logical, renderCommandPool, nullptr);

	//Depth buffer data
	depth.dispose(&device.logical);

	for (VkFramebuffer framebuffer : swapChainFramebuffers)
	{
		vkDestroyFramebuffer(device.logical, framebuffer, nullptr);
	}

	//Pipeline
	vkDestroyPipeline(device.logical, graphicsPipeline, nullptr);
	vkDestroyPipelineLayout(device.logical, pipelineLayout, nullptr);

	//Render pass
	vkDestroyRenderPass(device.logical, renderPass, nullptr);

	//Image view
	for (VkImageView imageView : swapChainImageViews)
	{
		vkDestroyImageView(device.logical, imageView, nullptr);
	}

	//Swap chain
	vkDestroySwapchainKHR(device.logical, swapChain, nullptr);
}

void VkCraft::cleanup()
{
	//Wait for the device to idle before cleaning up
	vkDeviceWaitIdle(device.logical);

	//Clean swap chain
	cleanupSwapChain();

	//Texture sampler
	vkDestroySampler(device.logical, textureSampler, nullptr);

	//Texture data
	texture.dispose(&device.logical);

	//Descriptors
	vkDestroyDescriptorPool(device.logical, descriptorPool, nullptr);
	vkDestroyDescriptorSetLayout(device.logical, descriptorSetLayout, nullptr);

	//Buffers
	vkDestroyBuffer(device.logical, uniformBuffer, nullptr);
	vkFreeMemory(device.logical, uniformBufferMemory, nullptr);

	//World geometries
	world.dispose(device.logical);

	//Semaphores
	for (int i = 0; i < CONCURRENT_FRAMES; i++)
	{
		vkDestroySemaphore(device.logical, renderFinishedSemaphores[i], nullptr);
		vkDestroySemaphore(device.logical, imageAvailableSemaphores[i], nullptr);
		vkDestroyFence(device.logical, inFlightFences[i], nullptr);
	}

	//Command pool
	vkDestroyCommandPool(device.logical, commandPool, nullptr);

	device.dispose();

	if (ENABLE_VALIDATION_LAYERS)
	{
		DestroyDebugUtilsMessengerEXT(instance, debugMessenger, nullptr);
	}

	vkDestroySurfaceKHR(instance, surface, nullptr);
	vkDestroyInstance(instance, nullptr);

	//GLFW destroy
	glfwDestroyWindow(window);
	glfwTerminate();
}

void VkCraft::createInstance()
{
	if (ENABLE_VALIDATION_LAYERS && !checkValidationLayerSupport())
	{
		throw std::runtime_error("vkCraft: Validation layers requested are not available");
	}

	VkApplicationInfo appInfo = {};
	appInfo.sType              = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	appInfo.pApplicationName   = "VkCraft";
	appInfo.pEngineName        = "No Engine";
	appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
	appInfo.engineVersion      = VK_MAKE_VERSION(1, 0, 0);
	appInfo.apiVersion         = VK_API_VERSION_1_0;

	VkInstanceCreateInfo createInfo = {};
	createInfo.sType            = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	createInfo.pApplicationInfo = &appInfo;

	std::vector<const char*> extensions = getRequiredExtensions();
	createInfo.enabledExtensionCount   = static_cast<uint32_t>(extensions.size());
	createInfo.ppEnabledExtensionNames = extensions.data();

	//Print available GLFW extensions
	fprintf(stdout, "GLFW Extensions:\n");
	for (int i = 0; i < (int)extensions.size(); i++)
	{
		fprintf(stdout, "\t%s\n", extensions[i]);
	}

	// Chain debug messenger into instance creation so we catch
	// errors that happen during vkCreateInstance / vkDestroyInstance.
	VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};
	if (ENABLE_VALIDATION_LAYERS)
	{
		createInfo.enabledLayerCount   = static_cast<uint32_t>(validationLayers.size());
		createInfo.ppEnabledLayerNames = validationLayers.data();

		populateDebugMessengerCreateInfo(debugCreateInfo);
		createInfo.pNext = &debugCreateInfo;
	}
	else
	{
		createInfo.enabledLayerCount = 0;
		createInfo.pNext             = nullptr;
	}

	if (vkCreateInstance(&createInfo, nullptr, &instance) != VK_SUCCESS)
	{
		throw std::runtime_error("vkCraft: Failed to create vulkan instance");
	}

	//Print available Vulkan extensions
	uint32_t extensionCount = 0;
	vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);

	std::vector<VkExtensionProperties> vkExtensions(extensionCount);
	vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, vkExtensions.data());

	fprintf(stdout, "\nVulkan extensions:\n");
	for (uint32_t i = 0; i < vkExtensions.size(); i++)
	{
		fprintf(stdout, "\t%s\n", vkExtensions[i].extensionName);
	}
}

void VkCraft::setupDebugMessenger()
{
	if (!ENABLE_VALIDATION_LAYERS)
	{
		return;
	}

	VkDebugUtilsMessengerCreateInfoEXT createInfo{};
	populateDebugMessengerCreateInfo(createInfo);

	if (CreateDebugUtilsMessengerEXT(instance, &createInfo, nullptr, &debugMessenger) != VK_SUCCESS)
	{
		throw std::runtime_error("vkCraft: Failed to set up debug messenger");
	}
}

void VkCraft::createSurface()
{
	if (glfwCreateWindowSurface(instance, window, nullptr, &surface) != VK_SUCCESS)
	{
		throw std::runtime_error("vkCraft: Failed to create window surface");
	}
}

void VkCraft::pickPhysicalDevice()
{
	uint32_t count = 0;

	if (vkEnumeratePhysicalDevices(instance, &count, nullptr) != VK_SUCCESS || count == 0)
	{
		throw std::runtime_error("vkCraft: No GPU with Vulkan support found");
	}

	std::vector<VkPhysicalDevice> devices(count);
	vkEnumeratePhysicalDevices(instance, &count, devices.data());

	for (VkPhysicalDevice physical : devices)
	{
		VkPhysicalDeviceProperties vpdp;
		vkGetPhysicalDeviceProperties(physical, &vpdp);

		fprintf(stdout, "\n%s:\n", vpdp.deviceName);
		fprintf(stdout, "\tPhysical Device Type: %d ", vpdp.deviceType);
		if (vpdp.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)   fprintf(stdout, " (Discrete GPU)\n");
		if (vpdp.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU) fprintf(stdout, " (Integrated GPU)\n");
		if (vpdp.deviceType == VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU)    fprintf(stdout, " (Virtual GPU)\n");
		if (vpdp.deviceType == VK_PHYSICAL_DEVICE_TYPE_CPU)            fprintf(stdout, " (CPU)\n");
		fprintf(stdout, "\tPipeline Cache Size: %d\n",  vpdp.pipelineCacheUUID[0]);
		fprintf(stdout, "\tAPI version:         %d\n",  vpdp.apiVersion);
		fprintf(stdout, "\tDriver version:      %d\n",  vpdp.driverVersion);
		fprintf(stdout, "\tVendor ID:           0x%04x\n", vpdp.vendorID);
		fprintf(stdout, "\tDevice ID:           0x%04x\n", vpdp.deviceID);

		//Check if device is suitable
		if (isPhysicalDeviceSuitable(physical))
		{
			device.physical = physical;
			break;
		}
	}

	if (device.physical == VK_NULL_HANDLE)
	{
		throw std::runtime_error("vkCraft: Could not find a suitable GPU");
	}
}

void VkCraft::createLogicalDevice()
{
	QueueFamilyIndices indices = device.getQueueFamilyIndices(surface);

	std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
	std::set<int> uniqueQueueFamilies = { indices.graphicsFamily, indices.presentFamily };

	float queuePriority = 1.0f;
	for (int queueFamily : uniqueQueueFamilies)
	{
		VkDeviceQueueCreateInfo queueCreateInfo = {};
		queueCreateInfo.sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		queueCreateInfo.queueFamilyIndex = queueFamily;
		queueCreateInfo.queueCount       = 1;
		queueCreateInfo.pQueuePriorities = &queuePriority;
		queueCreateInfos.push_back(queueCreateInfo);
	};

	VkPhysicalDeviceFeatures deviceFeatures = {};
	deviceFeatures.samplerAnisotropy = VK_TRUE;

	VkDeviceCreateInfo createInfo = {};
	createInfo.sType                   = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	createInfo.queueCreateInfoCount    = static_cast<uint32_t>(queueCreateInfos.size());
	createInfo.pQueueCreateInfos       = queueCreateInfos.data();
	createInfo.pEnabledFeatures        = &deviceFeatures;
	createInfo.enabledExtensionCount   = static_cast<uint32_t>(deviceExtensions.size());
	createInfo.ppEnabledExtensionNames = deviceExtensions.data();

	if (ENABLE_VALIDATION_LAYERS)
	{
		createInfo.enabledLayerCount   = static_cast<uint32_t>(validationLayers.size());
		createInfo.ppEnabledLayerNames = validationLayers.data();
	}
	else
	{
		createInfo.enabledLayerCount = 0;
	}

	if (vkCreateDevice(device.physical, &createInfo, nullptr, &device.logical) != VK_SUCCESS)
	{
		throw std::runtime_error("vkCraft: Failed to create the logical device");
	}

	vkGetDeviceQueue(device.logical, indices.graphicsFamily, 0, &graphicsQueue);
	vkGetDeviceQueue(device.logical, indices.presentFamily,  0, &presentQueue);
}

void VkCraft::createSwapChain()
{
	SwapChainSupportDetails swapChainSupport = querySwapChainSupport(device.physical);

	VkSurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(swapChainSupport.formats);
	VkPresentModeKHR   presentMode   = chooseSwapPresentMode(swapChainSupport.presentModes);
	VkExtent2D         extent        = chooseSwapExtent(swapChainSupport.capabilities);

	uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;
	if (swapChainSupport.capabilities.maxImageCount > 0 && imageCount > swapChainSupport.capabilities.maxImageCount)
	{
		imageCount = swapChainSupport.capabilities.maxImageCount;
	}

	VkSwapchainCreateInfoKHR createInfo = {};
	createInfo.sType            = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	createInfo.surface          = surface;
	createInfo.minImageCount    = imageCount;
	createInfo.imageFormat      = surfaceFormat.format;
	createInfo.imageColorSpace  = surfaceFormat.colorSpace;
	createInfo.imageExtent      = extent;
	createInfo.imageArrayLayers = 1;
	createInfo.imageUsage       = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

	QueueFamilyIndices indices = device.getQueueFamilyIndices(surface);
	uint32_t queueFamilyIndices[] = { (uint32_t)indices.graphicsFamily, (uint32_t)indices.presentFamily };

	if (indices.graphicsFamily != indices.presentFamily)
	{
		createInfo.imageSharingMode      = VK_SHARING_MODE_CONCURRENT;
		createInfo.queueFamilyIndexCount = 2;
		createInfo.pQueueFamilyIndices   = queueFamilyIndices;
	}
	else
	{
		createInfo.imageSharingMode      = VK_SHARING_MODE_EXCLUSIVE;
		createInfo.queueFamilyIndexCount = 0;       // Optional
		createInfo.pQueueFamilyIndices   = nullptr; // Optional
	}

	createInfo.preTransform   = swapChainSupport.capabilities.currentTransform;
	createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	createInfo.presentMode    = presentMode;
	createInfo.clipped        = VK_TRUE;
	createInfo.oldSwapchain   = VK_NULL_HANDLE;

	if (vkCreateSwapchainKHR(device.logical, &createInfo, nullptr, &swapChain) != VK_SUCCESS)
	{
		throw std::runtime_error("vkCraft: Failed to create swap chain");
	}

	vkGetSwapchainImagesKHR(device.logical, swapChain, &imageCount, nullptr);
	swapChainImages.resize(imageCount);
	vkGetSwapchainImagesKHR(device.logical, swapChain, &imageCount, swapChainImages.data());

	swapChainImageFormat = surfaceFormat.format;
	swapChainExtent      = extent;
}

void VkCraft::createImageViews()
{
	swapChainImageViews.resize(swapChainImages.size());

	for (int i = 0; i < (int)swapChainImages.size(); i++)
	{
		swapChainImageViews[i] = createImageView(swapChainImages[i], swapChainImageFormat, VK_IMAGE_ASPECT_COLOR_BIT);
	}
}

void VkCraft::createRenderPass()
{
	VkAttachmentDescription colorAttachment = {};
	colorAttachment.format         = swapChainImageFormat;
	colorAttachment.samples        = VK_SAMPLE_COUNT_1_BIT;
	colorAttachment.loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
	colorAttachment.storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
	colorAttachment.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	colorAttachment.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
	colorAttachment.finalLayout    = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

	VkAttachmentReference colorAttachmentRef = {};
	colorAttachmentRef.attachment = 0;
	colorAttachmentRef.layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkAttachmentDescription depthAttachment = {};
	depthAttachment.format         = findDepthFormat();
	depthAttachment.samples        = VK_SAMPLE_COUNT_1_BIT;
	depthAttachment.loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
	depthAttachment.storeOp        = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	depthAttachment.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	depthAttachment.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
	depthAttachment.finalLayout    = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	VkAttachmentReference depthAttachmentRef = {};
	depthAttachmentRef.attachment = 1;
	depthAttachmentRef.layout     = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	VkSubpassDescription subpass = {};
	subpass.pipelineBindPoint       = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.colorAttachmentCount    = 1;
	subpass.pColorAttachments       = &colorAttachmentRef;
	subpass.pDepthStencilAttachment = &depthAttachmentRef;

	VkSubpassDependency dependency = {};
	dependency.srcSubpass    = VK_SUBPASS_EXTERNAL;
	dependency.dstSubpass    = 0;
	dependency.srcStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependency.srcAccessMask = 0;
	dependency.dstStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

	std::array<VkAttachmentDescription, 2> attachments = { colorAttachment, depthAttachment };

	VkRenderPassCreateInfo renderPassInfo = {};
	renderPassInfo.sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
	renderPassInfo.pAttachments    = attachments.data();
	renderPassInfo.subpassCount    = 1;
	renderPassInfo.pSubpasses      = &subpass;
	renderPassInfo.dependencyCount = 1;
	renderPassInfo.pDependencies   = &dependency;

	if (vkCreateRenderPass(device.logical, &renderPassInfo, nullptr, &renderPass) != VK_SUCCESS)
	{
		throw std::runtime_error("vkCraft: Failed to create render pass");
	}
}

void VkCraft::createDescriptorSetLayout()
{
	VkDescriptorSetLayoutBinding uboLayoutBinding = {};
	uboLayoutBinding.binding            = 0;
	uboLayoutBinding.descriptorType     = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	uboLayoutBinding.descriptorCount    = 1;
	uboLayoutBinding.pImmutableSamplers = nullptr;
	uboLayoutBinding.stageFlags         = VK_SHADER_STAGE_VERTEX_BIT;

	VkDescriptorSetLayoutBinding samplerLayoutBinding = {};
	samplerLayoutBinding.binding            = 1;
	samplerLayoutBinding.descriptorCount    = 1;
	samplerLayoutBinding.descriptorType     = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	samplerLayoutBinding.pImmutableSamplers = nullptr;
	samplerLayoutBinding.stageFlags         = VK_SHADER_STAGE_FRAGMENT_BIT;

	std::array<VkDescriptorSetLayoutBinding, 2> bindings = { uboLayoutBinding, samplerLayoutBinding };

	VkDescriptorSetLayoutCreateInfo layoutInfo = {};
	layoutInfo.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
	layoutInfo.pBindings    = bindings.data();

	if (vkCreateDescriptorSetLayout(device.logical, &layoutInfo, nullptr, &descriptorSetLayout) != VK_SUCCESS)
	{
		throw std::runtime_error("vkCraft: Failed to create descriptor set layout!");
	}
}

void VkCraft::createGraphicsPipeline()
{
	std::vector<char> vertShaderCode = FileUtils::readFile("shaders/vert.spv");
	std::vector<char> fragShaderCode = FileUtils::readFile("shaders/frag.spv");

	VkShaderModule vertShaderModule = createShaderModule(vertShaderCode);
	VkShaderModule fragShaderModule = createShaderModule(fragShaderCode);

	VkPipelineShaderStageCreateInfo vertShaderStageInfo = {};
	vertShaderStageInfo.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	vertShaderStageInfo.stage  = VK_SHADER_STAGE_VERTEX_BIT;
	vertShaderStageInfo.module = vertShaderModule;
	vertShaderStageInfo.pName  = "main";

	VkPipelineShaderStageCreateInfo fragShaderStageInfo = {};
	fragShaderStageInfo.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	fragShaderStageInfo.stage  = VK_SHADER_STAGE_FRAGMENT_BIT;
	fragShaderStageInfo.module = fragShaderModule;
	fragShaderStageInfo.pName  = "main";

	VkPipelineShaderStageCreateInfo shaderStages[] = { vertShaderStageInfo, fragShaderStageInfo };

	VkVertexInputBindingDescription bindingDescription = Vertex::getBindingDescription();
	#ifdef _WIN32
		std::array<VkVertexInputAttributeDescription, 3Ui64> attributeDescriptions = Vertex::getAttributeDescriptions();
	#elif __unix__
		std::array<VkVertexInputAttributeDescription, 3ULL> attributeDescriptions = Vertex::getAttributeDescriptions();
	#endif

	//Vertex data format
	VkPipelineVertexInputStateCreateInfo vertexInputInfo = {};
	vertexInputInfo.sType                           = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertexInputInfo.vertexBindingDescriptionCount   = 1;
	vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
	vertexInputInfo.pVertexBindingDescriptions      = &bindingDescription;
	vertexInputInfo.pVertexAttributeDescriptions    = attributeDescriptions.data();

	//How to assemble vertex data
	VkPipelineInputAssemblyStateCreateInfo inputAssembly = {};
	inputAssembly.sType                  = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	inputAssembly.topology               = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	inputAssembly.primitiveRestartEnable = VK_FALSE;

	//Viewport
	VkViewport viewport = {};
	viewport.x        = 0.0f;
	viewport.y        = 0.0f;
	viewport.width    = (float)swapChainExtent.width;
	viewport.height   = (float)swapChainExtent.height;
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;

	//Scissor
	VkRect2D scissor = {};
	scissor.offset = { 0, 0 };
	scissor.extent = swapChainExtent;

	//Combines multiple viewports and scissors
	VkPipelineViewportStateCreateInfo viewportState = {};
	viewportState.sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewportState.viewportCount = 1;
	viewportState.pViewports    = &viewport;
	viewportState.scissorCount  = 1;
	viewportState.pScissors     = &scissor;

	//Rasterizer configuration
	VkPipelineRasterizationStateCreateInfo rasterizer = {};
	rasterizer.sType                   = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterizer.depthClampEnable        = VK_FALSE;
	rasterizer.rasterizerDiscardEnable = VK_FALSE;
	rasterizer.polygonMode             = VK_POLYGON_MODE_FILL;
	rasterizer.lineWidth               = 1.0f;
	rasterizer.cullMode                = VK_CULL_MODE_BACK_BIT;
	rasterizer.frontFace               = VK_FRONT_FACE_COUNTER_CLOCKWISE;
	rasterizer.depthBiasEnable         = VK_FALSE;

	//Antialiasing (multisampling)
	VkPipelineMultisampleStateCreateInfo multisampling = {};
	multisampling.sType                = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisampling.sampleShadingEnable  = VK_FALSE;
	multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

	VkPipelineColorBlendAttachmentState colorBlendAttachment = {};
	colorBlendAttachment.colorWriteMask      = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	colorBlendAttachment.blendEnable         = VK_FALSE;
	colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
	colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
	colorBlendAttachment.colorBlendOp        = VK_BLEND_OP_ADD;
	colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
	colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
	colorBlendAttachment.alphaBlendOp        = VK_BLEND_OP_ADD;

	VkPipelineColorBlendStateCreateInfo colorBlending = {};
	colorBlending.sType             = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	colorBlending.logicOpEnable     = VK_FALSE;
	colorBlending.logicOp           = VK_LOGIC_OP_COPY;
	colorBlending.attachmentCount   = 1;
	colorBlending.pAttachments      = &colorBlendAttachment;
	colorBlending.blendConstants[0] = 0.0f;
	colorBlending.blendConstants[1] = 0.0f;
	colorBlending.blendConstants[2] = 0.0f;
	colorBlending.blendConstants[3] = 0.0f;

	VkPipelineLayoutCreateInfo pipelineLayoutInfo = {};
	pipelineLayoutInfo.sType          = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutInfo.setLayoutCount = 1;
	pipelineLayoutInfo.pSetLayouts    = &descriptorSetLayout;

	if (vkCreatePipelineLayout(device.logical, &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS)
	{
		throw std::runtime_error("vkCraft: Failed to create pipeline layout");
	}

	VkPipelineDepthStencilStateCreateInfo depthStencil = {};
	depthStencil.sType                 = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	depthStencil.depthTestEnable       = VK_TRUE;
	depthStencil.depthWriteEnable      = VK_TRUE;
	depthStencil.depthCompareOp        = VK_COMPARE_OP_LESS;
	depthStencil.depthBoundsTestEnable = VK_FALSE;
	depthStencil.stencilTestEnable     = VK_FALSE;

	VkGraphicsPipelineCreateInfo pipelineInfo = {};
	pipelineInfo.sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pipelineInfo.stageCount          = 2;
	pipelineInfo.pStages             = shaderStages;
	pipelineInfo.pVertexInputState   = &vertexInputInfo;
	pipelineInfo.pInputAssemblyState = &inputAssembly;
	pipelineInfo.pViewportState      = &viewportState;
	pipelineInfo.pRasterizationState = &rasterizer;
	pipelineInfo.pMultisampleState   = &multisampling;
	pipelineInfo.pColorBlendState    = &colorBlending;
	pipelineInfo.layout              = pipelineLayout;
	pipelineInfo.renderPass          = renderPass;
	pipelineInfo.subpass             = 0;
	pipelineInfo.basePipelineHandle  = VK_NULL_HANDLE;
	pipelineInfo.pDepthStencilState  = &depthStencil;

	if (vkCreateGraphicsPipelines(device.logical, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &graphicsPipeline) != VK_SUCCESS)
	{
		throw std::runtime_error("vkCraft: Failed to create graphics pipeline");
	}

	vkDestroyShaderModule(device.logical, fragShaderModule, nullptr);
	vkDestroyShaderModule(device.logical, vertShaderModule, nullptr);
}

void VkCraft::createFramebuffers()
{
	swapChainFramebuffers.resize(swapChainImageViews.size());

	for (int i = 0; i < (int)swapChainImageViews.size(); i++)
	{
		std::array<VkImageView, 2> attachments = { swapChainImageViews[i], depth.imageView };

		VkFramebufferCreateInfo framebufferInfo = {};
		framebufferInfo.sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		framebufferInfo.renderPass      = renderPass;
		framebufferInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
		framebufferInfo.pAttachments    = attachments.data();
		framebufferInfo.width           = swapChainExtent.width;
		framebufferInfo.height          = swapChainExtent.height;
		framebufferInfo.layers          = 1;

		if (vkCreateFramebuffer(device.logical, &framebufferInfo, nullptr, &swapChainFramebuffers[i]) != VK_SUCCESS)
		{
			throw std::runtime_error("vkCraft: Failed to create framebuffer");
		}
	}
}

void VkCraft::createCommandPool(VkCommandPool *commandPool)
{
	QueueFamilyIndices queueFamilyIndices = device.getQueueFamilyIndices(surface);

	VkCommandPoolCreateInfo poolInfo = {};
	poolInfo.sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	poolInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily;

	if (vkCreateCommandPool(device.logical, &poolInfo, nullptr, commandPool) != VK_SUCCESS)
	{
		throw std::runtime_error("vkCraft: Failed to create command pool");
	}
}

void VkCraft::createGeometryBuffers(Geometry *geometry)
{
	if (geometry->hasBuffers() || geometry->indices.size() == 0 || geometry->vertices.size() == 0)
	{
		return;
	}

	VkDeviceSize vertexBufferSize = sizeof(Vertex) * geometry->vertices.size();
	VkDeviceSize indexBufferSize  = sizeof(uint32_t) * geometry->indices.size();

	VkBuffer       indexStagingBuffer,       vertexStagingBuffer;
	VkDeviceMemory indexStagingBufferMemory, vertexStagingBufferMemory;

	//Create CPU staging buffers
	BufferUtils::createBuffer(device, vertexBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, vertexStagingBuffer, vertexStagingBufferMemory);
	BufferUtils::createBuffer(device, indexBufferSize,  VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, indexStagingBuffer,  indexStagingBufferMemory);

	//Map memory
	void *vertexData, *indexData;
	vkMapMemory(device.logical, vertexStagingBufferMemory, 0, vertexBufferSize, 0, &vertexData);
	vkMapMemory(device.logical, indexStagingBufferMemory,  0, indexBufferSize,  0, &indexData);

	memcpy(vertexData, geometry->vertices.data(), (size_t)vertexBufferSize);
	memcpy(indexData,  geometry->indices.data(),  (size_t)indexBufferSize);

	vkUnmapMemory(device.logical, vertexStagingBufferMemory);
	vkUnmapMemory(device.logical, indexStagingBufferMemory);

	//Create GPU buffers
	BufferUtils::createBuffer(device, vertexBufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, geometry->vertexBuffer, geometry->vertexBufferMemory);
	BufferUtils::createBuffer(device, indexBufferSize,  VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,  VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, geometry->indexBuffer,  geometry->indexBufferMemory);

	//Copy from CPU staging buffer to GPU buffer
	BufferUtils::copyBuffer(&device, &graphicsQueue, &commandPool, vertexStagingBuffer, geometry->vertexBuffer, vertexBufferSize);
	BufferUtils::copyBuffer(&device, &graphicsQueue, &commandPool, indexStagingBuffer,  geometry->indexBuffer,  indexBufferSize);

	//Clean the staging (CPU) buffers
	vkDestroyBuffer(device.logical, vertexStagingBuffer, nullptr);
	vkFreeMemory(device.logical, vertexStagingBufferMemory, nullptr);
	vkDestroyBuffer(device.logical, indexStagingBuffer, nullptr);
	vkFreeMemory(device.logical, indexStagingBufferMemory, nullptr);
}

void VkCraft::createDescriptorPool()
{
	std::array<VkDescriptorPoolSize, 2> poolSizes = {};

	poolSizes[0].type            = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	poolSizes[0].descriptorCount = 1;
	poolSizes[1].type            = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	poolSizes[1].descriptorCount = 1;

	VkDescriptorPoolCreateInfo poolInfo = {};
	poolInfo.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
	poolInfo.pPoolSizes    = poolSizes.data();
	poolInfo.maxSets       = 1;

	if (vkCreateDescriptorPool(device.logical, &poolInfo, nullptr, &descriptorPool) != VK_SUCCESS)
	{
		throw std::runtime_error("vkCraft: Failed to create descriptor pool!");
	}
}

void VkCraft::createDescriptorSet()
{
	VkDescriptorSetLayout layouts[] = { descriptorSetLayout };
	VkDescriptorSetAllocateInfo allocInfo = {};
	allocInfo.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocInfo.descriptorPool     = descriptorPool;
	allocInfo.descriptorSetCount = 1;
	allocInfo.pSetLayouts        = layouts;

	if (vkAllocateDescriptorSets(device.logical, &allocInfo, &descriptorSet) != VK_SUCCESS)
	{
		throw std::runtime_error("vkCraft: Failed to allocate descriptor set!");
	}

	VkDescriptorBufferInfo bufferInfo = {};
	bufferInfo.buffer = uniformBuffer;
	bufferInfo.offset = 0;
	bufferInfo.range  = sizeof(UniformBufferObject);

	VkDescriptorImageInfo imageInfo = {};
	imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	imageInfo.imageView   = texture.imageView;
	imageInfo.sampler     = textureSampler;

	std::array<VkWriteDescriptorSet, 2> descriptorWrites = {};

	descriptorWrites[0].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	descriptorWrites[0].dstSet          = descriptorSet;
	descriptorWrites[0].dstBinding      = 0;
	descriptorWrites[0].dstArrayElement = 0;
	descriptorWrites[0].descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	descriptorWrites[0].descriptorCount = 1;
	descriptorWrites[0].pBufferInfo     = &bufferInfo;

	descriptorWrites[1].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	descriptorWrites[1].dstSet          = descriptorSet;
	descriptorWrites[1].dstBinding      = 1;
	descriptorWrites[1].dstArrayElement = 0;
	descriptorWrites[1].descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	descriptorWrites[1].descriptorCount = 1;
	descriptorWrites[1].pImageInfo      = &imageInfo;

	vkUpdateDescriptorSets(device.logical, static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
}

void VkCraft::transitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout)
{
	VkCommandBuffer commandBuffer = beginSingleTimeCommands();

	VkImageMemoryBarrier barrier = {};
	barrier.sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barrier.oldLayout           = oldLayout;
	barrier.newLayout           = newLayout;
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.image               = image;
	barrier.subresourceRange.baseMipLevel   = 0;
	barrier.subresourceRange.levelCount     = 1;
	barrier.subresourceRange.baseArrayLayer = 0;
	barrier.subresourceRange.layerCount     = 1;

	if (newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
	{
		barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;

		if (hasStencilComponent(format))
		{
			barrier.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
		}
	}
	else
	{
		barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	}

	VkPipelineStageFlags sourceStage;
	VkPipelineStageFlags destinationStage;

	if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
	{
		barrier.srcAccessMask = 0;
		barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		sourceStage           = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
		destinationStage      = VK_PIPELINE_STAGE_TRANSFER_BIT;
	}
	else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
	{
		barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
		sourceStage           = VK_PIPELINE_STAGE_TRANSFER_BIT;
		destinationStage      = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
	}
	else if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
	{
		barrier.srcAccessMask = 0;
		barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
		sourceStage           = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
		destinationStage      = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
	}
	else
	{
		throw std::invalid_argument("vkCraft: Unsupported layout transition!");
	}

	vkCmdPipelineBarrier(commandBuffer, sourceStage, destinationStage, 0, 0, nullptr, 0, nullptr, 1, &barrier);

	endSingleTimeCommands(commandBuffer);
}

void VkCraft::copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height)
{
	VkCommandBuffer commandBuffer = beginSingleTimeCommands();

	VkBufferImageCopy region = {};
	region.bufferOffset                    = 0;
	region.bufferRowLength                 = 0;
	region.bufferImageHeight               = 0;
	region.imageSubresource.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
	region.imageSubresource.mipLevel       = 0;
	region.imageSubresource.baseArrayLayer = 0;
	region.imageSubresource.layerCount     = 1;
	region.imageOffset                     = { 0, 0, 0 };
	region.imageExtent                     = { width, height, 1 };

	vkCmdCopyBufferToImage(commandBuffer, buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

	endSingleTimeCommands(commandBuffer);
}

void VkCraft::createTextureImage(const char *fname)
{
	int texWidth, texHeight, texChannels;

	//Load image
	stbi_uc* pixels = stbi_load(fname, &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
	VkDeviceSize imageSize = texWidth * texHeight * 4;

	if (!pixels)
	{
		throw std::runtime_error("vkCraft: Failed to load texture image!");
	}

	//Create staging buffer to transfer to GPU memory
	VkBuffer stagingBuffer;
	VkDeviceMemory stagingBufferMemory;
	BufferUtils::createBuffer(device, imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);

	//Copy data directly to GPU memory
	void* data;
	vkMapMemory(device.logical, stagingBufferMemory, 0, imageSize, 0, &data);
	memcpy(data, pixels, static_cast<size_t>(imageSize));

	//Clear staging buffer and image data
	vkUnmapMemory(device.logical, stagingBufferMemory);
	stbi_image_free(pixels);

	//Create vulkan image
	createImage(texWidth, texHeight, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, texture.image, texture.imageMemory);

	//Transition the texture image to VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
	transitionImageLayout(texture.image, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

	//Copy the staging buffer to the texture image
	copyBufferToImage(stagingBuffer, texture.image, static_cast<uint32_t>(texWidth), static_cast<uint32_t>(texHeight));

	//Prepare texture for shader access
	transitionImageLayout(texture.image, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

	//Clean staging buffers
	vkDestroyBuffer(device.logical, stagingBuffer, nullptr);
	vkFreeMemory(device.logical, stagingBufferMemory, nullptr);

	//Create texture view
	texture.imageView = createImageView(texture.image, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_ASPECT_COLOR_BIT);
}
