#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <iostream>
#include <stdexcept>
#include <functional>
#include <vector>
#include <set>
#include <algorithm>
#include <fstream>

#ifdef NDEBUG
const bool enableValidationLayers = false;
#else
const bool enableValidationLayers = true;
#endif

const unsigned int WIDTH = 800;
const unsigned int HEIGHT = 600;

/*
Totalne ukradene proxy funkce na ziskani a niceni extension funkce pro debug vypisy
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
	"VK_LAYER_LUNARG_standard_validation"
};

/*
Required device extensions
*/
const std::vector<const char*> deviceExtensions = {
	VK_KHR_SWAPCHAIN_EXTENSION_NAME
};

/*
Struktura pro ulozeni indexu jednotlivych queue families.
isComplete overi, jestli jsou pritomny vsechny hledane families
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

void keyCallback(GLFWwindow *pWindow, int key, int scancode, int action, int mods) {
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

private:
	void initWindow() {
		glfwInit();

		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
		glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

		m_window = glfwCreateWindow(WIDTH, HEIGHT, "Vulkan Triangle", nullptr, nullptr);
		if (!m_window) {
			glfwTerminate();
			throw std::runtime_error("ERROR: Failed to create GLFW window!");
		}

		glfwSetKeyCallback(m_window, keyCallback);
	}

	void initVulkan() {
		createInstance();
		setupDebugCallback();
		createSurface();
		selectPhysicalDevice();
		createLogicalDevice();
		createSwapchain();
		createImageViews();
		createGraphicsPipeline();
	}

	void mainLoop() {
		while (!glfwWindowShouldClose(m_window)) {
			glfwPollEvents();

			//glfwSwapBuffers(m_window);
		}
	}

	void cleanup() {
		//Vulkan cleanup
		vkDestroyPipelineLayout(m_logicalDevice, m_pipelineLayout, nullptr);
		for (auto imageview : m_swapchainImageViews) {
			vkDestroyImageView(m_logicalDevice, imageview, nullptr);
		}
		vkDestroySwapchainKHR(m_logicalDevice, m_swapchain, nullptr);
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
		Kontrola na podporu zadanych validacnich vrstev pred samotnym vytvorenim
		*/
		if (enableValidationLayers && !checkValidationLayerSupport()) {
			throw std::runtime_error("ERROR:Validation layers requested, but not available!");
		}

		/*
		Technicky nepovinna struktura popisujici program, kterou verzi Vulkanu pouziva, jaky engine atd.
		*/
		VkApplicationInfo programInfo = {};
		programInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
		programInfo.pApplicationName = "Vulkan Triangle";
		programInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
		programInfo.pEngineName = "No Engine";
		programInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
		programInfo.apiVersion = VK_API_VERSION_1_0;

		/*
		Tato struktura uz nepovinna neni a rika Vulkan ovladaci, jake extensiony/validacni vrstvy pouzit
		a pro ktera zarizeni (nebo pro cely program)
		*/
		VkInstanceCreateInfo createInfo = {};
		createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
		createInfo.pApplicationInfo = &programInfo;

		/*
		Ziskani rozsireni z window systemu (Vulkan je platform agnostic)
		*/
		uint32_t glfwExtensionCount = 0;
		const char** glfwExtensions;
		glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

		auto extensions = getRequiredExtensions();
		createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
		createInfo.ppEnabledExtensionNames = extensions.data();

		createInfo.enabledLayerCount = 0;

		/*
		Pridani validacnich vrstev do instance
		*/
		if (enableValidationLayers) {
			createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
			createInfo.ppEnabledLayerNames = validationLayers.data();
		}
		else {
			createInfo.enabledLayerCount = 0;
		}

		/*
		Vytvoreni samotne Vulkan instance
		*/
		VkResult result = vkCreateInstance(&createInfo, nullptr, &m_instance);
		if (vkCreateInstance(&createInfo, nullptr, &m_instance) != VK_SUCCESS) {
			throw std::runtime_error("ERROR:Failed to create instance!");
		}
	}

	bool checkValidationLayerSupport() {
		/*
		Nejprve je potreba ziskat veskere validacni vrstvy
		-> stejne pouziti jako u VkEnumerateInstanceExtensionProperties u VkInstance
		*/
		uint32_t layerCount = 0;
		vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

		std::vector<VkLayerProperties> availableLayers(layerCount);
		vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

		/*
		Nyni se overi, jestli veskere zadane val. vrstvy existuji
		v podporovanych vrstvach
		*/
		for (const char* layerName : validationLayers) {
			bool layerFound = false;

			for (const auto& layerProperties : availableLayers) {
				if (strcmp(layerName, layerProperties.layerName) == 0) {
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
		Opet, jako s kazdym Vulkan objektem ,je potreba naplnit info struct
		flags urcuji, ktere zpravy chceme ziskavat
		pfncallback urcuje callback funkci
		Da se pouzit i pUserData parametr pro predani do callback funkce
		ziskatelni pres userData argument (naprikald poslat si cely program objekt)
		*/
		VkDebugReportCallbackCreateInfoEXT createInfo = {};
		createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT;
		createInfo.flags = VK_DEBUG_REPORT_ERROR_BIT_EXT | VK_DEBUG_REPORT_WARNING_BIT_EXT;
		createInfo.pfnCallback = debugCallback;

		/*
		Protoze je debug callback je specificky pro Vulkan instanci a jejim vrstvam,
		je potreba ji zadat jako prvni
		*/
		if (CreateDebugReportCallbackEXT(m_instance, &createInfo, nullptr, &m_debugCallback) != VK_SUCCESS) {
			throw std::runtime_error("ERROR:Failed to set up debug callback!");
		}
	}

	/*
	VKAPI_ATTR a VKAPI_CALL zarucuji, ze maji spravnou Vulkan signaturu
	flags
		VK_DEBUG_REPORT_INFORMATION_BIT_EXT
		VK_DEBUG_REPORT_WARNING_BIT_EXT
		VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT
		VK_DEBUG_REPORT_ERROR_BIT_EXT
		VK_DEBUG_REPORT_DEBUG_BIT_EXT

	Vraci boolean: true pokud by se mela abortnout Vulkan call ktery
	vyvolal validacni vrstvu (vhodne pro testovani) jinak vzdy vracet VK_FALSE
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
	Vraci seznam rozsireni podle zadanych validacnich vrstev
	GLFW rozsireni jsou potreba vzdy ale DEBUG REPORT je pridan
	jen pokud jsou zapnute validacni vrstvy
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

	void createSurface() {
		if (glfwCreateWindowSurface(m_instance, m_window, nullptr, &m_surface) != VK_SUCCESS) {
			throw std::runtime_error("ERROR: Failed to create window surface!");
		}
	}

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
		Do indices se ulozi indexy jednotlivych hledanych queues
		*/
		int i = 0;
		for (const auto& queueFamily : queueFamilies) {
			//Hledani podpory pro grafickou frontu
			if (queueFamily.queueCount > 0 && queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
				indices.graphicsFamily = i;
			}

			/*
			Hledani podpory pro presentovani do surface -> je potreba hledat oddelene
			Dala by se dat podminka pro uprednostneni zarizeni, ktere podporuje jak kresleni
			tak presentation na stejne fronte pro lepsi performance
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
			VkExtent2D actualExtent = { WIDTH, HEIGHT };

			actualExtent.width = std::max(capabilities.minImageExtent.width, std::min(capabilities.maxImageExtent.width, actualExtent.width));
			actualExtent.height = std::max(capabilities.minImageExtent.height, std::min(capabilities.maxImageExtent.height, actualExtent.height));

			return actualExtent;
		}
	}


	void createLogicalDevice() {
		QueueFamilyIndices indices = findQueueFamilies(m_physicalDevice);

		/*
		Drivery zatim podporuji vytvoreni maleho poctu front
		ale neni moc potreba tvorit vice jak jednu -> command buffery se 
		vytvori na vice threadech a poslou naraz jednim volanim do
		main fronty s malym overheadem
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

		//Specifikace device feature ktere se budou pouzivat (napr. geometry shader)
		VkPhysicalDeviceFeatures deviceFeatures = {};

		/*
		Hlavni create info
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

		//Ziskani handlu na vytvorene fronty
		vkGetDeviceQueue(m_logicalDevice, indices.graphicsFamily, 0, &m_graphicsQueue);
		vkGetDeviceQueue(m_logicalDevice, indices.presentFamily, 0, &m_presentQueue);
	}

	/*
	Creating swapchain based on the info from SwapChainSupportDetails
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

	void createGraphicsPipeline() {
		/*
		Programmable part
		*/
		auto vertShaderCode = readFile("shaders/vert.spv");
		auto fragShaderCode = readFile("shaders/frag.spv");

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
		VkPipelineVertexInputStateCreateInfo vertexInputInfo = {};
		vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
		vertexInputInfo.vertexBindingDescriptionCount = 0;
		vertexInputInfo.pVertexBindingDescriptions = nullptr; // Optional
		vertexInputInfo.vertexAttributeDescriptionCount = 0;
		vertexInputInfo.pVertexAttributeDescriptions = nullptr; // Optional

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
		rasterizer.depthBiasConstantFactor = 0.0f; // Optional
		rasterizer.depthBiasClamp = 0.0f; // Optional
		rasterizer.depthBiasSlopeFactor = 0.0f; // Optional

		/*
		Multisampling settings
		*/
		VkPipelineMultisampleStateCreateInfo multisampling = {};
		multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
		multisampling.sampleShadingEnable = VK_FALSE;
		multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
		multisampling.minSampleShading = 1.0f; // Optional
		multisampling.pSampleMask = nullptr; // Optional
		multisampling.alphaToCoverageEnable = VK_FALSE; // Optional
		multisampling.alphaToOneEnable = VK_FALSE; // Optional

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
		pipelineLayoutInfo.setLayoutCount = 0;
		pipelineLayoutInfo.pSetLayouts = nullptr;
		pipelineLayoutInfo.pushConstantRangeCount = 0;
		pipelineLayoutInfo.pPushConstantRanges = 0;

		if (vkCreatePipelineLayout(m_logicalDevice, &pipelineLayoutInfo, nullptr, &m_pipelineLayout) != VK_SUCCESS) {
			throw std::runtime_error("ERROR: Failed to create pipeline layout!");
		}

		/*
		Cleanup of the "bytecode wrappers"
		*/
		vkDestroyShaderModule(m_logicalDevice, vertShaderModule, nullptr);
		vkDestroyShaderModule(m_logicalDevice, fragShaderModule, nullptr);
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
/*
Members
*/
public:
private:
	GLFWwindow*					m_window;
	VkInstance					m_instance;
	VkSurfaceKHR				m_surface;
	VkDebugReportCallbackEXT	m_debugCallback;
	VkPhysicalDevice			m_physicalDevice = VK_NULL_HANDLE; //Implicitne znicen pri niceni instance -> neni potreba uvolnit
	VkDevice					m_logicalDevice;
	VkQueue						m_graphicsQueue;
	VkQueue						m_presentQueue;
	VkSwapchainKHR				m_swapchain;
	std::vector<VkImage>		m_swapchainImages;
	VkFormat					m_swapchainImageFormat;
	VkExtent2D					m_swapchainExtent;
	std::vector<VkImageView>	m_swapchainImageViews;
	VkPipelineLayout			m_pipelineLayout;
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