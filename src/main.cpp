#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <iostream>
#include <stdexcept>
#include <functional>
#include <vector>
#include <set>
#include <algorithm>
#include <fstream>

#include <glm/glm.hpp>

#include "math.hpp"

#ifdef _DEBUG
const bool enableValidationLayers = true;
#else
const bool enableValidationLayers = false;
#endif

const unsigned int WIDTH = 1280;
const unsigned int HEIGHT = 720;

class HelloTriangleApplication;


/*
Proxy function for creating and destroying extension function for debug messages
*/
VkResult CreateDebugReportCallbackEXT(VkInstance instance, const VkDebugReportCallbackCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugReportCallbackEXT* pCallback) {
	auto func = (PFN_vkCreateDebugReportCallbackEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugReportCallbackEXT");
	if (func != nullptr) {
		return func(instance, pCreateInfo, pAllocator, pCallback);
	}
	else {
		return VK_ERROR_EXTENSION_NOT_PRESENT;
	}
}

void DestroyDebugReportCallbackEXT(VkInstance instance, VkDebugReportCallbackEXT callback, const VkAllocationCallbacks* pAllocator) {
	auto func = (PFN_vkDestroyDebugReportCallbackEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugReportCallbackEXT");
	if (func != nullptr) {
		func(instance, callback, pAllocator);
	}
}

static std::vector<char> readFile(const std::string &filename) {
	//ATE : at the end. Reasoning: start at the end can help determine the size of the file
	std::ifstream file(filename, std::ios::ate | std::ios::binary);

	if (!file.is_open()) {
		throw std::runtime_error("ERROR: Failed to open file!");
	}
	//Allocate buffer
	size_t fileSize = (size_t)file.tellg();
	std::vector<char> buffer(fileSize);

	file.seekg(0);
	file.read(buffer.data(), fileSize);
	file.close();

	return buffer;
}

/*
Required device validation layers
*/
const std::vector<const char*> validationLayers = {
	"VK_LAYER_LUNARG_standard_validation",
	"VK_LAYER_LUNARG_assistant_layer"
};

/*
Required device extensions
*/
const std::vector<const char*> deviceExtensions = {
	VK_KHR_SWAPCHAIN_EXTENSION_NAME
};

/*
Struct to hold indices of queue families.
isComplete checks, if all families are present
*/
struct QueueFamilyIndices {
	int graphicsFamily = -1;
	int presentFamily = -1;

	bool isComplete() {
		return graphicsFamily >= 0
			&& presentFamily >= 0;
	}
};

/*
Struct to hold information need to create swapchain
	capabilities : basic surface capabilities (min/max number of images in swap chain, min/max width and height of images)
	surface formats (pixel format, color space)
	available presentation modes
*/
struct SwapchainSupportDetails {
	VkSurfaceCapabilitiesKHR capabilities;
	std::vector<VkSurfaceFormatKHR> formats;
	std::vector<VkPresentModeKHR> presentModes;
};

void windowKeyCallback(GLFWwindow *pWindow, int key, int scancode, int action, int mods) {
	switch (key) {
	case GLFW_KEY_ESCAPE:
		if (action == GLFW_PRESS)
			glfwSetWindowShouldClose(pWindow, true);
		break;
	}
}


class HelloTriangleApplication {
public:
	void run() {
		initWindow();
		initVulkan();
		mainLoop();
		cleanup();
	}

	/*
	Recreates whole swapchain if ie. window was resized
	*/
	void recreateSwapchain() {
		int width, height;
		glfwGetWindowSize(m_window, &width, &height);
		if (width == 0 || height == 0) {
			return;
		}

		vkDeviceWaitIdle(m_logicalDevice);

		cleanupSwapchain();

		createSwapchain();
		createImageViews();
		createRenderPass();
		createGraphicsPipeline();
		createFramebuffers();
		createCommandBuffers();
	}

private:
	void initWindow() {
		glfwInit();

		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
		glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

		m_window = glfwCreateWindow(WIDTH, HEIGHT, "Vulkan Triangle", nullptr, nullptr);
		if (!m_window) {
			glfwTerminate();
			throw std::runtime_error("ERROR: Failed to create GLFW window!");
		}

		glfwSetWindowUserPointer(m_window, this);
		glfwSetKeyCallback(m_window, windowKeyCallback);
		glfwSetWindowSizeCallback(m_window, windowSizeCallback);
	}

	static void windowSizeCallback(GLFWwindow* window, int width, int height) {
		HelloTriangleApplication* app = reinterpret_cast<HelloTriangleApplication*>(glfwGetWindowUserPointer(window));
		app->recreateSwapchain();
	}

	void initVulkan() {
		createInstance();
		setupDebugCallback();
		createSurface();
		selectPhysicalDevice();
		createLogicalDevice();
		createSwapchain();
		createImageViews();
		createRenderPass();
		createDescriptorSetLayout();
		createGraphicsPipeline();
		createFramebuffers();
		createCommandPool();
		createVertexBuffer();
		createIndexBuffer();
		createUniformBuffer();
		createDescriptorPool();
		createDescriptorSet();
		createCommandBuffers();
		createSemaphores();
	}

	void mainLoop() {
		while (!glfwWindowShouldClose(m_window)) {
			glfwPollEvents();

			updateUniformData();

			drawFrame();
		}

		vkDeviceWaitIdle(m_logicalDevice);
	}

	void cleanup() {
		//Vulkan cleanup
		vkDestroySemaphore(m_logicalDevice, m_imageAvailableSemaphore, nullptr);
		vkDestroySemaphore(m_logicalDevice, m_renderFinishedSemaphore, nullptr);

		cleanupSwapchain();

		vkDestroyDescriptorSetLayout(m_logicalDevice, m_descriptorSetLayout, nullptr);
		vkDestroyDescriptorPool(m_logicalDevice, m_descriptorPool, nullptr);

		vkDestroyCommandPool(m_logicalDevice, m_commandPool, nullptr);

		vkDestroyBuffer(m_logicalDevice, m_vertexBuffer, nullptr);
		vkFreeMemory(m_logicalDevice, m_vertexBufferMemory, nullptr);
		vkDestroyBuffer(m_logicalDevice, m_indexBuffer, nullptr);
		vkFreeMemory(m_logicalDevice, m_indexBufferMemory, nullptr);
		vkDestroyBuffer(m_logicalDevice, m_uniformBuffer, nullptr);
		vkFreeMemory(m_logicalDevice, m_uniformBufferMemory, nullptr);

		vkDestroyDevice(m_logicalDevice, nullptr);
		DestroyDebugReportCallbackEXT(m_instance, m_debugCallback, nullptr);
		vkDestroySurfaceKHR(m_instance, m_surface, nullptr);
		vkDestroyInstance(m_instance, nullptr);

		//GLFW cleanup
		glfwDestroyWindow(m_window);
		glfwTerminate();
	}

	void createInstance() {
		/*
		Checking for support of given validation layers before instance creation
		*/
		if (enableValidationLayers && !checkValidationLayerSupport()) {
			throw std::runtime_error("ERROR:Validation layers requested, but not available!");
		}

		/*
		Technically optional struct to describe program, which Vulkan version it uses, engine name etc.
		*/
		VkApplicationInfo programInfo = {};
		programInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
		programInfo.pApplicationName = "Vulkan Triangle";
		programInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
		programInfo.pEngineName = "No Engine";
		programInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
		programInfo.apiVersion = VK_API_VERSION_1_0;

		/*
		This struct is mandatory and tells Vulkan driver which extensions/validation layers to use
		and for which devices (or whole program)
		*/
		VkInstanceCreateInfo createInfo = {};
		createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
		createInfo.pApplicationInfo = &programInfo;

		/*
		Here we get extensions from used window system (Vulkan is platform agnostic)
		*/
		uint32_t glfwExtensionCount = 0;
		const char** glfwExtensions;
		glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

		auto extensions = getRequiredExtensions();
		createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
		createInfo.ppEnabledExtensionNames = extensions.data();

		createInfo.enabledLayerCount = 0;

		/*
		Adding validation layers into the instance
		*/
		if (enableValidationLayers) {
			createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
			createInfo.ppEnabledLayerNames = validationLayers.data();
		}
		else {
			createInfo.enabledLayerCount = 0;
		}

		/*
		Vulkan instance creation
		*/
		VkResult result = vkCreateInstance(&createInfo, nullptr, &m_instance);
		if (vkCreateInstance(&createInfo, nullptr, &m_instance) != VK_SUCCESS) {
			throw std::runtime_error("ERROR:Failed to create instance!");
		}
	}

	bool checkValidationLayerSupport() {
		/*
		First we need to get all of validation layers
		*/
		uint32_t layerCount = 0;
		vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

		std::vector<VkLayerProperties> availableLayers(layerCount);
		vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

		/*
		Now we check if all of given validation layers
		are in supported validation layers
		*/
		for (const char* layerName : validationLayers) {
			bool layerFound = false;

			for (const auto& layerProperties : availableLayers) {
				if (strcmp(layerName, layerProperties.layerName) == 0) {
					std::cout << "Layer: " << layerName << " found." << std::endl;
					layerFound = true;
					break;
				}
			}

			if (!layerFound) {
				return false;
			}
		}

		return true;
	}

	void setupDebugCallback() {
		if (!enableValidationLayers) {
			return;
		}
		/*
		Again as with any other Vulkan object we fill info struct
		Flags say which messages we want to receive
		pUserData can be used to transfer use data into the callback function
		(rg. send a whole program object)
		*/
		VkDebugReportCallbackCreateInfoEXT createInfo = {};
		createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT;
		createInfo.flags = VK_DEBUG_REPORT_ERROR_BIT_EXT | VK_DEBUG_REPORT_WARNING_BIT_EXT;
		createInfo.pfnCallback = debugCallback;

		/*
		Because debug callback is specific for the whole Vulkan instance and it's layers,
		we need to set it first
		*/
		if (CreateDebugReportCallbackEXT(m_instance, &createInfo, nullptr, &m_debugCallback) != VK_SUCCESS) {
			throw std::runtime_error("ERROR: Failed to set up debug callback!");
		}
	}

	/*
	Debug callback function that reports messages from validation layers
	VKAPI_ATTR and VKAPI_CALL guarantee the right Vulkan signature for the func.
	flags
		VK_DEBUG_REPORT_INFORMATION_BIT_EXT
		VK_DEBUG_REPORT_WARNING_BIT_EXT
		VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT
		VK_DEBUG_REPORT_ERROR_BIT_EXT
		VK_DEBUG_REPORT_DEBUG_BIT_EXT

	Returns boolean: true if we want to abort whole Vulkan call, which
	set off the validation layer (good for testing) otherwise return VK_FALSE
	*/
	static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
		VkDebugReportFlagsEXT flags,
		VkDebugReportObjectTypeEXT objType,
		uint64_t obj,
		size_t location,
		int32_t code,
		const char* layer_prefix,
		const char* msg,
		void* userData) {

		std::cerr << "Validation layer: " << msg << std::endl;

		return VK_FALSE;
	}

	/*
	Returns vector of extensions
	GLFW extensions are needed but DEBUG REPORT is added
	if validation layers are on
	*/
	std::vector<const char*> getRequiredExtensions() {
		uint32_t glfwExtensionCount = 0;
		const char** glfwExtensions;
		glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

		std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

		if (enableValidationLayers) {
			extensions.push_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);
		}

		return extensions;
	}

	/*
	Creates window surface
	*/
	void createSurface() {
		if (glfwCreateWindowSurface(m_instance, m_window, nullptr, &m_surface) != VK_SUCCESS) {
			throw std::runtime_error("ERROR: Failed to create window surface!");
		}
	}

	/*
	Selects physical device from all available
	devices based on it's "suitability"
	*/
	void selectPhysicalDevice() {
		uint32_t deviceCount = 0;
		vkEnumeratePhysicalDevices(m_instance, &deviceCount, nullptr);

		if (deviceCount == 0) {
			throw std::runtime_error("ERROR:Failed to find physical devices with Vulkan support!");
		}

		std::vector<VkPhysicalDevice> devices(deviceCount);
		vkEnumeratePhysicalDevices(m_instance, &deviceCount, devices.data());

		for (const auto &device : devices) {
			if (isDeviceSuitable(device)) {
				m_physicalDevice = device;
				break;
			}
		}

		if (m_physicalDevice == VK_NULL_HANDLE) {
			throw std::runtime_error("ERROR:Failed to find suitable physical device!");
		}
	}

	/*
	Checks if selected device is suitable to our needs eg.
		discrete gpu
		support for given queue families
	*/
	bool isDeviceSuitable(const VkPhysicalDevice& device) {
		/*
		//Name, version and Vulkan support
		VkPhysicalDeviceProperties deviceProperties;
		//Optional support: 64bit floats, texture compression, multi-viewport rendering
		VkPhysicalDeviceFeatures deviceFeatures;

		vkGetPhysicalDeviceProperties(device, &deviceProperties);
		vkGetPhysicalDeviceFeatures(device, &deviceFeatures);
		*/
		QueueFamilyIndices indices = findQueueFamilies(device);

		//Check the extension support
		bool extensionsSupported = checkDeviceExtensionSupport(device);

		//Check swapchain support: at least one supported image format and one present. mode for given window surface
		bool swapchainSupported = false;
		if (extensionsSupported) {
			SwapchainSupportDetails swapchainSupport = querySwapchainSupport(device);
			swapchainSupported = !swapchainSupport.formats.empty() && !swapchainSupport.presentModes.empty();
		}

		return (indices.isComplete() && extensionsSupported && swapchainSupported);
	}

	/*
	Checks if selected device supports ALL extensions listed in
	deviceExtensions global list
	*/
	bool checkDeviceExtensionSupport(VkPhysicalDevice device) {
		uint32_t extensionsCount = 0;
		vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionsCount, nullptr);

		std::vector<VkExtensionProperties> availableExtensions(extensionsCount);
		vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionsCount, availableExtensions.data());

		//Copy all extensions into a set
		std::set<std::string> requiredExtensions(deviceExtensions.begin(), deviceExtensions.end());

		//For each required extensions present, erase it from the set
		for (const auto &extension : availableExtensions) {
			std::cout << "Avail. Extension: " << extension.extensionName << std::endl;
			requiredExtensions.erase(extension.extensionName);
		}

		return requiredExtensions.empty();
	}

	/*
	Search for all queue families, which are supported by the device: compute, graphics, etc.
	*/
	QueueFamilyIndices findQueueFamilies(const VkPhysicalDevice& device) {
		QueueFamilyIndices indices;
		uint32_t queueFamilyCount = 0;
		vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

		std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
		vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

		/*
		Indices hold indices of searched queues
		*/
		int i = 0;
		for (const auto& queueFamily : queueFamilies) {
			//Hledani podpory pro grafickou frontu
			if (queueFamily.queueCount > 0 && queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
				indices.graphicsFamily = i;
			}

			/*
			Searching for support for presenting into the surface -> it's required to search separately
			It is possible to prioritize device which supports presenting and rendering on the same queue
			for better performance
			*/
			VkBool32 presentSupport = false;
			vkGetPhysicalDeviceSurfaceSupportKHR(device, i, m_surface, &presentSupport);
			if (queueFamily.queueCount > 0 && presentSupport) {
				indices.presentFamily = i;
			}

			if (indices.isComplete()) {
				break;
			}
			i++;
		}

		return indices;
	}

	/*
	Queries for swapchain support and populates the SwapchainSupportDetails struct
	*/
	SwapchainSupportDetails querySwapchainSupport(VkPhysicalDevice device) {
		SwapchainSupportDetails swapchainDetails;

		//Basic surface capabilities
		vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, m_surface, &swapchainDetails.capabilities);

		//Supported surface formats
		uint32_t formatCount = 0;
		vkGetPhysicalDeviceSurfaceFormatsKHR(device, m_surface, &formatCount, nullptr);

		if (formatCount != 0) {
			swapchainDetails.formats.resize(formatCount);
			vkGetPhysicalDeviceSurfaceFormatsKHR(device, m_surface, &formatCount, swapchainDetails.formats.data());
		}

		//Supported presentation modes
		uint32_t presentModesCount = 0;
		vkGetPhysicalDeviceSurfacePresentModesKHR(device, m_surface, &presentModesCount, nullptr);

		if (presentModesCount != 0) {
			swapchainDetails.presentModes.resize(presentModesCount);
			vkGetPhysicalDeviceSurfacePresentModesKHR(device, m_surface, &presentModesCount, swapchainDetails.presentModes.data());
		}

		return swapchainDetails;
	}

	/*
	Selects optimal VkSurface from 
	*/
	VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR> &availableFormats) {
		//Best case scenario: if the surface has no prefered format, set colorspace to SRGB and format to RGB 8bit
		if (availableFormats.size() == 1 && availableFormats[0].format == VK_FORMAT_UNDEFINED) {
			return { VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR };
		}

		//If we are free to choose
		for (const auto &availableFormat : availableFormats) {
			if (availableFormat.format == VK_FORMAT_B8G8R8A8_UNORM && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
				return availableFormat;
			}
		}

		//Is is possible to rank available formats based on some metric but it usually ok take the first one
		return availableFormats[0];
	}
	
	/*
	Selects optimal swapchain present mode
	It represents the actual conditions for showing images on the screen
	Four possible modes: 
		VK_PRESENT_MODE_IMMEDIATE_KHR: Images submitted by your application are transferred to thescreen right away, which may result in tearing.
		VK_PRESENT_MODE_FIFO_KHR: The swap chain is a queue where the display takes an image from the front of the queue when the display is refreshed
								and the program inserts rendered images at the back of the queue. If the queue is full then the program has to wait.
								This is most similar to vertical sync as found in modern games. The moment that the display is refreshed is known as "vertical blank".
		VK_PRESENT_MODE_FIFO_RELAXED_KHR: This mode only differs from the previous one if the application is late and the queue was empty at the last vertical blank.
								Instead of waiting for the next vertical blank, the image is transferred right away when it finally arrives. This may result in visible tearing.
		VK_PRESENT_MODE_MAILBOX_KHR: This is another variation of the second mode. Instead of blocking the application when the queue is full, the images that are already queued
								are simply replaced with the newer ones. This mode can be used to implement triple buffering, which allows you to avoid tearing with significantly
								less latency issues than standard vertical sync that uses double buffering.
	*/
	VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR> availablePresentModes) {
		/*
		Here we are lookig for triple buffering option and if its not found, then look for immediate mode
		FIFO mode is guaranteed to be available but there might be problems with it in some drivers
		*/	
		VkPresentModeKHR selectedMode = VK_PRESENT_MODE_FIFO_KHR;

		for (const auto& availablePresentMode : availablePresentModes) {
			if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
				return availablePresentMode;
			}
			else if (availablePresentMode == VK_PRESENT_MODE_IMMEDIATE_KHR) {
				selectedMode = availablePresentMode;
			}
		}

		return selectedMode;
	}
	
	/*
	Selects swap extent.
	Swap extent is the resolution of the swapchain images
	*/
	VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities) {
		if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
			return capabilities.currentExtent;
		}
		else {
			int width, height;
			glfwGetWindowSize(m_window, &width, &height);

			VkExtent2D actualExtent = { width, height };

			actualExtent.width = std::max(capabilities.minImageExtent.width, std::min(capabilities.maxImageExtent.width, actualExtent.width));
			actualExtent.height = std::max(capabilities.minImageExtent.height, std::min(capabilities.maxImageExtent.height, actualExtent.height));

			return actualExtent;
		}
	}

	void createLogicalDevice() {
		QueueFamilyIndices indices = findQueueFamilies(m_physicalDevice);

		/*
		Drivers so far support creation of a small number of queues
		but there is no need to need more than one -> command buffers
		are created on multiple threads and sent at once with one call
		into the main queue with small overhead
		*/
		std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
		std::set<int> uniqueQueueFamilies = { indices.graphicsFamily, indices.presentFamily };
		float queuePriority = 1.0f;
		for (int queueFamily : uniqueQueueFamilies) {
			VkDeviceQueueCreateInfo queueCreateInfo = {};
			queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
			queueCreateInfo.queueFamilyIndex = queueFamily;
			queueCreateInfo.queueCount = 1;
			queueCreateInfo.pQueuePriorities = &queuePriority;
			queueCreateInfos.push_back(queueCreateInfo);
		}

		//Specification of used device features eg. geometry shader
		VkPhysicalDeviceFeatures deviceFeatures = {};

		/*
		Main create info
		*/
		VkDeviceCreateInfo createInfo = {};
		createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
		createInfo.pQueueCreateInfos = queueCreateInfos.data();
		createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
		createInfo.pEnabledFeatures = &deviceFeatures;
		createInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
		createInfo.ppEnabledExtensionNames = deviceExtensions.data();

		if (enableValidationLayers) {
			createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
			createInfo.ppEnabledLayerNames = validationLayers.data();
		}
		else {
			createInfo.enabledLayerCount = 0;
		}

		if (vkCreateDevice(m_physicalDevice, &createInfo, nullptr, &m_logicalDevice) != VK_SUCCESS) {
			throw std::runtime_error("ERROR: Failed to create logical device!");
		}

		//Handles for created queue
		vkGetDeviceQueue(m_logicalDevice, indices.graphicsFamily, 0, &m_graphicsQueue);
		vkGetDeviceQueue(m_logicalDevice, indices.presentFamily, 0, &m_presentQueue);
	}

	/*
	Creates swapchain based on the info from SwapChainSupportDetails
	*/
	void createSwapchain() {
		SwapchainSupportDetails swapchainDetails = querySwapchainSupport(m_physicalDevice);

		VkSurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(swapchainDetails.formats);
		VkPresentModeKHR presentMode = chooseSwapPresentMode(swapchainDetails.presentModes);
		VkExtent2D extent = chooseSwapExtent(swapchainDetails.capabilities);

		//Actual number of images in the queue, maxImageCount = 0 is no limit beside memory req.
		uint32_t imageCount = swapchainDetails.capabilities.minImageCount + 1;
		if (swapchainDetails.capabilities.maxImageCount > 0 && imageCount > swapchainDetails.capabilities.maxImageCount) {
			imageCount = swapchainDetails.capabilities.maxImageCount;
		}

		VkSwapchainCreateInfoKHR createInfo = {};
		createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
		createInfo.surface = m_surface; //which surface swapchain is tied to
		createInfo.minImageCount = imageCount;
		createInfo.imageFormat = surfaceFormat.format;
		createInfo.imageColorSpace = surfaceFormat.colorSpace;
		createInfo.imageExtent = extent;
		createInfo.imageArrayLayers = 1; //No. of layers for each image. Always 1 if not stereoscopic 3D program
		createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT; //Usage. In this case, directly draw to them

		/*
		Next is required to specify, how to handle images shared across multiple
		queue families eg. if the graphics queue is different from the presentation queue
		Two ways to handle shared images:
			VK_SHARING_MODE_EXCLUSIVE:
									An image is owned by one queue family at a time and ownership must be
									explicitly transfered before using it in another queue family.
									This option offers the best performance.
			VK_SHARING_MODE_CONCURRENT:
									Images can be used across multiple queue families without explicit
									ownership transfers.
		*/
		QueueFamilyIndices indices = findQueueFamilies(m_physicalDevice);
		uint32_t queueFamilyIndices[] = { (uint32_t)indices.graphicsFamily, (uint32_t)indices.presentFamily };

		if (indices.graphicsFamily != indices.presentFamily) {
			createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
			createInfo.queueFamilyIndexCount = 2;
			createInfo.pQueueFamilyIndices = queueFamilyIndices;
		}
		else {
			createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
			createInfo.queueFamilyIndexCount = 0; // Optional
			createInfo.pQueueFamilyIndices = nullptr; // Optional
		}
		createInfo.preTransform = swapchainDetails.capabilities.currentTransform; //Transf. of images like 90 rotation
		createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
		createInfo.presentMode = presentMode;
		createInfo.clipped = VK_TRUE; //True means we dont care for colour of obstructed pixels by eg. another window
		createInfo.oldSwapchain = VK_NULL_HANDLE; //If the swapchain is invalidated and recreated, give ref. to previous one

		if (vkCreateSwapchainKHR(m_logicalDevice, &createInfo, nullptr, &m_swapchain) != VK_SUCCESS) {
			throw std::runtime_error("ERROR: Failed to create swap chain!");
		}

		/*
		Retrieve handles to images in swapchain
		*/
		vkGetSwapchainImagesKHR(m_logicalDevice, m_swapchain, &imageCount, nullptr);
		m_swapchainImages.resize(imageCount);
		vkGetSwapchainImagesKHR(m_logicalDevice, m_swapchain, &imageCount, m_swapchainImages.data());

		//Saved the format and extent
		m_swapchainImageFormat = surfaceFormat.format;
		m_swapchainExtent = extent;
	}

	void cleanupSwapchain() {
		for (size_t i = 0; i < m_swapchainFramebuffers.size(); i++) {
			vkDestroyFramebuffer(m_logicalDevice, m_swapchainFramebuffers[i], nullptr);
		}

		vkFreeCommandBuffers(m_logicalDevice, m_commandPool, static_cast<uint32_t>(m_commandBuffers.size()), m_commandBuffers.data());

		vkDestroyPipeline(m_logicalDevice, m_graphicsPipeline, nullptr);
		vkDestroyPipelineLayout(m_logicalDevice, m_pipelineLayout, nullptr);
		vkDestroyRenderPass(m_logicalDevice, m_renderPass, nullptr);

		for (size_t i = 0; i < m_swapchainImageViews.size(); i++) {
			vkDestroyImageView(m_logicalDevice, m_swapchainImageViews[i], nullptr);
		}

		vkDestroySwapchainKHR(m_logicalDevice, m_swapchain, nullptr);
	}

	/*
	Creates image views to access images in swapchain to use them
	as color targets
	*/
	void createImageViews() {
		m_swapchainImageViews.resize(m_swapchainImages.size());

		for (size_t i = 0; i < m_swapchainImageViews.size(); i++) {
			VkImageViewCreateInfo createInfo = {};
			createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
			createInfo.image = m_swapchainImages[i];
			createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
			createInfo.format = m_swapchainImageFormat;
			createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
			createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
			createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
			createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
			createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			createInfo.subresourceRange.baseMipLevel = 0; //We want this image as color target so no mipmapping
			createInfo.subresourceRange.levelCount = 1;
			createInfo.subresourceRange.baseArrayLayer = 0;
			createInfo.subresourceRange.layerCount = 1;

			if (vkCreateImageView(m_logicalDevice, &createInfo, nullptr, &m_swapchainImageViews[i]) != VK_SUCCESS) {
				throw std::runtime_error("ERROR: Failed to create imageview for swapchain!");
			}
		}
	}

	/*
	Specifies framebuffer attachments used for rendring.
	How many color, depth buffers will be used, how many
	samples are used for each of them and how they should be
	handled throughout the operations
	*/
	void createRenderPass() {
		VkAttachmentDescription colorAttachment = {};
		colorAttachment.format = m_swapchainImageFormat;
		colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT; //multisampling
		/*
		loadOp and storeOp determine what to do with the data in the attachment
		before and after rendering:
		VK_ATTACHMENT_LOAD_OP_LOAD: Preserve the existing contents of the attachment
		VK_ATTACHMENT_LOAD_OP_CLEAR: Clear the values to a constant at the start
		VK_ATTACHMENT_LOAD_OP_DONT_CARE: Existing contents are undefined; we don't care about them

		VK_ATTACHMENT_STORE_OP_STORE: Rendered contents will be stored in memory and can be read later
		VK_ATTACHMENT_STORE_OP_DONT_CARE: Contents of the framebuffer will be undefined after the rendering operation
		*/
		colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		/*
		Sets layout of pixels in memory. We first dont care for how pixels
		are stored since we will clear them to black and after renderpass
		finishes, we want to transition into finalLayout
		*/
		colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

		/*
		Subpasses
		The index of the attachment in this array is directly referenced from
		the fragment shader with the layout(location = 0) out vec4 outColor directive
		*/
		VkAttachmentReference colorAttachmentRef = {};
		colorAttachmentRef.attachment = 0; //We have just one attachment description
		colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		VkSubpassDescription subpass = {};
		subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpass.colorAttachmentCount = 1;
		subpass.pColorAttachments = &colorAttachmentRef;

		/*
		Subpass dependencies
		We could change the waitStages for the imageAvailableSemaphore to VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT
		to ensure that the render passes don't begin until the image is available
		or this...
		*/
		VkSubpassDependency dependency = {};
		dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
		dependency.dstSubpass = 0;
		dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dependency.srcAccessMask = 0;
		dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

		VkRenderPassCreateInfo createInfo = {};
		createInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		createInfo.attachmentCount = 1;
		createInfo.pAttachments = &colorAttachment;
		createInfo.subpassCount = 1;
		createInfo.pSubpasses = &subpass;
		createInfo.dependencyCount = 1;
		createInfo.pDependencies = &dependency;

		if (vkCreateRenderPass(m_logicalDevice, &createInfo, nullptr, &m_renderPass) != VK_SUCCESS) {
			throw std::runtime_error("ERROR: Failed to create render pass!");
		}
	}

	/*
	Creates a shader module which sort of works like a shader bytecode wrapper
	*/
	VkShaderModule createShaderModule(const std::vector<char>& code) {
		VkShaderModuleCreateInfo createInfo = {};
		
		createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
		createInfo.codeSize = code.size();
		createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());

		VkShaderModule shaderModule;
		if (vkCreateShaderModule(m_logicalDevice, &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
			throw std::runtime_error("ERROR: Failed to create shader module!");
		}

		return shaderModule;
	}

	void createDescriptorSetLayout() {
		VkDescriptorSetLayoutBinding uboLayoutBinding = {};
		uboLayoutBinding.binding = 0;
		uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		uboLayoutBinding.descriptorCount = 1;
		uboLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
		uboLayoutBinding.pImmutableSamplers = nullptr; //optional

		VkDescriptorSetLayoutCreateInfo layoutInfo = {};
		layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		layoutInfo.bindingCount = 1;
		layoutInfo.pBindings = &uboLayoutBinding;

		if (vkCreateDescriptorSetLayout(m_logicalDevice, &layoutInfo, nullptr, &m_descriptorSetLayout)) {
			throw std::runtime_error("ERROR: Failed to create descriptor set layout!");
		}
	}

	void createGraphicsPipeline() {
		/*
		Programmable part
		*/
		auto vertShaderCode = readFile("shaders/triangle.vert.spv");
		auto fragShaderCode = readFile("shaders/triangle.frag.spv");

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

		/**
		Fixed pipeline part
		**/

		/*
		This part describes the format of the vertex data that will be passed to the vertex shader
		Bindings:
			spacing between data and whether data is per-vertex or per-instance
		Attribute descriptions:
			type of attributes passed into the vertex shader, binding and offset
		*/
		auto bindingDescription = Vertex::getBindingDescription();
		auto attributeDescriptions = Vertex::getAttributeDescriptions();
		VkPipelineVertexInputStateCreateInfo vertexInputInfo = {};
		vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
		vertexInputInfo.vertexBindingDescriptionCount = 1;
		vertexInputInfo.pVertexBindingDescriptions = &bindingDescription; // Optional, can be nullptr
		vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
		vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data(); // Optional, can be nullptr

		/*
		Input assembly describes, what kind of geometry will be drawn and if primitive restart is enables.
		VK_PRIMITIVE_TOPOLOGY_POINT_LIST: points from vertices
		VK_PRIMITIVE_TOPOLOGY_LINE_LIST: line from every 2 vertices without reuse
		VK_PRIMITIVE_TOPOLOGY_LINE_STRIP: the end vertex of every line is used as start vertex for the next line
		VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST: triangle from every 3 vertices without reuse
		VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP: the second and third vertex of every triangle are used as first two
		*/
		VkPipelineInputAssemblyStateCreateInfo inputAssembly = {};
		inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
		inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
		inputAssembly.primitiveRestartEnable = VK_FALSE;

		/*
		Viewport and scissors
		*/
		VkViewport viewport = {};
		viewport.x = 0.0f;
		viewport.y = 0.0f;
		viewport.width = (float)m_swapchainExtent.width;
		viewport.height = (float)m_swapchainExtent.height;
		viewport.minDepth = 0.0f;
		viewport.maxDepth = 1.0f;

		VkRect2D scissor = {};
		scissor.offset = { 0, 0 };
		scissor.extent = m_swapchainExtent;

		VkPipelineViewportStateCreateInfo viewportState = {};
		viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
		viewportState.viewportCount = 1;
		viewportState.pViewports = &viewport;
		viewportState.scissorCount = 1;
		viewportState.pScissors = &scissor;

		/*
		Rasterizer performs depth testing and backface culling.
		Can be configured to fill polygons or just output wireframes
		*/
		VkPipelineRasterizationStateCreateInfo rasterizer = {};
		rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
		rasterizer.depthClampEnable = VK_FALSE; //Clamps instead of discards faces, req. device feature
		rasterizer.rasterizerDiscardEnable = VK_FALSE; //If true, geometry never passes through rasterizer -> disables output to framebuffer
		rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
		rasterizer.lineWidth = 1.0f;
		rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
		rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;
		rasterizer.depthBiasEnable = VK_FALSE;

		/*
		Multisampling settings
		*/
		VkPipelineMultisampleStateCreateInfo multisampling = {};
		multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
		multisampling.sampleShadingEnable = VK_FALSE;
		multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

		/*
		Depth and stencil configuration
		*/

		/*
		Color blending
		There are two types of structs to config. blending:
		VkPipelineColorBlendAttachmentState contains the configuration per attached framebuffer
		VkPipelineColorBlendStateCreateInfo contains the global color blending settings
		*/
		VkPipelineColorBlendAttachmentState colorBlendAttachment = {};
		colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
		colorBlendAttachment.blendEnable = VK_FALSE;

		VkPipelineColorBlendStateCreateInfo colorBlending = {};
		colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
		colorBlending.logicOpEnable = VK_FALSE;
		colorBlending.attachmentCount = 1;
		colorBlending.pAttachments = &colorBlendAttachment;

		/*
		It is also possible to change some parts of the dynamic state of the pipeline here
		*/
		/*
		VkDynamicState dynamicStates[] = {
			VK_DYNAMIC_STATE_VIEWPORT,
			VK_DYNAMIC_STATE_LINE_WIDTH
		};

		VkPipelineDynamicStateCreateInfo dynamicState = {};
		dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
		dynamicState.dynamicStateCount = 2;
		dynamicState.pDynamicStates = dynamicStates;
		*/

		/*
		Pipeline layout for passing uniforms into shaders: required even if there are none in shaders
		*/
		VkPipelineLayoutCreateInfo pipelineLayoutInfo = {};
		pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		pipelineLayoutInfo.setLayoutCount = 1;
		pipelineLayoutInfo.pSetLayouts = &m_descriptorSetLayout;
		pipelineLayoutInfo.pushConstantRangeCount = 0;
		pipelineLayoutInfo.pPushConstantRanges = 0;

		if (vkCreatePipelineLayout(m_logicalDevice, &pipelineLayoutInfo, nullptr, &m_pipelineLayout) != VK_SUCCESS) {
			throw std::runtime_error("ERROR: Failed to create pipeline layout!");
		}


		VkGraphicsPipelineCreateInfo pipelineCreateInfo = {};
		pipelineCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
		pipelineCreateInfo.stageCount = 2;
		pipelineCreateInfo.pStages = shaderStages;
		pipelineCreateInfo.pVertexInputState = &vertexInputInfo;
		pipelineCreateInfo.pInputAssemblyState = &inputAssembly;
		pipelineCreateInfo.pViewportState = &viewportState;
		pipelineCreateInfo.pRasterizationState = &rasterizer;
		pipelineCreateInfo.pMultisampleState = &multisampling;
		pipelineCreateInfo.pColorBlendState = &colorBlending;
		pipelineCreateInfo.layout = m_pipelineLayout;
		pipelineCreateInfo.renderPass = m_renderPass;
		pipelineCreateInfo.subpass = 0;
		pipelineCreateInfo.basePipelineHandle = VK_NULL_HANDLE; //Handle for derived pipeline
		pipelineCreateInfo.basePipelineIndex = -1;

		if (vkCreateGraphicsPipelines(m_logicalDevice, VK_NULL_HANDLE, 1, &pipelineCreateInfo, nullptr, &m_graphicsPipeline) != VK_SUCCESS) {
			throw std::runtime_error("ERROR: Failed to create graphics pipeline!");
		}
		/*
		Cleanup of the "bytecode wrappers"
		*/
		vkDestroyShaderModule(m_logicalDevice, vertShaderModule, nullptr);
		vkDestroyShaderModule(m_logicalDevice, fragShaderModule, nullptr);
	}

	/*
	Create framebuffers for all imageviews compatible with renderpass
	*/
	void createFramebuffers() {
		m_swapchainFramebuffers.resize(m_swapchainImageViews.size());
		
		for (size_t i = 0; i < m_swapchainImageViews.size(); i++) {
			VkImageView attachments[]{ m_swapchainImageViews[i] };

			VkFramebufferCreateInfo createInfo = {};
			createInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
			createInfo.renderPass = m_renderPass;
			createInfo.attachmentCount = 1;
			createInfo.pAttachments = attachments;
			createInfo.width = m_swapchainExtent.width;
			createInfo.height = m_swapchainExtent.height;
			createInfo.layers = 1;

			if (vkCreateFramebuffer(m_logicalDevice, &createInfo, nullptr, &m_swapchainFramebuffers[i]) != VK_SUCCESS) {
				throw std::runtime_error("ERROR: Failed to create swapchain framebuffer!");
			}
		}
	}

	/*
	Creates command pool attached to one queue family
	*/
	void createCommandPool() {
		QueueFamilyIndices queueFamilies = findQueueFamilies(m_physicalDevice);

		VkCommandPoolCreateInfo createInfo = {};
		createInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		createInfo.queueFamilyIndex = queueFamilies.graphicsFamily;

		if (vkCreateCommandPool(m_logicalDevice, &createInfo, nullptr, &m_commandPool) != VK_SUCCESS) {
			throw std::runtime_error("ERROR: Failed to create command pool!");
		}
	}

	/*
	Creates command buffers and records commands
	*/
	void createCommandBuffers() {
		m_commandBuffers.resize(m_swapchainImageViews.size());

		VkCommandBufferAllocateInfo allocInfo = {};
		allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		allocInfo.commandPool = m_commandPool;
		/*
		VK_COMMAND_BUFFER_LEVEL_PRIMARY: Can be submitted to a queue for execution, but cannot be called from other command buffers.
		VK_COMMAND_BUFFER_LEVEL_SECONDARY: Cannot be submitted directly, but can be called from primary command buffers.
		*/
		allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		allocInfo.commandBufferCount = (uint32_t)m_commandBuffers.size();

		if (vkAllocateCommandBuffers(m_logicalDevice, &allocInfo, m_commandBuffers.data()) != VK_SUCCESS) {
			throw std::runtime_error("ERROR: Failed to allocate command buffers!");
		}

		for (size_t i = 0; i < m_commandBuffers.size(); i++) {
			VkCommandBufferBeginInfo beginInfo = {};
			beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
			beginInfo.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;
			beginInfo.pInheritanceInfo = nullptr;

			vkBeginCommandBuffer(m_commandBuffers[i], &beginInfo);

			VkRenderPassBeginInfo renderPassInfo = {};
			renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
			renderPassInfo.renderPass = m_renderPass;
			renderPassInfo.framebuffer = m_swapchainFramebuffers[i];
			//render area defines where shader loads and stores will take place, should match size of attachments
			renderPassInfo.renderArea.offset = { 0, 0 };
			renderPassInfo.renderArea.extent = m_swapchainExtent;
			
			VkClearValue clearColor = { 0.2f, 0.3f, 0.3f, 1.0f };
			renderPassInfo.pClearValues = &clearColor;
			renderPassInfo.clearValueCount = 1;

			/*
			Begin recording commands
			*/
			vkCmdBeginRenderPass(m_commandBuffers[i], &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

			vkCmdBindPipeline(m_commandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, m_graphicsPipeline);
			/*
			vertexCount: Even though we don't have a vertex buffer, we technically still have 3 vertices to draw.
			instanceCount: Used for instanced rendering, use 1 if you're not doing that.
			firstVertex: Used as an offset into the vertex buffer, defines the lowest value of gl_VertexIndex.
			firstInstance: Used as an offset for instanced rendering, defines the lowest value of gl_InstanceIndex.
			*/
			VkBuffer vertexBuffers[] = { m_vertexBuffer };
			VkDeviceSize offsets[] = { 0 };
			vkCmdBindVertexBuffers(m_commandBuffers[i], 0, 1, vertexBuffers, offsets);
			vkCmdBindIndexBuffer(m_commandBuffers[i], m_indexBuffer, 0, VK_INDEX_TYPE_UINT16);
			vkCmdBindDescriptorSets(m_commandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayout, 0, 1, &m_descriptorSet, 0, nullptr);

			//vkCmdDraw(m_commandBuffers[i], static_cast<uint32_t>(vertices.size()), 1, 0, 0);
			vkCmdDrawIndexed(m_commandBuffers[i], static_cast<uint32_t>(indices.size()), 1, 0, 0, 0);

			vkCmdEndRenderPass(m_commandBuffers[i]);

			if (vkEndCommandBuffer(m_commandBuffers[i]) != VK_SUCCESS) {
				throw std::runtime_error("ERROR: Failed to record command buffer!");
			}
		}
	}

	void createSemaphores() {
		VkSemaphoreCreateInfo createInfo = {};
		createInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

		if (vkCreateSemaphore(m_logicalDevice, &createInfo, nullptr, &m_imageAvailableSemaphore) != VK_SUCCESS ||
			vkCreateSemaphore(m_logicalDevice, &createInfo, nullptr, &m_renderFinishedSemaphore) != VK_SUCCESS) {
			throw std::runtime_error("ERROR: Failed to create semaphores!");
		}
	}

	void drawFrame() {
		/*
		Here should be update for program state
		updateState()
		*/
		vkQueueWaitIdle(m_presentQueue); //wait for presentation to finish before drawing again

		uint32_t imageIndex;
		VkResult result = vkAcquireNextImageKHR(m_logicalDevice,
			m_swapchain,
			std::numeric_limits<uint64_t>::max(), //disables timeout
			m_imageAvailableSemaphore,
			VK_NULL_HANDLE,
			&imageIndex);
		
		if (result == VK_ERROR_OUT_OF_DATE_KHR) {
			recreateSwapchain();
			return;
		}
		else if(result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
			throw std::runtime_error("Failed to acquire swap chain image!");
		}
		VkSubmitInfo submitInfo = {};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

		/*
		We want to wait with writing colors into framebuffer until it's ready
		Theoretically we can start executing vertex stage and such before imag is available
		*/
		VkSemaphore waitSemaphore[] = { m_imageAvailableSemaphore };
		VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
		submitInfo.waitSemaphoreCount = 1;
		submitInfo.pWaitSemaphores = waitSemaphore;
		submitInfo.pWaitDstStageMask = waitStages;
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &m_commandBuffers[imageIndex];

		VkSemaphore signalSemaphores[] = { m_renderFinishedSemaphore };
		submitInfo.signalSemaphoreCount = 1;
		submitInfo.pSignalSemaphores = signalSemaphores;

		if (vkQueueSubmit(m_graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE) != VK_SUCCESS) {
			throw std::runtime_error("ERROR: Failed to submit draw command buffer!");
		}

		VkPresentInfoKHR presentInfo = {};
		presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
		presentInfo.waitSemaphoreCount = 1;
		presentInfo.pWaitSemaphores = signalSemaphores;

		VkSwapchainKHR swapchains[] = { m_swapchain };
		presentInfo.swapchainCount = 1;
		presentInfo.pSwapchains = swapchains;
		presentInfo.pImageIndices = &imageIndex;

		result = vkQueuePresentKHR(m_presentQueue, &presentInfo);

		if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
			recreateSwapchain();
		}
		else if (result != VK_SUCCESS) {
			throw std::runtime_error("ERROR: Failed to present swap chain image!");
		}
	}

	uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) {
		VkPhysicalDeviceMemoryProperties memProperties;
		vkGetPhysicalDeviceMemoryProperties(m_physicalDevice, &memProperties);

		/*
		If there is a memory type suitable for the buffer that also has
		all of the properties we need, then we return its index
		*/
		for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
			if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
				return i;
			}
		}

		throw std::runtime_error("failed to find suitable memory type!");
	}

	void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory) {
		VkBufferCreateInfo bufferInfo = {};
		bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		bufferInfo.size = size;
		bufferInfo.usage = usage;
		bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		//bufferInfo.flags = 0;

		if (vkCreateBuffer(m_logicalDevice, &bufferInfo, nullptr, &buffer) != VK_SUCCESS) {
			throw std::runtime_error("ERROR: failed to create vertex buffer!");
		}

		//Buffer is created, now we need to allocate memory for it
		VkMemoryRequirements memRequirements;
		vkGetBufferMemoryRequirements(m_logicalDevice, buffer, &memRequirements);

		VkMemoryAllocateInfo allocInfo = {};
		allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		allocInfo.allocationSize = memRequirements.size;
		allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties);

		if (vkAllocateMemory(m_logicalDevice, &allocInfo, nullptr, &bufferMemory) != VK_SUCCESS) {
			throw std::runtime_error("failed to allocate vertex buffer memory!");
		}

		//If allocation was succ., we can bind it to buffer
		vkBindBufferMemory(m_logicalDevice, buffer, bufferMemory, 0); //0 - offset in memory
	}

	void createVertexBuffer() {
		VkDeviceSize bufferSize = sizeof(vertices[0]) * vertices.size();

		VkBuffer stagingBuffer;
		VkDeviceMemory stagingBufferMemory;
		createBuffer(
			bufferSize,
			VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			stagingBuffer,
			stagingBufferMemory);

		//Copy vertex data into buffer
		void* data;
		vkMapMemory(m_logicalDevice, stagingBufferMemory, 0, bufferSize, 0, &data);
		memcpy(data, vertices.data(), (size_t)bufferSize);
		vkUnmapMemory(m_logicalDevice, stagingBufferMemory);

		createBuffer(
			bufferSize,
			VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
			m_vertexBuffer,
			m_vertexBufferMemory);
		/*
		It may happen that memory might not be copied when unmapping memory region -> solutions:
			Use a memory heap that is host coherent, indicated with VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
			Call vkFlushMappedMemoryRanges to after writing to the mapped memory,
			and call vkInvalidateMappedMemoryRanges before reading from the mapped memory
		*/

		copyBufferData(stagingBuffer, m_vertexBuffer, bufferSize);

		vkDestroyBuffer(m_logicalDevice, stagingBuffer, nullptr);
		vkFreeMemory(m_logicalDevice, stagingBufferMemory, nullptr);
	}

	void createIndexBuffer() {
		VkDeviceSize bufferSize = sizeof(indices[0]) * indices.size();

		VkBuffer stagingBuffer;
		VkDeviceMemory stagingBufferMemory;
		createBuffer(
			bufferSize,
			VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			stagingBuffer,
			stagingBufferMemory
		);

		void* data;
		vkMapMemory(m_logicalDevice, stagingBufferMemory, 0, bufferSize, 0, &data);
		memcpy(data, indices.data(), (size_t)bufferSize);
		vkUnmapMemory(m_logicalDevice, stagingBufferMemory);

		createBuffer(
			bufferSize,
			VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
			m_indexBuffer,
			m_indexBufferMemory
		);

		copyBufferData(stagingBuffer, m_indexBuffer, bufferSize);

		vkDestroyBuffer(m_logicalDevice, stagingBuffer, nullptr);
		vkFreeMemory(m_logicalDevice, stagingBufferMemory, nullptr);
	}

	void createUniformBuffer() {
		VkDeviceSize bufferSize = sizeof(UniformBufferObject);
		createBuffer(
			bufferSize,
			VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			m_uniformBuffer,
			m_uniformBufferMemory);
	}

	void copyBufferData(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size) {
		//You can create separate command pool for these buffers -> may apply memory optimization
		VkCommandBufferAllocateInfo allocInfo = {};
		allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		allocInfo.commandPool = m_commandPool;
		allocInfo.commandBufferCount = 1;

		VkCommandBuffer commandBuffer;
		vkAllocateCommandBuffers(m_logicalDevice, &allocInfo, &commandBuffer);

		//Start recording command buffer
		VkCommandBufferBeginInfo beginInfo = {};
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

		vkBeginCommandBuffer(commandBuffer, &beginInfo);
			VkBufferCopy copyRegion = {};
			copyRegion.srcOffset = 0; //optional
			copyRegion.dstOffset = 0; //optional
			copyRegion.size = size;
			vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);
		vkEndCommandBuffer(commandBuffer);

		//Submit command
		VkSubmitInfo submitInfo = {};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &commandBuffer;

		vkQueueSubmit(m_graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
		vkQueueWaitIdle(m_graphicsQueue); 
		/*
		here we wait for queue to be idle, it is possible to wait for
		fences, which could be used for multiple transfer simultaneously
		*/
		vkFreeCommandBuffers(m_logicalDevice, m_commandPool, 1, &commandBuffer);
	}
 
	void updateUniformData() {
		static auto timeStart = std::chrono::high_resolution_clock::now();

		auto timeCurrent = std::chrono::high_resolution_clock::now();
		float time = std::chrono::duration<float, std::chrono::seconds::period>(timeCurrent - timeStart).count();

		UniformBufferObject ubo = {};
		//ubo.model = glm::rotate(glm::mat4(1.0f), time * glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f));
		//ubo.view = glm::lookAt(glm::vec3(0.0f, 0.0f, 2.0f), glm::vec3(0.0f), glm::vec3(0.0f, -1.0f, 0.0f));
		//ubo.projection = glm::perspective(glm::radians(90.0f), m_swapchainExtent.width / (float) m_swapchainExtent.height, 0.1f, 1000.0f);

		void* data;
		vkMapMemory(m_logicalDevice, m_uniformBufferMemory, 0, sizeof(ubo), 0, &data);
			memcpy(data, &ubo, sizeof(ubo));
		vkUnmapMemory(m_logicalDevice, m_uniformBufferMemory);
	}

	/*
	Descriptor sets must be created, like command buffers, from Descriptor pools
	*/
	void createDescriptorPool() {
		/*
		First specify descriptor types which descriptor set will contain
		*/
		VkDescriptorPoolSize poolSize = {};
		poolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		poolSize.descriptorCount = 1;

		VkDescriptorPoolCreateInfo poolInfo = {};
		poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		poolInfo.poolSizeCount = 1;
		poolInfo.pPoolSizes = &poolSize;
		poolInfo.maxSets = 1;

		if (vkCreateDescriptorPool(m_logicalDevice, &poolInfo, nullptr, &m_descriptorPool) != VK_SUCCESS) {
			throw std::runtime_error("ERROR: Failed to create descriptor set pool!");
		}
	}

	void createDescriptorSet() {
		VkDescriptorSetLayout layouts[] = {m_descriptorSetLayout};
		VkDescriptorSetAllocateInfo allocInfo = {};
		allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		allocInfo.descriptorPool = m_descriptorPool;
		allocInfo.descriptorSetCount = 1;
		allocInfo.pSetLayouts = layouts;

		//No need for cleanup, freed with descriptor pool
		if (vkAllocateDescriptorSets(m_logicalDevice, &allocInfo, &m_descriptorSet) != VK_SUCCESS) {
			throw std::runtime_error("ERROR: Failed to allocate descriptor set!");
		}

		/*
		Descriptor set is now allocated but descriptors within still need to be configured
		*/
		VkDescriptorBufferInfo bufferInfo = {};
		bufferInfo.buffer = m_uniformBuffer;
		bufferInfo.offset = 0;
		bufferInfo.range = sizeof(UniformBufferObject);

		VkWriteDescriptorSet descriptorWrite = {};
		descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		descriptorWrite.dstSet = m_descriptorSet;
		descriptorWrite.dstBinding = 0;
		descriptorWrite.dstArrayElement = 0;
		descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		descriptorWrite.descriptorCount = 1; //How many and what types of array elements to update
		descriptorWrite.pBufferInfo = &bufferInfo;

		vkUpdateDescriptorSets(m_logicalDevice, 1, &descriptorWrite, 0, nullptr);
	}

