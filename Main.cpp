#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <iostream>
#include <stdexcept>
#include <vector>

const int WIDTH = 800;
const int HEIGHT = 600;

const std::vector<const char*> validationLayers = {
    "VK_LAYER_LUNARG_standard_validation"
};

#ifdef NDEBUG
const bool enableValidationLayers = false;
#else
const bool enableValidationLayers = true;
#endif

// vkCreateDebugReportCallbackEXT is not in Vulkan Core so we have to manually pull it's address
VkResult CreateDebugReportCallbackEXT(VkInstance instance, const VkDebugReportCallbackCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugReportCallbackEXT* pCallback) {
    auto func = (PFN_vkCreateDebugReportCallbackEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugReportCallbackEXT");
    if (func != nullptr) {
        return func(instance, pCreateInfo, pAllocator, pCallback);
    }
    else {
        return VK_ERROR_EXTENSION_NOT_PRESENT;
    }
}
// Similarly I need to pull vkDestroyDebugReportCallbackEXT
void DestroyDebugReportCallbackEXT(VkInstance instance, VkDebugReportCallbackEXT callback, const VkAllocationCallbacks* pAllocator) {
    auto func = (PFN_vkDestroyDebugReportCallbackEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugReportCallbackEXT");
    if (func != nullptr) {
        func(instance, callback, pAllocator);
    }
}

class VKTriangle {
public:
    void run() {
        initWindow();
        initVulkan();
        mainLoop();
        cleanup();
    }

private:
    GLFWwindow* m_window;

    VkInstance m_instance;
    VkDebugReportCallbackEXT m_callback;
    
    VkPhysicalDevice m_phyDevice = VK_NULL_HANDLE;
    VkDevice m_device;

    VkQueue m_graphicsQueue;

    void initWindow() {
        glfwInit();
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

        m_window = glfwCreateWindow(WIDTH, HEIGHT, "Vulkan", nullptr, nullptr);
    }

    void initVulkan() {
        createInstance();
        createDebugCallback();
        getPhysicalDevice();
        createLogicalDevice();
    }

    void mainLoop() {
        while (!glfwWindowShouldClose(m_window)) {
            glfwPollEvents();
        }
    }

    void cleanup() {

        vkDestroyDevice(m_device, nullptr);
        DestroyDebugReportCallbackEXT(m_instance, m_callback, nullptr);
        vkDestroyInstance(m_instance, nullptr);

        glfwDestroyWindow(m_window);
        glfwTerminate();
    }

/////////////////////////////////////////////////////////////////
// Main steps in Vulkan render pipeline
/////////////////////////////////////////////////////////////////

    void createInstance() {

        // Check layer support first
        if (enableValidationLayers && !checkValidationLayerSupport()) {
            throw std::runtime_error("validation layers requested, but not available!");
        }

        // VkApplicationInfo is optional
        VkApplicationInfo appInfo = {};
        appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        appInfo.pApplicationName = "VKTriangle";
        appInfo.apiVersion = VK_API_VERSION_1_0;
        appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.engineVersion = 0;
        appInfo.pEngineName = nullptr;
        appInfo.pNext = nullptr;

        VkInstanceCreateInfo createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        createInfo.pApplicationInfo = &appInfo;

        // Be careful trying to use all extensions may result in failing to create instance!
        //auto extensionNames = getAllExtensionNames();
        auto extensionNames = getRequiredExtensionNames();
        createInfo.enabledExtensionCount = static_cast<uint32_t>(extensionNames.size());
        createInfo.ppEnabledExtensionNames = extensionNames.data();

        if (enableValidationLayers) {
            createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
            createInfo.ppEnabledLayerNames = validationLayers.data();
        }
        else {
            createInfo.enabledLayerCount = 0;
        }

        // Create the actual instance
        if (vkCreateInstance(&createInfo, nullptr, &m_instance) != VK_SUCCESS) {
            throw std::runtime_error("failed to create instance!");
        }
    }

    // set up debugging environment using VK_EXT_debug_report extension
    void createDebugCallback() {
        if (!enableValidationLayers)
            return;

        VkDebugReportCallbackCreateInfoEXT createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT;
        createInfo.flags = VK_DEBUG_REPORT_ERROR_BIT_EXT | VK_DEBUG_REPORT_WARNING_BIT_EXT;
        createInfo.pfnCallback = debugCallback;

        if (CreateDebugReportCallbackEXT(m_instance, &createInfo, nullptr, &m_callback) != VK_SUCCESS) {
            throw std::runtime_error("failed to set up debug callback!");
        }
    }

    void getPhysicalDevice() {
        uint32_t deviceCount = 0;
        vkEnumeratePhysicalDevices(m_instance, &deviceCount, nullptr);
        if (deviceCount == 0) {
            throw std::runtime_error("failed to find any GPU with Vulkan support!");
        }

        std::vector<VkPhysicalDevice> phyDevices(deviceCount);
        vkEnumeratePhysicalDevices(m_instance, &deviceCount, phyDevices.data());

        if (!findOptimalPhysicalDevice(phyDevices, &m_phyDevice)) {
            throw std::runtime_error("failed to find a suitable GPU!");
        }
    }

    void createLogicalDevice() {
        int index = findQueueFamily(m_phyDevice, VK_QUEUE_GRAPHICS_BIT);

        VkDeviceQueueCreateInfo queueCreateInfo = {};
        queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueCreateInfo.queueCount = 1;
        queueCreateInfo.queueFamilyIndex = index;
        float queuePriority = 1.0f;
        queueCreateInfo.pQueuePriorities = &queuePriority;

        // TODO, fill more features later
        VkPhysicalDeviceFeatures phyDeviceFeatures = {};

        VkDeviceCreateInfo deviceCreateInfo = {};
        deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        // Note we do not need to explicitly create queue
        deviceCreateInfo.pQueueCreateInfos = &queueCreateInfo;
        deviceCreateInfo.queueCreateInfoCount = 1;
        deviceCreateInfo.enabledExtensionCount = 0;
        deviceCreateInfo.ppEnabledExtensionNames = nullptr;
        deviceCreateInfo.pEnabledFeatures = &phyDeviceFeatures;

        if (enableValidationLayers) {
            deviceCreateInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
            deviceCreateInfo.ppEnabledLayerNames = validationLayers.data();
        } else {
            deviceCreateInfo.enabledLayerCount = 0;
        }

        if (vkCreateDevice(m_phyDevice, &deviceCreateInfo, nullptr, &m_device) != VK_SUCCESS) {
            throw std::runtime_error("failed to create logical device!");
        }   
    }

/////////////////////////////////////////////////////////////////
// Utility member functions to for each Vulkan steps
/////////////////////////////////////////////////////////////////

    // get all extension names. Note your system may not support all of them!
    std::vector<const char*>getAllExtensionNames() {
        // Check Instance extension info
        uint32_t extensionCount;
        vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);
        std::vector<VkExtensionProperties> extensions(extensionCount);
        vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, extensions.data());

        std::vector<const char*> extensionNames;
        for (VkExtensionProperties extensionProperty : extensions) {
            extensionNames.push_back(extensionProperty.extensionName);
        }
#ifndef NDEBUG
        // Optionally List all supported instance extensions		
        std::cout << "available instance extensions: " << std::endl;
        for (const auto& extension : extensions) {
            std::cout << "\t" << extension.extensionName << std::endl;
        }
#endif
        return extensionNames;
    }

    // Get minimum extension names only for the app
    std::vector<const char*> getRequiredExtensionNames() {
        std::vector<const char*> extensionNames;

        unsigned int glfwExtensionCount = 0;
        const char** glfwExtensions;
        glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

        for (unsigned int i = 0; i < glfwExtensionCount; i++) {
            extensionNames.push_back(glfwExtensions[i]);
        }

        if (enableValidationLayers) {
            extensionNames.push_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);
        }

        return extensionNames;
    }

    bool checkValidationLayerSupport() {
        uint32_t layerCount;
        vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

        std::vector<VkLayerProperties> availableLayers(layerCount);
        vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

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

    bool findOptimalPhysicalDevice(std::vector<VkPhysicalDevice> phyDevices, VkPhysicalDevice* optimalPhyDevice) {

        // Use the simplest logic here to just pick the first suitable device
        // TODO: Will driver be able to automatically pick the better GPU?
        for (const auto phyDevice : phyDevices) {
            if (isPhyiscalDeviceSuitable(phyDevice)) {
                *optimalPhyDevice = phyDevice;
                return true;
            }
        }
        return false;
    }

    bool isPhyiscalDeviceSuitable(VkPhysicalDevice phyDevice) {

        bool isSuitable = true;
        // only care about qualified queue family now
        if (findQueueFamily(phyDevice, VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_TRANSFER_BIT) < 0)
            isSuitable = false;

        return isSuitable;
    }

    // Pass a combined bit mask of desired queue family flags here
    //VK_QUEUE_GRAPHICS_BIT = 0x00000001,
    //VK_QUEUE_COMPUTE_BIT = 0x00000002,
    //VK_QUEUE_TRANSFER_BIT = 0x00000004,
    //VK_QUEUE_SPARSE_BINDING_BIT = 0x00000008,
    int findQueueFamily(VkPhysicalDevice phyDevice, VkQueueFlags familyFlags) {
        
        uint32_t queueFamilyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(phyDevice, &queueFamilyCount, nullptr);
        std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(phyDevice, &queueFamilyCount, queueFamilies.data());

        int index = 0;
        for (const auto queueFamily : queueFamilies) {
            // We will just return the first qualified queue family here
            if (queueFamily.queueCount > 0 && queueFamily.queueFlags & familyFlags)
                return index;
            index++;
        }

        // return a negative value if no qualified queuefamily is found
        return -1;
    }

/////////////////////////////////////////////////////////////////
// static functions
/////////////////////////////////////////////////////////////////
    
    // This device a prototype of debug callback function
    static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(VkDebugReportFlagsEXT flags,
        VkDebugReportObjectTypeEXT objType,
        uint64_t obj,
        size_t location,
        int32_t code,
        const char* layerPrefix,
        const char* msg,
        void* userData) {
        std::cerr << "validation layer: " << msg << std::endl;

        return VK_FALSE;
    }
};

int main() {
    VKTriangle app;

    try {
        app.run();
    }
    catch (const std::runtime_error& e) {
        std::cerr << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    system("pause");
    return EXIT_SUCCESS;
}