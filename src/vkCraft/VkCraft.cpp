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

	glm::ivec3 index = world.getIndex(camera.position);

	//Create geometries buffers
	if (index != cameraIndex)
	{
		double t = glfwGetTime();

		int distance = RENDER_DISTANCE;
		std::vector<Geometry*> geometries = world.getGeometries(camera.position, distance);

		for (size_t i = 0; i < geometries.size(); i++)
		{
			createGeometryBuffers(geometries[i]);
		}
		
		std::cout << "VkCraft: Got " << geometries.size() << " geometries." << std::endl;

		recreateRenderingCommandBuffers();
		cameraIndex = index;

		std::cout << "VkCraft: Get world geometries took " << (glfwGetTime() - t) << "s." << std::endl;
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
	VkSemaphore signalSemaphores[] = { renderFinishedSemaphores[currentFrame] };

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

	//Command buffers
	createRenderingCommandBuffers();

	//Semaphores
	createSyncObjects();
}

void VkCraft::recreateSwapChain()
{
	// Wait while window is minimized
	int width = 0, height = 0;
	while (width == 0 || height == 0)
	{
		glfwGetFramebufferSize(window, &width, &height);
		glfwWaitEvents();
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
	for (size_t i = 0; i < static_cast<size_t>(CONCURRENT_FRAMES); i++)
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
		DestroyDebugReportCallbackEXT(instance, callback, nullptr);
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
	appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	appInfo.pApplicationName = "VkCraft";
	appInfo.pEngineName = "No Engine";
	appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
	appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
	appInfo.apiVersion = VK_API_VERSION_1_0;

	VkInstanceCreateInfo createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	createInfo.pApplicationInfo = &appInfo;

	std::vector<const char*> extensions = getRequiredExtensions();
	// macOS/MoltenVK: Vulkan SDK >= 1.3.216 requires portability enumeration
	extensions.push_back(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
	extensions.push_back("VK_KHR_get_physical_device_properties2");
	createInfo.flags = VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
	createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
	createInfo.ppEnabledExtensionNames = extensions.data();

	//Check extensions available
	fprintf(stdout, "GLFW Extensions:\n");
	for (size_t i = 0; i < extensions.size(); i++)
	{
		fprintf(stdout, "\t%s\n", extensions[i]);
	}

	if (ENABLE_VALIDATION_LAYERS)
	{
		createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
		createInfo.ppEnabledLayerNames = validationLayers.data();
	}
	else
	{
		createInfo.enabledLayerCount = 0;
	}

	if (vkCreateInstance(&createInfo, nullptr, &instance) != VK_SUCCESS)
	{
		throw std::runtime_error("vkCraft: Failed to create instance!");
	}

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

void VkCraft::setupDebugCallback()
{
	if (!ENABLE_VALIDATION_LAYERS)
	{
		return;
	}

	VkDebugReportCallbackCreateInfoEXT createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT;
	createInfo.flags = VK_DEBUG_REPORT_ERROR_BIT_EXT | VK_DEBUG_REPORT_WARNING_BIT_EXT;
	createInfo.pfnCallback = debugCallback;

	if (CreateDebugReportCallbackEXT(instance, &createInfo, nullptr, &callback) != VK_SUCCESS)
	{
		throw std::runtime_error("vkCraft: Failed to set up debug callback");
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
		if (vpdp.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) fprintf(stdout, " (Discrete GPU)\n");
		if (vpdp.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU) fprintf(stdout, " (Integrated GPU)\n");
		if (vpdp.deviceType == VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU) fprintf(stdout, " (Virtual GPU)\n");
		if (vpdp.deviceType == VK_PHYSICAL_DEVICE_TYPE_CPU) fprintf(stdout, " (CPU)\n");
		fprintf(stdout, "\tPipeline Cache Size: %d\n", vpdp.pipelineCacheUUID[0]);
		fprintf(stdout, "\tAPI version: %d\n", vpdp.apiVersion);
		fprintf(stdout, "\tDriver version: %d\n", vpdp.driverVersion);
		fprintf(stdout, "\tVendor ID: 0x%04x\n", vpdp.vendorID);
		fprintf(stdout, "\tDevice ID: 0x%04x\n", vpdp.deviceID);

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
	// FIX: use uint32_t to match Vulkan queue family index type (avoids narrowing from uint32_t to int)
	std::set<uint32_t> uniqueQueueFamilies = {
		indices.graphicsFamily,
		indices.presentFamily
	};

	float queuePriority = 1.0f;
	for (uint32_t queueFamily : uniqueQueueFamilies)
	{
		VkDeviceQueueCreateInfo queueCreateInfo = {};
		queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		queueCreateInfo.queueFamilyIndex = queueFamily;
		queueCreateInfo.queueCount = 1;
		queueCreateInfo.pQueuePriorities = &queuePriority;
		queueCreateInfos.push_back(queueCreateInfo);
	}

	VkPhysicalDeviceFeatures deviceFeatures = {};
	deviceFeatures.samplerAnisotropy = VK_TRUE;

	VkDeviceCreateInfo createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	createInfo.pQueueCreateInfos = queueCreateInfos.data();
	createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
	createInfo.pEnabledFeatures = &deviceFeatures;
	// macOS/MoltenVK requires the portability subset extension
	std::vector<const char*> activeDeviceExtensions(deviceExtensions.begin(), deviceExtensions.end());
	activeDeviceExtensions.push_back("VK_KHR_portability_subset");
	createInfo.enabledExtensionCount = static_cast<uint32_t>(activeDeviceExtensions.size());
	createInfo.ppEnabledExtensionNames = activeDeviceExtensions.data();

	if (ENABLE_VALIDATION_LAYERS)
	{
		createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
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
	vkGetDeviceQueue(device.logical, indices.presentFamily, 0, &presentQueue);
}

void VkCraft::createSwapChain()
{
	SwapChainSupportDetails swapChainSupport = querySwapChainSupport(device.physical);

	VkSurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(swapChainSupport.formats);
	VkPresentModeKHR presentMode = chooseSwapPresentMode(swapChainSupport.presentModes);
	VkExtent2D extent = chooseSwapExtent(swapChainSupport.capabilities);

	uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;
	if (swapChainSupport.capabilities.maxImageCount > 0 && imageCount > swapChainSupport.capabilities.maxImageCount)
	{
		imageCount = swapChainSupport.capabilities.maxImageCount;
	}

	VkSwapchainCreateInfoKHR createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	createInfo.surface = surface;
	createInfo.minImageCount = imageCount;
	createInfo.imageFormat = surfaceFormat.format;
	createInfo.imageColorSpace = surfaceFormat.colorSpace;
	createInfo.imageExtent = extent;
	createInfo.imageArrayLayers = 1;
	createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

	QueueFamilyIndices indices = device.getQueueFamilyIndices(surface);
	// FIX: use uint32_t array directly — no narrowing cast needed
	uint32_t queueFamilyIndices[] = { indices.graphicsFamily, indices.presentFamily };

	if (indices.graphicsFamily != indices.presentFamily)
	{
		createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
		createInfo.queueFamilyIndexCount = 2;
		createInfo.pQueueFamilyIndices = queueFamilyIndices;
	}
	else
	{
		createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
		createInfo.queueFamilyIndexCount = 0; // Optional
		createInfo.pQueueFamilyIndices = nullptr; // Optional
	}

	createInfo.preTransform = swapChainSupport.capabilities.currentTransform;
	createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	createInfo.presentMode = presentMode;
	createInfo.clipped = VK_TRUE;
	createInfo.oldSwapchain = VK_NULL_HANDLE;

	if (vkCreateSwapchainKHR(device.logical, &createInfo, nullptr, &swapChain) != VK_SUCCESS)
	{
		throw std::runtime_error("vkCraft: Failed to create swap chain");
	}

	vkGetSwapchainImagesKHR(device.logical, swapChain, &imageCount, nullptr);
	swapChainImages.resize(imageCount);
	vkGetSwapchainImagesKHR(device.logical, swapChain, &imageCount, swapChainImages.data());

	swapChainImageFormat = surfaceFormat.format;
	swapChainExtent = extent;
}

void VkCraft::createImageViews()
{
	swapChainImageViews.resize(swapChainImages.size());

	for (size_t i = 0; i < swapChainImages.size(); i++)
	{
		swapChainImageViews[i] = createImageView(swapChainImages[i], swapChainImageFormat, VK_IMAGE_ASPECT_COLOR_BIT);
	}
}

void VkCraft::createRenderPass()
{
	VkAttachmentDescription colorAttachment = {};
	colorAttachment.format = swapChainImageFormat;
	colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
	colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

	VkAttachmentReference colorAttachmentRef = {};
	colorAttachmentRef.attachment = 0;
	colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkAttachmentDescription depthAttachment = {};
	depthAttachment.format = findDepthFormat();
	depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
	depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	VkAttachmentReference depthAttachmentRef = {};
	depthAttachmentRef.attachment = 1;
	depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	VkSubpassDescription subpass = {};
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.colorAttachmentCount = 1;
	subpass.pColorAttachments = &colorAttachmentRef;
	subpass.pDepthStencilAttachment = &depthAttachmentRef;

	VkSubpassDependency dependency = {};
	dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
	dependency.dstSubpass = 0;
	dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependency.srcAccessMask = 0;
	dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

	std::array<VkAttachmentDescription, 2> attachments = { colorAttachment, depthAttachment };
	VkRenderPassCreateInfo renderPassInfo = {};
	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
	renderPassInfo.pAttachments = attachments.data();
	renderPassInfo.subpassCount = 1;
	renderPassInfo.pSubpasses = &subpass;
	renderPassInfo.dependencyCount = 1;
	renderPassInfo.pDependencies = &dependency;

	if (vkCreateRenderPass(device.logical, &renderPassInfo, nullptr, &renderPass) != VK_SUCCESS)
	{
		throw std::runtime_error("vkCraft: Failed to create render pass!");
	}
}

void VkCraft::createDescriptorSetLayout()
{
	VkDescriptorSetLayoutBinding uboLayoutBinding = {};
	uboLayoutBinding.binding = 0;
	uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	uboLayoutBinding.descriptorCount = 1;
	uboLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
	uboLayoutBinding.pImmutableSamplers = nullptr;

	VkDescriptorSetLayoutBinding samplerLayoutBinding = {};
	samplerLayoutBinding.binding = 1;
	samplerLayoutBinding.descriptorCount = 1;
	samplerLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	samplerLayoutBinding.pImmutableSamplers = nullptr;
	samplerLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

	std::array<VkDescriptorSetLayoutBinding, 2> bindings = { uboLayoutBinding, samplerLayoutBinding };
	VkDescriptorSetLayoutCreateInfo layoutInfo = {};
	layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
	layoutInfo.pBindings = bindings.data();

	if (vkCreateDescriptorSetLayout(device.logical, &layoutInfo, nullptr, &descriptorSetLayout) != VK_SUCCESS)
	{
		throw std::runtime_error("vkCraft: Failed to create descriptor set layout!");
	}
}

void VkCraft::createGraphicsPipeline()
{
	std::vector<char> vertShaderCode = FileUtils::readFile("shader/vert.spv");
	std::vector<char> fragShaderCode = FileUtils::readFile("shader/frag.spv");

	VkShaderModule vertShaderModule = createShaderModule(vertShaderCode);
	VkShaderModule fragShaderModule = createShaderModule(fragShaderCode);

	VkPipelineShaderStageCreateInfo vertShaderStageInfo = {};
	vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
	vertShaderStageInfo.module = vertShaderModule;
	vertShaderStageInfo.pName = "main";

	VkPipelineShaderStageCreateInfo fragShaderStageInfo = {};
	fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	fragShaderStageInfo.module = fragShaderModule;
	fragShaderStageInfo.pName = "main";

	VkPipelineShaderStageCreateInfo shaderStages[] = { vertShaderStageInfo, fragShaderStageInfo };

	auto bindingDescription = Vertex::getBindingDescription();
	auto attributeDescriptions = Vertex::getAttributeDescriptions();

	VkPipelineVertexInputStateCreateInfo vertexInputInfo = {};
	vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertexInputInfo.vertexBindingDescriptionCount = 1;
	vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
	vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
	vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();

	VkPipelineInputAssemblyStateCreateInfo inputAssembly = {};
	inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	inputAssembly.primitiveRestartEnable = VK_FALSE;

	VkViewport viewport = {};
	viewport.x = 0.0f;
	viewport.y = 0.0f;
	viewport.width = (float)swapChainExtent.width;
	viewport.height = (float)swapChainExtent.height;
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;

	VkRect2D scissor = {};
	scissor.offset = { 0, 0 };
	scissor.extent = swapChainExtent;

	VkPipelineViewportStateCreateInfo viewportState = {};
	viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewportState.viewportCount = 1;
	viewportState.pViewports = &viewport;
	viewportState.scissorCount = 1;
	viewportState.pScissors = &scissor;

	VkPipelineRasterizationStateCreateInfo rasterizer = {};
	rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterizer.depthClampEnable = VK_FALSE;
	rasterizer.rasterizerDiscardEnable = VK_FALSE;
	rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
	rasterizer.lineWidth = 1.0f;
	rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
	rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
	rasterizer.depthBiasEnable = VK_FALSE;

	VkPipelineMultisampleStateCreateInfo multisampling = {};
	multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisampling.sampleShadingEnable = VK_FALSE;
	multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

	VkPipelineDepthStencilStateCreateInfo depthStencil = {};
	depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	depthStencil.depthTestEnable = VK_TRUE;
	depthStencil.depthWriteEnable = VK_TRUE;
	depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;
	depthStencil.depthBoundsTestEnable = VK_FALSE;
	depthStencil.stencilTestEnable = VK_FALSE;

	VkPipelineColorBlendAttachmentState colorBlendAttachment = {};
	colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	colorBlendAttachment.blendEnable = VK_FALSE;

	VkPipelineColorBlendStateCreateInfo colorBlending = {};
	colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	colorBlending.logicOpEnable = VK_FALSE;
	colorBlending.logicOp = VK_LOGIC_OP_COPY;
	colorBlending.attachmentCount = 1;
	colorBlending.pAttachments = &colorBlendAttachment;
	colorBlending.blendConstants[0] = 0.0f;
	colorBlending.blendConstants[1] = 0.0f;
	colorBlending.blendConstants[2] = 0.0f;
	colorBlending.blendConstants[3] = 0.0f;

	VkPipelineLayoutCreateInfo pipelineLayoutInfo = {};
	pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutInfo.setLayoutCount = 1;
	pipelineLayoutInfo.pSetLayouts = &descriptorSetLayout;

	if (vkCreatePipelineLayout(device.logical, &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS)
	{
		throw std::runtime_error("vkCraft: Failed to create pipeline layout!");
	}

	VkGraphicsPipelineCreateInfo pipelineInfo = {};
	pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pipelineInfo.stageCount = 2;
	pipelineInfo.pStages = shaderStages;
	pipelineInfo.pVertexInputState = &vertexInputInfo;
	pipelineInfo.pInputAssemblyState = &inputAssembly;
	pipelineInfo.pViewportState = &viewportState;
	pipelineInfo.pRasterizationState = &rasterizer;
	pipelineInfo.pMultisampleState = &multisampling;
	pipelineInfo.pDepthStencilState = &depthStencil;
	pipelineInfo.pColorBlendState = &colorBlending;
	pipelineInfo.layout = pipelineLayout;
	pipelineInfo.renderPass = renderPass;
	pipelineInfo.subpass = 0;
	pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;

	if (vkCreateGraphicsPipelines(device.logical, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &graphicsPipeline) != VK_SUCCESS)
	{
		throw std::runtime_error("vkCraft: Failed to create graphics pipeline!");
	}

	vkDestroyShaderModule(device.logical, fragShaderModule, nullptr);
	vkDestroyShaderModule(device.logical, vertShaderModule, nullptr);
}

void VkCraft::createFramebuffers()
{
	swapChainFramebuffers.resize(swapChainImageViews.size());

	for (size_t i = 0; i < swapChainImageViews.size(); i++)
	{
		std::array<VkImageView, 2> attachments = { swapChainImageViews[i], depth.imageView };

		VkFramebufferCreateInfo framebufferInfo = {};
		framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		framebufferInfo.renderPass = renderPass;
		framebufferInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
		framebufferInfo.pAttachments = attachments.data();
		framebufferInfo.width = swapChainExtent.width;
		framebufferInfo.height = swapChainExtent.height;
		framebufferInfo.layers = 1;

		if (vkCreateFramebuffer(device.logical, &framebufferInfo, nullptr, &swapChainFramebuffers[i]) != VK_SUCCESS)
		{
			throw std::runtime_error("vkCraft: Failed to create framebuffer!");
		}
	}
}

void VkCraft::createGeometryBuffers(Geometry *geometry)
{
	if (geometry->hasBuffers() || geometry->indices.size() == 0 || geometry->vertices.size() == 0)
	{
		return;
	}

	VkDeviceSize vertexBufferSize = sizeof(Vertex) * geometry->vertices.size();
	VkDeviceSize indexBufferSize = sizeof(uint32_t) * geometry->indices.size();

	VkBuffer indexStagingBuffer, vertexStagingBuffer;
	VkDeviceMemory indexStagingBufferMemory, vertexStagingBufferMemory;

	//Create CPU buffers
	BufferUtils::createBuffer(device, vertexBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, vertexStagingBuffer, vertexStagingBufferMemory);
	BufferUtils::createBuffer(device, indexBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, indexStagingBuffer, indexStagingBufferMemory);

	//Map memory
	void *vertexData, *indexData;
	vkMapMemory(device.logical, vertexStagingBufferMemory, 0, vertexBufferSize, 0, &vertexData);
	vkMapMemory(device.logical, indexStagingBufferMemory, 0, indexBufferSize, 0, &indexData);

	memcpy(vertexData, geometry->vertices.data(), (size_t)vertexBufferSize);
	memcpy(indexData, geometry->indices.data(), (size_t)indexBufferSize);

	vkUnmapMemory(device.logical, vertexStagingBufferMemory);
	vkUnmapMemory(device.logical, indexStagingBufferMemory);

	//Create GPU buffers
	BufferUtils::createBuffer(device, vertexBufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, geometry->vertexBuffer, geometry->vertexBufferMemory);
	BufferUtils::createBuffer(device, indexBufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, geometry->indexBuffer, geometry->indexBufferMemory);

	//Copy from CPU buffer to GPU buffer
	BufferUtils::copyBuffer(&device, &graphicsQueue, &commandPool, vertexStagingBuffer, geometry->vertexBuffer, vertexBufferSize);
	BufferUtils::copyBuffer(&device, &graphicsQueue, &commandPool, indexStagingBuffer, geometry->indexBuffer, indexBufferSize);

	//Clean the staging (CPU) buffer
	vkDestroyBuffer(device.logical, vertexStagingBuffer, nullptr);
	vkFreeMemory(device.logical, vertexStagingBufferMemory, nullptr);
	vkDestroyBuffer(device.logical, indexStagingBuffer, nullptr);
	vkFreeMemory(device.logical, indexStagingBufferMemory, nullptr);

}

void VkCraft::createDescriptorPool()
{
	std::array<VkDescriptorPoolSize, 2> poolSizes = {};

	poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	poolSizes[0].descriptorCount = 1;
	poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	poolSizes[1].descriptorCount = 1;

	VkDescriptorPoolCreateInfo poolInfo = {};
	poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
	poolInfo.pPoolSizes = poolSizes.data();
	poolInfo.maxSets = 1;

	if (vkCreateDescriptorPool(device.logical, &poolInfo, nullptr, &descriptorPool) != VK_SUCCESS)
	{
		throw std::runtime_error("vkCraft: Failed to create descriptor pool!");
	}
}

void VkCraft::createDescriptorSet()
{
	VkDescriptorSetAllocateInfo allocInfo = {};
	allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocInfo.descriptorPool = descriptorPool;
	allocInfo.descriptorSetCount = 1;
	allocInfo.pSetLayouts = &descriptorSetLayout;

	if (vkAllocateDescriptorSets(device.logical, &allocInfo, &descriptorSet) != VK_SUCCESS)
	{
		throw std::runtime_error("vkCraft: Failed to allocate descriptor set!");
	}

	VkDescriptorBufferInfo bufferInfo = {};
	bufferInfo.buffer = uniformBuffer;
	bufferInfo.offset = 0;
	bufferInfo.range = sizeof(UniformBufferObject);

	VkDescriptorImageInfo imageInfo = {};
	imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	imageInfo.imageView = texture.imageView;
	imageInfo.sampler = textureSampler;

	std::array<VkWriteDescriptorSet, 2> descriptorWrites = {};

	descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	descriptorWrites[0].dstSet = descriptorSet;
	descriptorWrites[0].dstBinding = 0;
	descriptorWrites[0].dstArrayElement = 0;
	descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	descriptorWrites[0].descriptorCount = 1;
	descriptorWrites[0].pBufferInfo = &bufferInfo;

	descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	descriptorWrites[1].dstSet = descriptorSet;
	descriptorWrites[1].dstBinding = 1;
	descriptorWrites[1].dstArrayElement = 0;
	descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	descriptorWrites[1].descriptorCount = 1;
	descriptorWrites[1].pImageInfo = &imageInfo;

	vkUpdateDescriptorSets(device.logical, static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
}

void VkCraft::createCommandPool(VkCommandPool *commandPool)
{
	QueueFamilyIndices queueFamilyIndices = device.getQueueFamilyIndices(surface);

	VkCommandPoolCreateInfo poolInfo = {};
	poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	poolInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily;

	if (vkCreateCommandPool(device.logical, &poolInfo, nullptr, commandPool) != VK_SUCCESS)
	{
		throw std::runtime_error("vkCraft: Failed to create command pool");
	}
}

void VkCraft::createRenderingCommandBuffers()
{
	renderCommandBuffers.resize(swapChainFramebuffers.size());

	VkCommandBufferAllocateInfo allocInfo = {};
	allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocInfo.commandPool = renderCommandPool;
	allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocInfo.commandBufferCount = (uint32_t)renderCommandBuffers.size();

	if (vkAllocateCommandBuffers(device.logical, &allocInfo, renderCommandBuffers.data()) != VK_SUCCESS)
	{
		throw std::runtime_error("vkCraft: Failed to allocate command buffers!");
	}

	for (size_t j = 0; j < world.geometries.size(); j++)
	{
		createGeometryBuffers(world.geometries[j]);
	}

	for (size_t i = 0; i < renderCommandBuffers.size(); i++)
	{
		VkCommandBufferBeginInfo beginInfo = {};
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		beginInfo.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;

		vkBeginCommandBuffer(renderCommandBuffers[i], &beginInfo);

		VkRenderPassBeginInfo renderPassInfo = {};
		renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		renderPassInfo.renderPass = renderPass;
		renderPassInfo.framebuffer = swapChainFramebuffers[i];
		renderPassInfo.renderArea.offset = { 0, 0 };
		renderPassInfo.renderArea.extent = swapChainExtent;

		std::array<VkClearValue, 2> clearValues = {};
		clearValues[0].color = { 0.53f, 0.81f, 0.98f, 1.0f };
		clearValues[1].depthStencil = { 1.0f, 0 };

		renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
		renderPassInfo.pClearValues = clearValues.data();

		vkCmdBeginRenderPass(renderCommandBuffers[i], &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
		vkCmdBindPipeline(renderCommandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);

		vkCmdBindDescriptorSets(renderCommandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSet, 0, nullptr);

		for (Geometry* geometry : world.geometries)
		{
			if (!geometry->hasBuffers())
			{
				continue;
			}

			VkBuffer vertexBuffers[] = { geometry->vertexBuffer };
			VkDeviceSize offsets[] = { 0 };
			vkCmdBindVertexBuffers(renderCommandBuffers[i], 0, 1, vertexBuffers, offsets);
			vkCmdBindIndexBuffer(renderCommandBuffers[i], geometry->indexBuffer, 0, VK_INDEX_TYPE_UINT32);
			vkCmdDrawIndexed(renderCommandBuffers[i], static_cast<uint32_t>(geometry->indices.size()), 1, 0, 0, 0);
		}

		vkCmdEndRenderPass(renderCommandBuffers[i]);

		if (vkEndCommandBuffer(renderCommandBuffers[i]) != VK_SUCCESS)
		{
			throw std::runtime_error("vkCraft: Failed to record command buffer!");
		}
	}
}

void VkCraft::createSyncObjects()
{
	imageAvailableSemaphores.resize(CONCURRENT_FRAMES);
	renderFinishedSemaphores.resize(CONCURRENT_FRAMES);
	inFlightFences.resize(CONCURRENT_FRAMES);

	VkSemaphoreCreateInfo semaphoreInfo = {};
	semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

	VkFenceCreateInfo fenceInfo = {};
	fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

	for (size_t i = 0; i < static_cast<size_t>(CONCURRENT_FRAMES); i++)
	{
		if (vkCreateSemaphore(device.logical, &semaphoreInfo, nullptr, &imageAvailableSemaphores[i]) != VK_SUCCESS ||
			vkCreateSemaphore(device.logical, &semaphoreInfo, nullptr, &renderFinishedSemaphores[i]) != VK_SUCCESS ||
			vkCreateFence(device.logical, &fenceInfo, nullptr, &inFlightFences[i]) != VK_SUCCESS)
		{
			throw std::runtime_error("vkCraft: Failed to create synchronization objects for a frame!");
		}
	}
}

bool VkCraft::isPhysicalDeviceSuitable(VkPhysicalDevice physicalDevice)
{
	// FIX: use the passed physicalDevice parameter to get queue families,
	// not this->device.physical which is VK_NULL_HANDLE during enumeration
	Device tempDevice;
	tempDevice.physical = physicalDevice;
	QueueFamilyIndices indices = tempDevice.getQueueFamilyIndices(surface);

	bool extensionsSupported = checkDeviceExtensionSupport(physicalDevice);

	bool swapChainAdequate = false;
	if (extensionsSupported)
	{
		SwapChainSupportDetails swapChainSupport = querySwapChainSupport(physicalDevice);
		swapChainAdequate = !swapChainSupport.formats.empty() && !swapChainSupport.presentModes.empty();
	}

	VkPhysicalDeviceFeatures supportedFeatures;
	vkGetPhysicalDeviceFeatures(physicalDevice, &supportedFeatures);

	return indices.isComplete() && extensionsSupported && swapChainAdequate && supportedFeatures.samplerAnisotropy;
}

bool VkCraft::checkDeviceExtensionSupport(VkPhysicalDevice device)
{
	uint32_t extensionCount;
	vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);

	std::vector<VkExtensionProperties> availableExtensions(extensionCount);
	vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());

	std::set<std::string> requiredExtensions(deviceExtensions.begin(), deviceExtensions.end());

	for (const auto& extension : availableExtensions)
	{
		requiredExtensions.erase(extension.extensionName);
	}

	return requiredExtensions.empty();
}

VkSurfaceFormatKHR VkCraft::chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats)
{
	if (availableFormats.size() == 1 && availableFormats[0].format == VK_FORMAT_UNDEFINED)
	{
		return { VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR };
	}

	for (const auto& availableFormat : availableFormats)
	{
		if (availableFormat.format == VK_FORMAT_B8G8R8A8_UNORM && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
		{
			return availableFormat;
		}
	}

	return availableFormats[0];
}

// FIX: pass by const reference to avoid unnecessary vector copy
VkPresentModeKHR VkCraft::chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes)
{
	VkPresentModeKHR bestMode = VK_PRESENT_MODE_FIFO_KHR;

	for (const auto& availablePresentMode : availablePresentModes)
	{
		if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR)
		{
			return availablePresentMode;
		}
		else if (availablePresentMode == VK_PRESENT_MODE_IMMEDIATE_KHR)
		{
			bestMode = availablePresentMode;
		}
	}

	return bestMode;
}

VkExtent2D VkCraft::chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities)
{
	if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max())
	{
		return capabilities.currentExtent;
	}
	else
	{
		int width, height;
		glfwGetFramebufferSize(window, &width, &height);

		VkExtent2D actualExtent = { static_cast<uint32_t>(width), static_cast<uint32_t>(height) };
		actualExtent.width = std::max(capabilities.minImageExtent.width, std::min(capabilities.maxImageExtent.width, actualExtent.width));
		actualExtent.height = std::max(capabilities.minImageExtent.height, std::min(capabilities.maxImageExtent.height, actualExtent.height));

		return actualExtent;
	}
}

bool VkCraft::checkValidationLayerSupport()
{
	uint32_t layerCount;
	vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

	std::vector<VkLayerProperties> availableLayers(layerCount);
	vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

	for (const char* layerName : validationLayers)
	{
		bool layerFound = false;

		for (const auto& layerProperties : availableLayers)
		{
			if (strcmp(layerName, layerProperties.layerName) == 0)
			{
				layerFound = true;
				break;
			}
		}

		if (!layerFound)
		{
			return false;
		}
	}

	return true;
}

std::vector<const char*> VkCraft::getRequiredExtensions()
{
	uint32_t glfwExtensionCount = 0;
	const char** glfwExtensions;
	glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

	std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

	if (ENABLE_VALIDATION_LAYERS)
	{
		extensions.push_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);
	}

	return extensions;
}

VkFormat VkCraft::findSupportedFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features)
{
	for (VkFormat format : candidates)
	{
		VkFormatProperties props;
		vkGetPhysicalDeviceFormatProperties(device.physical, format, &props);

		if (tiling == VK_IMAGE_TILING_LINEAR && (props.linearTilingFeatures & features) == features)
		{
			return format;
		}
		else if (tiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & features) == features)
		{
			return format;
		}
	}

	throw std::runtime_error("vkCraft: Failed to find supported format!");
}