/*
Members
*/
public:
private:
	GLFWwindow*						m_window;
	VkInstance						m_instance;
	VkSurfaceKHR					m_surface;
	VkDebugReportCallbackEXT		m_debugCallback;
	VkPhysicalDevice				m_physicalDevice = VK_NULL_HANDLE; //Destroyed when instance is destroyed
	VkDevice						m_logicalDevice;
	VkQueue							m_graphicsQueue;
	VkQueue							m_presentQueue;
	VkSwapchainKHR					m_swapchain;
	std::vector<VkImage>			m_swapchainImages;
	VkFormat						m_swapchainImageFormat;
	VkExtent2D						m_swapchainExtent;
	std::vector<VkImageView>		m_swapchainImageViews;
	VkRenderPass					m_renderPass;
	VkDescriptorSetLayout			m_descriptorSetLayout;
	VkDescriptorPool				m_descriptorPool;
	VkDescriptorSet					m_descriptorSet;
	VkPipelineLayout				m_pipelineLayout;
	VkPipeline						m_graphicsPipeline;
	std::vector<VkFramebuffer>		m_swapchainFramebuffers;
	VkCommandPool					m_commandPool;
	VkBuffer						m_vertexBuffer;
	VkDeviceMemory					m_vertexBufferMemory;
	VkBuffer						m_indexBuffer;
	VkDeviceMemory					m_indexBufferMemory;
	VkBuffer						m_uniformBuffer;
	VkDeviceMemory					m_uniformBufferMemory;
	std::vector<VkCommandBuffer>	m_commandBuffers;
	VkSemaphore						m_imageAvailableSemaphore;
	VkSemaphore						m_renderFinishedSemaphore;
};


int main(int argc, char* argv[]) {
	HelloTriangleApplication app;
	try {
		app.run();
	}
	catch (const std::runtime_error& e) {
		std::cerr << e.what() << std::endl;
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}