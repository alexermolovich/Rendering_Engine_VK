#include "renderer.hpp"
#include "vulkan_check.hpp"
#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <glm/gtc/type_ptr.hpp>
#include <iterator>
#include <meshoptimizer/src/meshoptimizer.h>
#include <random>
#include <stb_image.h>

/* Construct each pass with its renderer. */
Renderer::Renderer()
    : b_cascade_filtering_technique(*this),
      b_triangle_filtering_technique(*this),
      b_shadow_depth_technique(*this),
      b_g_buffer_technique(*this),
      b_ssao_technique(*this),
      b_reflections_technique(*this),
      b_compose_technique(*this),
      b_fxaa_technique(*this),
      b_ssao_blur_technique(*this)
{
}

/* Load extension functions. */
void Renderer::load_extension_functions()
{
    b_pfn_cmd_set_color_write_enable_ext =
        (PFN_vkCmdSetColorWriteEnableEXT)vkGetDeviceProcAddr(b_device, "vkCmdSetColorWriteEnableEXT");
    b_pfn_cmd_set_color_blend_enable_ext =
        (PFN_vkCmdSetColorBlendEnableEXT)vkGetDeviceProcAddr(b_device, "vkCmdSetColorBlendEnableEXT");
    b_pfn_cmd_set_vertex_input_ext =
        (PFN_vkCmdSetVertexInputEXT)vkGetDeviceProcAddr(b_device, "vkCmdSetVertexInputEXT");

    require(b_pfn_cmd_set_color_write_enable_ext, "vkCmdSetColorWriteEnableEXT not found");
    require(b_pfn_cmd_set_color_blend_enable_ext, "vkCmdSetColorBlendEnableEXT not found");
    require(b_pfn_cmd_set_vertex_input_ext, "vkCmdSetVertexInputEXT not found");
}

/* Add swap chain. */
void Renderer::add_swap_chain()
{

    VkSurfaceCapabilitiesKHR surfaceCapabilities;
    VK_CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(b_physical_device, b_surface, &surfaceCapabilities));

    uint32_t imageCount = surfaceCapabilities.minImageCount + 1;
    if (surfaceCapabilities.maxImageCount > 0 && imageCount > surfaceCapabilities.maxImageCount)
    {
        imageCount = surfaceCapabilities.maxImageCount;
    }

    VkExtent2D extent;
    if (surfaceCapabilities.currentExtent.width != UINT32_MAX)
    {
        extent = surfaceCapabilities.currentExtent;
    }
    else
    {
        extent = {b_app_settings->width, b_app_settings->height};
        extent.width = std::clamp(extent.width, surfaceCapabilities.minImageExtent.width,
                                  surfaceCapabilities.maxImageExtent.width);
        extent.height = std::clamp(extent.height, surfaceCapabilities.minImageExtent.height,
                                   surfaceCapabilities.maxImageExtent.height);
    }

    b_settings.width = extent.width;
    b_settings.height = extent.height;
    VkSwapchainCreateInfoKHR swapchainDesc{};
    swapchainDesc.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    swapchainDesc.pNext = nullptr;
    swapchainDesc.flags = 0;
    swapchainDesc.surface = b_surface;
    swapchainDesc.minImageCount = imageCount;
    swapchainDesc.imageFormat = VK_FORMAT_B8G8R8A8_SRGB;
    swapchainDesc.imageColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
    swapchainDesc.imageExtent = extent;
    swapchainDesc.imageArrayLayers = 1;
    swapchainDesc.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    uint32_t queueFamilyIndices[] = {b_indecies.graphicsFamily.value(), b_indecies.presentFamily.value()};
    if (queueFamilyIndices[0] != queueFamilyIndices[1])
    {
        swapchainDesc.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        swapchainDesc.queueFamilyIndexCount = 2;
        swapchainDesc.pQueueFamilyIndices = queueFamilyIndices;
    }
    else
    {
        swapchainDesc.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        swapchainDesc.queueFamilyIndexCount = 0;
        swapchainDesc.pQueueFamilyIndices = nullptr;
    }

    swapchainDesc.preTransform = surfaceCapabilities.currentTransform;
    swapchainDesc.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    swapchainDesc.presentMode = VK_PRESENT_MODE_FIFO_KHR;
    swapchainDesc.clipped = VK_TRUE;
    swapchainDesc.oldSwapchain = VK_NULL_HANDLE;

    VK_CHECK(vkCreateSwapchainKHR(b_device, &swapchainDesc, nullptr, &b_swapchain));

    {
        uint32_t count = 0;
        VK_CHECK(vkGetSwapchainImagesKHR(b_device, b_swapchain, &count, nullptr));
        require(count <= SWAPCHAIN_IMAGE_CAPACITY, "SWAPCHAIN_IMAGE_CAPACITY exceeded");
        VK_CHECK(vkGetSwapchainImagesKHR(b_device, b_swapchain, &count, b_swapchain_images));
        b_swapchain_image_count = count;

        for (uint32_t i = 0; i < count; i++)
        {
            b_swapchain_image_layouts[i] = VK_IMAGE_LAYOUT_UNDEFINED;
            VkImageViewCreateInfo viewInfo = {};
            viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            viewInfo.image = b_swapchain_images[i];
            viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
            viewInfo.format = VK_FORMAT_B8G8R8A8_SRGB;
            viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            viewInfo.subresourceRange.baseMipLevel = 0;
            viewInfo.subresourceRange.levelCount = 1;
            viewInfo.subresourceRange.baseArrayLayer = 0;
            viewInfo.subresourceRange.layerCount = 1;
            VK_CHECK(vkCreateImageView(b_device, &viewInfo, nullptr, &b_swapchain_image_views[i]));
        }

        for (uint32_t i = 0; i < b_swapchain_image_count; ++i)
            add_semaphore(b_render_finished_semaphores[i]);
    }
}