VkFormat VkCraft::findDepthFormat()
{
	return findSupportedFormat({ VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT }, VK_IMAGE_TILING_OPTIMAL, VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);
}

bool VkCraft::hasStencilComponent(VkFormat format)
{
	return format == VK_FORMAT_D32_SFLOAT_S8_UINT || format == VK_FORMAT_D24_UNORM_S8_UINT;
}

void VkCraft::createDepthResources()
{
	VkFormat depthFormat = findDepthFormat();
	createImage(swapChainExtent.width, swapChainExtent.height, depthFormat, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, depth.image, depth.imageMemory);
	depth.imageView = createImageView(depth.image, depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT);
	transitionImageLayout(depth.image, depthFormat, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
}

VkCommandBuffer VkCraft::beginSingleTimeCommands()
{
	return CommandBufferUtils::beginSingleTimeCommands(&device, &commandPool);
}

void VkCraft::endSingleTimeCommands(VkCommandBuffer commandBuffer)
{
	CommandBufferUtils::endSingleTimeCommands(&device, &graphicsQueue, &commandPool, &commandBuffer);
}

void VkCraft::createImage(uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage& image, VkDeviceMemory& imageMemory)
{
	VkImageCreateInfo imageInfo = {};
	imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageInfo.imageType = VK_IMAGE_TYPE_2D;
	imageInfo.extent.width = width;
	imageInfo.extent.height = height;
	imageInfo.extent.depth = 1;
	imageInfo.mipLevels = 1;
	imageInfo.arrayLayers = 1;
	imageInfo.format = format;
	imageInfo.tiling = tiling;
	imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	imageInfo.usage = usage;
	imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	if (vkCreateImage(device.logical, &imageInfo, nullptr, &image) != VK_SUCCESS)
	{
		throw std::runtime_error("vkCraft: Failed to create image!");
	}

	VkMemoryRequirements memRequirements;
	vkGetImageMemoryRequirements(device.logical, image, &memRequirements);

	VkMemoryAllocateInfo allocInfo = {};
	allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocInfo.allocationSize = memRequirements.size;
	allocInfo.memoryTypeIndex = device.findMemoryType(memRequirements.memoryTypeBits, properties);

	if (vkAllocateMemory(device.logical, &allocInfo, nullptr, &imageMemory) != VK_SUCCESS)
	{
		throw std::runtime_error("vkCraft: Failed to allocate image memory!");
	}

	vkBindImageMemory(device.logical, image, imageMemory, 0);
}

VkImageView VkCraft::createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags)
{
	VkImageViewCreateInfo viewInfo = {};
	viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	viewInfo.image = image;
	viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
	viewInfo.format = format;
	viewInfo.subresourceRange.aspectMask = aspectFlags;
	viewInfo.subresourceRange.baseMipLevel = 0;
	viewInfo.subresourceRange.levelCount = 1;
	viewInfo.subresourceRange.baseArrayLayer = 0;
	viewInfo.subresourceRange.layerCount = 1;

	VkImageView imageView;
	if (vkCreateImageView(device.logical, &viewInfo, nullptr, &imageView) != VK_SUCCESS)
	{
		throw std::runtime_error("vkCraft: Failed to create texture image view!");
	}

	return imageView;
}

