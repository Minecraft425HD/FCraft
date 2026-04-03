#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/constants.hpp>

#include <iostream>
#include <fstream>
#include <stdexcept>
#include <algorithm>
#include <chrono>
#include <vector>
#include <cstring>
#include <cstdlib>
#include <array>
#include <set>
#include <ctime>

#include "Device.h"
#include "BufferUtils.h"
#include "QueueFamilyIndices.h"
#include "SwapChainSupportDetails.h"
#include "UniformBufferObject.h"
#include "Texture.h"
#include "FileUtils.h"
#include "CommandBufferUtils.h"
#include "Object3D.h"
#include "FirstPersonCamera.h"
#include "ChunkWorld.h"

class VkCraft
{
public:
	// Maximum amount of frames that can be rendered simultaneously
	const int CONCURRENT_FRAMES = 1;

	// Render distance in chunks
	const int RENDER_DISTANCE = 5;

	// World seed
	const int WORLD_SEED = 349995;

	// Validation layer control – disable in release builds via NDEBUG
#ifdef NDEBUG
	const bool ENABLE_VALIDATION_LAYERS = false;
#else
	const bool ENABLE_VALIDATION_LAYERS = true;
#endif

	// Vulkan context and window
	GLFWwindow *window;
	VkInstance instance;
	VkSurfaceKHR surface;

	// Debug messenger (modern VK_EXT_debug_utils replacement for the
	// deprecated VK_EXT_debug_report / VkDebugReportCallbackEXT)
	VkDebugUtilsMessengerEXT debugMessenger = VK_NULL_HANDLE;

	Device device;

	// Graphics queue
	VkQueue graphicsQueue;
	VkQueue presentQueue;

	// Swap chain
	VkSwapchainKHR swapChain;
	std::vector<VkImage> swapChainImages;
	VkFormat swapChainImageFormat;
	VkExtent2D swapChainExtent;
	std::vector<VkImageView> swapChainImageViews;
	std::vector<VkFramebuffer> swapChainFramebuffers;

	VkRenderPass renderPass;

	VkPipelineLayout pipelineLayout;
	VkDescriptorSetLayout descriptorSetLayout;
	VkPipeline graphicsPipeline;

	VkCommandPool commandPool;

	// Render pipeline
	VkCommandPool renderCommandPool;
	std::vector<VkCommandBuffer> renderCommandBuffers;

	// Uniform buffer
	VkBuffer uniformBuffer;
	VkDeviceMemory uniformBufferMemory;

	Texture texture;
	VkSampler textureSampler = VK_NULL_HANDLE;

	// Depth buffer
	Texture depth;

	// Descriptors
	VkDescriptorPool descriptorPool;
	VkDescriptorSet descriptorSet;

	// Synchronization semaphores (one for each concurrent frame)
	std::vector<VkSemaphore> imageAvailableSemaphores;
	std::vector<VkSemaphore> renderFinishedSemaphores;
	std::vector<VkFence> inFlightFences;
	size_t currentFrame = 0;

	// Object3D and camera
	Object3D model;
	FirstPersonCamera camera;
	glm::ivec3 cameraIndex = { 0, 0, 0 };
	UniformBufferObject uniformBuf;
	double time = 0.0;
	double delta = 0.0;

	ChunkWorld world = ChunkWorld(WORLD_SEED);

	/**
	 * Use LunarG validation layers provided by the SDK.
	 */
	const std::vector<const char*> validationLayers =
	{
		"VK_LAYER_KHRONOS_validation"
	};

	const std::vector<const char*> deviceExtensions =
	{
		VK_KHR_SWAPCHAIN_EXTENSION_NAME
	};

