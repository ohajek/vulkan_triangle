#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <iostream>
#include <stdexcept>
#include <functional>
#include <vector>

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


const std::vector<const char*> validationLayers = {
	"VK_LAYER_LUNARG_standard_validation"
};



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
	}

	void initVulkan() {
		createInstance();
		setupDebugCallback();
	}

	void mainLoop() {
		while (!glfwWindowShouldClose(m_window)) {
			glfwPollEvents();

			glfwSwapBuffers(m_window);
		}
	}

	void cleanup() {
		DestroyDebugReportCallbackEXT(m_instance, m_debug_callback, nullptr);
		vkDestroyInstance(m_instance, nullptr);

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
		if (CreateDebugReportCallbackEXT(m_instance, &createInfo, nullptr, &m_debug_callback) != VK_SUCCESS) {
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
		const char* layerPrefix,
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

	GLFWwindow* m_window;
	VkInstance m_instance;
	VkDebugReportCallbackEXT m_debug_callback;
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