void VkCraft::transitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout)
{
	VkCommandBuffer commandBuffer = beginSingleTimeCommands();

	VkImageMemoryBarrier barrier = {};
	barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barrier.oldLayout = oldLayout;
	barrier.newLayout = newLayout;
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.image = image;

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

	barrier.subresourceRange.baseMipLevel = 0;
	barrier.subresourceRange.levelCount = 1;
	barrier.subresourceRange.baseArrayLayer = 0;
	barrier.subresourceRange.layerCount = 1;

	VkPipelineStageFlags sourceStage;
	VkPipelineStageFlags destinationStage;

	if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
	{
		barrier.srcAccessMask = 0;
		barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

		sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
		destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
	}
	else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
	{
		barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

		sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
		destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
	}
	else if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
	{
		barrier.srcAccessMask = 0;
		barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

		sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
		destinationStage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
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
	region.bufferOffset = 0;
	region.bufferRowLength = 0;
	region.bufferImageHeight = 0;
	region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	region.imageSubresource.mipLevel = 0;
	region.imageSubresource.baseArrayLayer = 0;
	region.imageSubresource.layerCount = 1;
	region.imageOffset = { 0, 0, 0 };
	region.imageExtent = { width, height, 1 };

	vkCmdCopyBufferToImage(commandBuffer, buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

	endSingleTimeCommands(commandBuffer);
}

void VkCraft::createTextureImage(const char *fname)
{
	int texWidth, texHeight, texChannels;

	//Load image
	stbi_uc* pixels = stbi_load(fname, &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);

	if (!pixels)
	{
		throw std::runtime_error("vkCraft: Failed to load texture image!");
	}

	VkDeviceSize imageSize = texWidth * texHeight * 4;

	//Create buffer to transfer to GPU memory
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

	//Create image view
	texture.imageView = createImageView(texture.image, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_ASPECT_COLOR_BIT);
}

VkShaderModule VkCraft::createShaderModule(const std::vector<char>& code)
{
	VkShaderModuleCreateInfo createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	createInfo.codeSize = code.size();
	createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());

	VkShaderModule shaderModule;
	if (vkCreateShaderModule(device.logical, &createInfo, nullptr, &shaderModule) != VK_SUCCESS)
	{
		throw std::runtime_error("vkCraft: Failed to create shader module!");
	}

	return shaderModule;
}

SwapChainSupportDetails VkCraft::querySwapChainSupport(VkPhysicalDevice device)
{
	SwapChainSupportDetails details;
	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &details.capabilities);

	uint32_t formatCount;
	vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, nullptr);

	if (formatCount != 0)
	{
		details.formats.resize(formatCount);
		vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, details.formats.data());
	}

	uint32_t presentModeCount;
	vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, nullptr);

	if (presentModeCount != 0)
	{
		details.presentModes.resize(presentModeCount);
		vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, details.presentModes.data());
	}

	return details;
}

VkBool32 VkCraft::debugCallback(VkDebugReportFlagsEXT flags, VkDebugReportObjectTypeEXT objType, uint64_t obj, size_t location, int32_t code, const char* layerPrefix, const char* msg, void* userData)
{
	std::cerr << "vkCraft: Validation layer: " << msg << std::endl;
	return VK_FALSE;
}