/* Create window. */
GLFWwindow *Renderer::create_window(int width, int height, const char *title)
{

    if (!glfwInit())
    {
        throw std::runtime_error("Failed to initialize GLFW!");
    }

    b_glfw_initialized = true;
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

    GLFWwindow *b_window = glfwCreateWindow(width, height, title, nullptr, nullptr);

    if (!b_window)
    {
        glfwTerminate();
        throw std::runtime_error("Failed to create GLFW window!");
    }

    return b_window;
}

/* Pick physical device. */
void Renderer::pick_physical_device(VkInstance b_instance, VkPhysicalDevice &b_physical_device,
                                    VkSurfaceKHR b_surface)
{
    uint32_t deviceCount = 0;
    VK_CHECK(vkEnumeratePhysicalDevices(b_instance, &deviceCount, nullptr));

    if (deviceCount == 0)
    {
        throw std::runtime_error("Failed to find GPUs with Vulkan support!");
    }

    require(deviceCount <= PHYSICAL_DEVICE_CAPACITY, "PHYSICAL_DEVICE_CAPACITY exceeded");
    VkPhysicalDevice devices[PHYSICAL_DEVICE_CAPACITY] = {};
    VK_CHECK(vkEnumeratePhysicalDevices(b_instance, &deviceCount, devices));
    {
        b_descriptor_indexing_feature.pNext = &b_buffer_device_address_feature;
        b_eds3.pNext = &b_descriptor_indexing_feature;
        b_color_write_feature.pNext = &b_vertex_input_feature;
    }
    for (uint32_t deviceIndex = 0; deviceIndex < deviceCount; ++deviceIndex)
    {
        const auto b_device = devices[deviceIndex];
        VkPhysicalDeviceProperties deviceProperties;

        vkGetPhysicalDeviceProperties(b_device, &deviceProperties);
        b_device_features2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
        b_device_features2.pNext = &b_color_write_feature;

        vkGetPhysicalDeviceFeatures2(b_device, &b_device_features2);

        b_indecies = find_queue_families(b_device, b_surface);

        uint32_t extensionCount;
        VK_CHECK(vkEnumerateDeviceExtensionProperties(b_device, nullptr, &extensionCount, nullptr));
        require(extensionCount <= DEVICE_EXTENSION_CAPACITY, "DEVICE_EXTENSION_CAPACITY exceeded");
        VkExtensionProperties availableExtensions[DEVICE_EXTENSION_CAPACITY] = {};
        VK_CHECK(vkEnumerateDeviceExtensionProperties(b_device, nullptr, &extensionCount,
                                                      availableExtensions));

        const bool hasRequiredExtensions = std::all_of(
            std::begin(b_device_extensions), std::end(b_device_extensions), [&](const char *required) {
                return std::any_of(availableExtensions, availableExtensions + extensionCount,
                                   [&](const VkExtensionProperties &extension) {
                                       return std::strcmp(required, extension.extensionName) == 0;
                                   });
            });

        if (b_indecies.is_complete() && hasRequiredExtensions &&
            b_color_write_feature.colorWriteEnable && b_vertex_input_feature.vertexInputDynamicState &&
            b_dynamic_rendering_feature.dynamicRendering && b_eds3.extendedDynamicState3ColorBlendEnable &&
            b_buffer_device_address_feature.bufferDeviceAddress &&
            b_descriptor_indexing_feature.shaderSampledImageArrayNonUniformIndexing &&
            b_device_features2.features.samplerAnisotropy)
        {
            b_physical_device = b_device;
            spdlog::info("Using GPU: {}", deviceProperties.deviceName);
            return;
        }
    }

    throw std::runtime_error("Failed to find a suitable GPU!");
}