	/**
	 * Populate a VkDebugUtilsMessengerCreateInfoEXT structure.
	 */
	void populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo);

	/**
	 * Create the debug utils messenger extension object.
	 */
	VkResult CreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pMessenger);

	/**
	 * Destroy the debug utils messenger extension object.
	 */
	void DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT messenger, const VkAllocationCallbacks* pAllocator);

	/**
	 * Start the application.
	 */
	void run();

	/**
	 * Logic loop where the draw call is made.
	 */
	void loop();

	/**
	 * Update the uniform buffers (and run some logic), before drawing the next frame.
	 */
	void update();

	/**
	 * Draw frame to the swap chain and display it.
	 */
	void render();

	/**
	 * Initialize the data structures, get devices and prepare Vulkan for rendering.
	 */
	void initialize();

	/**
	 * Recreate the whole swap chain.
	 */
	void recreateSwapChain();

	/**
	 * Recreate the command buffers to update the geometry to be drawn.
	 */
	void recreateRenderingCommandBuffers();

	/**
	 * Cleanup swap chain elements. Can be used to clear the swap chain before recreating it
	 * using a different layout.
	 */
	void cleanupSwapChain();

	/**
	 * Full cleanup of memory.
	 */
	void cleanup();

	/**
	 * Create a new Vulkan instance.
	 */
	void createInstance();

	/**
	 * Setup debug messenger.
	 */
	void setupDebugMessenger();

	/**
	 * Create a window surface using GLFW.
	 */
	void createSurface();

	/**
	 * Choose an appropriate physical device to be used.
	 */
	void pickPhysicalDevice();

	/**
	 * Create a logical device using the selected physical device.
	 */
	void createLogicalDevice();

	/**
	 * Create the swap chain for rendering.
	 */
	void createSwapChain();

	/**
	 * Create image views.
	 */
	void createImageViews();

	/**
	 * Create render pass (indicates where to read and write rendered data).
	 */
	void createRenderPass();

	/**
	 * Create descriptor set layout.
	 */
	void createDescriptorSetLayout();

	/**
	 * Initialize graphics pipeline, load shaders, configure vertex format and rendering steps.
	 */
	void createGraphicsPipeline();

	/**
	 * Create frame buffers for the swap chain.
	 */
	void createFramebuffers();

	/**
	 * Create a command pool to send commands to the GPU.
	 */
	void createCommandPool(VkCommandPool *commandPool);

	/**
	 * Create vertex buffer.
	 */
	void createGeometryBuffers(Geometry *geometry);

	void createDescriptorPool();

	void createDescriptorSet();

	/**
	 * Handle image layout transitions.
	 * TODO: Move to ImageUtils
	 */
	void transitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout);

	/**
	 * Copy buffer to image helper.
	 * TODO: Move to ImageUtils
	 */
	void copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height);

	/**
	 * Create texture image.
	 * TODO: Move to ImageUtils
	 */
	void createTextureImage(const char *fname);

	/**
	 * Create image view (generic).
	 * TODO: Move to ImageViewUtils
	 */
	VkImageView createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags);

	/**
	 * Create Vulkan image and allocate memory for it.
	 * TODO: Move to ImageUtils
	 */
	void createImage(uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage& image, VkDeviceMemory& imageMemory);

	/**
	 * Depth buffer creation.
	 */
	void createDepthResources();

	/**
	 * Check if the format for depth buffer has a stencil component.
	 */
	bool hasStencilComponent(VkFormat format);

	/**
	 * Get depth buffer format.
	 */
	VkFormat findDepthFormat();

	/**
	 * Check formats supported by the device from a vector of candidates.
	 */
	VkFormat findSupportedFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features);

	/**
	 * Create the actual draw command buffer.
	 */
	void createRenderingCommandBuffers();

	/**
	 * Create synchronization semaphores and fences.
	 */
	void createSyncObjects();

	/**
	 * Begin a single-time-use command buffer.
	 * TODO: Move to CommandBufferUtils
	 */
	VkCommandBuffer beginSingleTimeCommands();

	/**
	 * End single-time-use command buffer.
	 * TODO: Move to CommandBufferUtils
	 */
	void endSingleTimeCommands(VkCommandBuffer commandBuffer);

	/**
	 * Create a shader from SPIR-V code.
	 */
	VkShaderModule createShaderModule(const std::vector<char>& code);

	/**
	 * Check if a device has all the required capabilities
	 * (is a discrete GPU and supports geometry shading).
	 */
	bool isPhysicalDeviceSuitable(VkPhysicalDevice physical);

	/**
	 * Check available device extensions.
	 */
	bool checkDeviceExtensionSupport(VkPhysicalDevice physical);

	/**
	 * Choose swap chain format (color depth).
	 */
	VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats);

	/**
	 * Choose swap chain presentation mode.
	 */
	VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR> availablePresentModes);

	/**
	 * Choose resolution of the swap chain images (get it from the window size).
	 */
	VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities);

	/**
	 * Check compatible swap chain support for our device.
	 */
	SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice physical);

	/**
	 * Get required extensions; if validation layers are active also add debug extension to the list.
	 */
	std::vector<const char*> getRequiredExtensions();

	/**
	 * Check if validation layers are supported.
	 */
	bool checkValidationLayerSupport();

	/**
	 * Debug callback (VK_EXT_debug_utils).
	 */
	static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
		VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
		VkDebugUtilsMessageTypeFlagsEXT messageType,
		const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
		void* pUserData);
};