/* Create logical device. */
void Renderer::create_logical_device(VkPhysicalDevice b_physical_device, VkDevice &b_device,
                                     VkQueue &b_graphics_queue, VkQueue &b_present_queue,
                                     VkSurfaceKHR b_surface)
{
    QueueFamilyIndices indices = find_queue_families(b_physical_device, b_surface);

    const uint32_t queueFamilies[] = {indices.graphicsFamily.value(), indices.presentFamily.value()};
    const uint32_t queueCount = queueFamilies[0] == queueFamilies[1] ? 1 : 2;
    VkDeviceQueueCreateInfo queueCreateInfos[2] = {};

    float queuePriority = 1.0f;
    for (uint32_t i = 0; i < queueCount; ++i)
    {
        auto &queueCreateInfo = queueCreateInfos[i];
        queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueCreateInfo.queueFamilyIndex = queueFamilies[i];
        queueCreateInfo.queueCount = 1;
        queueCreateInfo.pQueuePriorities = &queuePriority;
    }

    VkDeviceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;

    createInfo.queueCreateInfoCount = queueCount;
    createInfo.pQueueCreateInfos = queueCreateInfos;
    createInfo.enabledExtensionCount = DEVICE_EXTENSION_COUNT;
    createInfo.pNext = &b_device_features2;
    createInfo.ppEnabledExtensionNames = b_device_extensions;
    createInfo.enabledLayerCount = VALIDATION_LAYER_COUNT;
    createInfo.ppEnabledLayerNames = b_validation_layers;

    if (vkCreateDevice(b_physical_device, &createInfo, nullptr, &b_device) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create logical device!");
    }
    vkGetDeviceQueue(b_device, indices.graphicsFamily.value(), 0, &b_graphics_queue);
    vkGetDeviceQueue(b_device, indices.presentFamily.value(), 0, &b_present_queue);
}

/* Find queue families. */
QueueFamilyIndices Renderer::find_queue_families(VkPhysicalDevice b_device, VkSurfaceKHR b_surface)
{
    QueueFamilyIndices indices;

    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(b_device, &queueFamilyCount, nullptr);

    require(queueFamilyCount <= QUEUE_FAMILY_CAPACITY, "QUEUE_FAMILY_CAPACITY exceeded");
    VkQueueFamilyProperties queueFamilies[QUEUE_FAMILY_CAPACITY] = {};
    vkGetPhysicalDeviceQueueFamilyProperties(b_device, &queueFamilyCount, queueFamilies);

    for (uint32_t i = 0; i < queueFamilyCount; ++i)
    {
        const auto &queueFamily = queueFamilies[i];
        if ((queueFamily.queueFlags & (VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT)) ==
            (VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT))
        {
            indices.graphicsFamily = i;
        }

        VkBool32 presentSupport = false;
        VK_CHECK(vkGetPhysicalDeviceSurfaceSupportKHR(b_device, i, b_surface, &presentSupport));

        if (presentSupport)
        {
            indices.presentFamily = i;
        }

        if (indices.is_complete())
        {
            break;
        }
    }

    return indices;
}

/* Vulkan init. */
void Renderer::vulkan_init()
{
    VkApplicationInfo appInfo{};

    VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "Vulkan Application";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "No Engine";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_3;
    appInfo.pNext = NULL;
    uint32_t glfwExtensionCount = 0;
    const char **glfwExtensions;
    glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

    require(glfwExtensions, "GLFW did not provide Vulkan instance extensions");
    require(glfwExtensionCount < INSTANCE_EXTENSION_CAPACITY, "INSTANCE_EXTENSION_CAPACITY exceeded");
    std::copy_n(glfwExtensions, glfwExtensionCount, b_extensions);
    b_extensions[glfwExtensionCount] = VK_EXT_DEBUG_UTILS_EXTENSION_NAME;
    b_extension_count = glfwExtensionCount + 1;

    debugCreateInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    debugCreateInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
                                      VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                                      VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    debugCreateInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                                  VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                                  VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    debugCreateInfo.pfnUserCallback = debug_callback;
    debugCreateInfo.pUserData = nullptr;

    {
        VkInstanceCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        createInfo.pApplicationInfo = &appInfo;
        createInfo.enabledExtensionCount = b_extension_count;
        createInfo.ppEnabledExtensionNames = b_extensions;
        createInfo.enabledLayerCount = VALIDATION_LAYER_COUNT;
        createInfo.ppEnabledLayerNames = b_validation_layers;
        createInfo.pNext = &debugCreateInfo;

        if (vkCreateInstance(&createInfo, nullptr, &b_instance) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to create Vulkan instance!");
        }
    }
    auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(b_instance,
                                                                          "vkCreateDebugUtilsMessengerEXT");
    if (func != nullptr)
    {
        VK_CHECK(func(b_instance, &debugCreateInfo, nullptr, &b_debug_messenger));
    }
}

/* Debug callback. */
VKAPI_ATTR VkBool32 VKAPI_CALL Renderer::debug_callback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageType,
    const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData, void *pUserData)
{
    if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
        spdlog::error("Vulkan: {}", pCallbackData->pMessage);
    else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
        spdlog::warn("Vulkan: {}", pCallbackData->pMessage);
    else
        spdlog::debug("Vulkan: {}", pCallbackData->pMessage);
    return VK_FALSE;
}

