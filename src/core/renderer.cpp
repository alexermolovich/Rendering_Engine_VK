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

/* Register each pipeline by its type and supported technique bits. */
GraphicsPipeline *Renderer::add_pipeline(PipelineType type, TechniqueFlags flags)
{
    const auto key = pipeline_hash(type, flags);
    for (uint32_t i = 0; i < b_pipeline_count; ++i)
        require(pipeline_hash(b_pipelines[i].type(), b_pipelines[i].flags()) != key,
                "Duplicate pipeline type and technique flags");
    require(b_pipeline_count < RENDERER_PIPELINE_COUNT, "RENDERER_PIPELINE_COUNT capacity exceeded");
    auto &pipeline = b_pipelines[b_pipeline_count];
    pipeline.configure(type, flags);
    ++b_pipeline_count;
    return &pipeline;
}

/* Build the existing scene and passes around a single owner. */
void Renderer::init()
{
    require(!b_initialized && !b_instance, "Renderer has already been initialized");
    spdlog::info("Initializing renderer");
    b_window = create_window(800, 600, "Renderer");
    vulkan_init();
    VK_CHECK(glfwCreateWindowSurface(b_instance, b_window, nullptr, &b_surface));
    pick_physical_device(b_instance, b_physical_device, b_surface);
    create_logical_device(b_physical_device, b_device, b_graphics_queue, b_present_queue, b_surface);
    load_extension_functions();
    create_command_pool();
    create_command_buffer();
    b_tracy_gpu_context =
        TracyVkContext(b_physical_device, b_device, b_graphics_queue, b_command_buffers[0].handle());
    add_swap_chain();
    add_samplers();
    init_sky_light();
    add_buffers();
    load_sponza();
    add_textures();
    add_descriptor_sets();
    init_rendering_tecniques();
    prepare_persistent_descriptors();
    prepare_frame_descriptors();
    for (uint32_t i = 0; i < FRAMES_IN_FLIGHT; ++i)
    {
        add_fence(b_command_buffer_fences[i]);
        add_semaphore(b_frame_semaphores[i]);
    }
    b_previous_time = glfwGetTime();
    b_initialized = true;
    spdlog::info("Renderer ready: {} pipelines, FIFO presentation", b_pipeline_count);
}

/* Workers only read copies; GLFW and Vulkan queue access stay on the main thread. */
PreparedFrame Renderer::prepare_frame(uint64_t sequence, Camera camera, Uniform uniform, SkyLightUniform sky)
{
    camera.update_view_matrix();
    camera.update_proj_matrix();
    PreparedFrame frame;
    frame.sequence = sequence;
    frame.camera = camera;
    frame.uniform = uniform;
    frame.uniform.cameraPos = camera.pos;
    frame.uniform.view = camera.get_view_matrix();
    frame.uniform.proj = camera.get_proj_matrix();
    frame.uniform.invProjection = glm::inverse(frame.uniform.proj);
    frame.sky = sky;
    frame.shadows.lightDirection = glm::vec4(sky.lightDirection, 0.0f);
    frame.shadows.shadowMapSizeCascadeCount =
        glm::uvec4(SHADOW_MAP_SIZE, SHADOW_MAP_SIZE, MAX_SHADOW_CASCADES, 1);
    frame.shadows.bias = glm::vec4(0.0005f, 0.0f, 0.0f, 0.0f);
    update_cascades(camera, sky.lightDirection, frame.shadows.cascades, MAX_SHADOW_CASCADES);
    return frame;
}

/* Bounded preparation overlaps CPU work while futures are consumed in FIFO order. */
void Renderer::run(uint64_t frame_limit)
{
    require(b_initialized, "Renderer must be initialized before run");
    uint64_t scheduled = b_submitted_sequence;
    const uint64_t end_sequence = frame_limit ? scheduled + frame_limit : UINT64_MAX;
    while (!glfwWindowShouldClose(b_window) && scheduled < end_sequence)
    {
        glfwPollEvents();
        if (glfwWindowShouldClose(b_window))
            break;
        
        update_camera();
       
        const auto sequence = scheduled++;
        const auto camera = b_camera;
        const auto uniform = b_uniform_data;
        const auto sky = b_sky_light_uniform_data;
      
        b_prepared_frames.enqueue(
            [sequence, camera, uniform, sky] { return prepare_frame(sequence, camera, uniform, sky); });
     
            if (b_prepared_frames.size() >= FRAMES_IN_FLIGHT)
            DrawFrame();
    }
    while (!b_prepared_frames.empty() && !glfwWindowShouldClose(b_window))
        DrawFrame();
    VK_CHECK(vkDeviceWaitIdle(b_device));
    spdlog::info("Submitted {} frames", b_submitted_sequence);
}

/* Track the actual image layout at each explicit pass boundary. */
void Renderer::transition_image(VkCommandBuffer cmd, RenderTarget &target, VkImageLayout layout,
                                VkPipelineStageFlags source_stage, VkPipelineStageFlags target_stage,
                                VkAccessFlags source_access, VkAccessFlags target_access)
{
    VkImageMemoryBarrier barrier = {};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = target.currentLayout;
    barrier.newLayout = layout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = target.image;
    barrier.subresourceRange = {is_depth_format(target.format) ? VK_IMAGE_ASPECT_DEPTH_BIT
                                                               : VK_IMAGE_ASPECT_COLOR_BIT,
                                0, target.mipLevels, 0, target.arraySize};
    barrier.srcAccessMask = target.currentLayout == VK_IMAGE_LAYOUT_UNDEFINED ? 0 : source_access;
    barrier.dstAccessMask = target_access;
    vkCmdPipelineBarrier(cmd, source_stage, target_stage, 0, 0, nullptr, 0, nullptr, 1, &barrier);
    target.currentLayout = layout;
}

/* Consume one prepared frame, record the named passes, then submit and present it. */
void Renderer::DrawFrame()
{
    require(b_initialized && !b_prepared_frames.empty(), "No prepared frame is available");
    PreparedFrame frame = b_prepared_frames.pop();
    require(frame.sequence == b_submitted_sequence, "Prepared frames arrived out of FIFO order");
    b_frame_index = static_cast<uint32_t>(frame.sequence % FRAMES_IN_FLIGHT);
    VkFence fence = b_command_buffer_fences[b_frame_index];
    VK_CHECK(vkWaitForFences(b_device, 1, &fence, VK_TRUE, UINT64_MAX));
    VkSemaphore acquired = b_frame_semaphores[b_frame_index];
    VkResult acquire_result;
    for (;;)
    {
        acquire_result = vkAcquireNextImageKHR(b_device, b_swapchain, UINT64_MAX, acquired, VK_NULL_HANDLE,
                                               &b_image_index);
        if (acquire_result != VK_ERROR_OUT_OF_DATE_KHR)
            break;
        recreate_swapchain();
        if (glfwWindowShouldClose(b_window))
            return;
    }
    if (acquire_result != VK_SUBOPTIMAL_KHR)
        VK_CHECK(acquire_result);

    const float aspect = static_cast<float>(b_settings.width) / b_settings.height;
    if (frame.camera.aspectRatio != aspect)
    {
        frame.camera.update_aspect_ratio(aspect);
        frame = prepare_frame(frame.sequence, frame.camera, frame.uniform, frame.sky);
    }
    b_uniform_data = frame.uniform;
    b_shadow_gen_uniform_data = frame.shadows;
    b_sky_light_uniform_data = frame.sky;
    map_buffer(b_device, b_ubo_buffer[b_frame_index], &frame.uniform);
    map_buffer(b_device, b_shadow_gen_buffer[b_frame_index], &frame.shadows);
    map_buffer(b_device, b_sky_lights_data_buffer[b_frame_index], &frame.sky);

    auto &command_buffer = b_command_buffers[b_frame_index];
    const VkCommandBuffer cmd = command_buffer.handle();
    VK_CHECK(vkResetCommandBuffer(cmd, 0));
    VkCommandBufferBeginInfo begin = {};
    begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    VK_CHECK(vkBeginCommandBuffer(cmd, &begin));
    {
        TracyVkZone(b_tracy_gpu_context, cmd, "Scene Rendering Pass");

        // The current passes share targets and filtering output between frame slots.
        VkMemoryBarrier previous_frame = {};
        previous_frame.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
        previous_frame.srcAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
        previous_frame.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0,
                             1, &previous_frame, 0, nullptr, 0, nullptr);

        VkDrawIndexedIndirectCommand reset = {};
        reset.instanceCount = 1;
        vkCmdUpdateBuffer(cmd, b_t_filter_indirect_args->buffer, 0, sizeof(reset), &reset);
        for (auto *args : b_t_filter_indirect_args_cascade_shadows)
            if (args)
                vkCmdUpdateBuffer(cmd, args->buffer, 0, sizeof(reset), &reset);
        VkMemoryBarrier reset_ready = {};
        reset_ready.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
        reset_ready.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        reset_ready.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1,
                             &reset_ready, 0, nullptr, 0, nullptr);

        b_cascade_filtering_technique.draw(command_buffer);
        b_triangle_filtering_technique.draw(command_buffer);

        VkMemoryBarrier filtered_ready = {};
        filtered_ready.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
        filtered_ready.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
        filtered_ready.dstAccessMask = VK_ACCESS_INDEX_READ_BIT | VK_ACCESS_INDIRECT_COMMAND_READ_BIT;
        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                             VK_PIPELINE_STAGE_VERTEX_INPUT_BIT | VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT, 0, 1,
                             &filtered_ready, 0, nullptr, 0, nullptr);

        transition_image(cmd, *b_shadow_render_targets.cascadeDepth, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
                         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                         VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT |
                             VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
                         VK_ACCESS_SHADER_READ_BIT, VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT);
        b_shadow_depth_technique.draw(command_buffer);
        transition_image(cmd, *b_shadow_render_targets.cascadeDepth, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                         VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT |
                             VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
                         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
                         VK_ACCESS_SHADER_READ_BIT);

        transition_image(cmd, *b_g_buffer_albedo_rt, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                         VK_ACCESS_SHADER_READ_BIT, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT);
        transition_image(cmd, *b_g_buffer_normal_specular_rt, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                         VK_ACCESS_SHADER_READ_BIT, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT);
        transition_image(cmd, *b_depth_buffer_rt, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
                         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                         VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT |
                             VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
                         VK_ACCESS_SHADER_READ_BIT, VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT);
        b_g_buffer_technique.draw(command_buffer);
        transition_image(cmd, *b_g_buffer_albedo_rt, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                         VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                         VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT);
        transition_image(cmd, *b_g_buffer_normal_specular_rt, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                         VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                         VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT);
        transition_image(cmd, *b_depth_buffer_rt, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                         VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT |
                             VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
                         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
                         VK_ACCESS_SHADER_READ_BIT);

        transition_image(cmd, *b_ssao_rt, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                         VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_SHADER_READ_BIT,
                         VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT);
        b_ssao_technique.draw(command_buffer);
        transition_image(cmd, *b_ssao_rt, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                         VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                         VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                         VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT);
        transition_image(cmd, *b_ssao_blur_rt, VK_IMAGE_LAYOUT_GENERAL, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                         VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT,
                         VK_ACCESS_SHADER_WRITE_BIT);
        b_ssao_blur_technique.draw(command_buffer);
        transition_image(cmd, *b_ssao_blur_rt, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                         VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                         VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT);

        transition_image(cmd, *b_screen_space_reflections_rt, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                         VK_ACCESS_SHADER_READ_BIT, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT);
        b_reflections_technique.draw(command_buffer);
        transition_image(cmd, *b_screen_space_reflections_rt, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                         VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                         VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT);
        transition_image(cmd, *b_compose_image_post_processing_rt, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                         VK_ACCESS_SHADER_READ_BIT, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT);
        b_compose_technique.draw(command_buffer);
        transition_image(cmd, *b_compose_image_post_processing_rt, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                         VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                         VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT);

        VkImageMemoryBarrier swapchain_barrier = {};
        swapchain_barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        swapchain_barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        swapchain_barrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        swapchain_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        swapchain_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        swapchain_barrier.image = b_swapchain_images[b_image_index];
        swapchain_barrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        swapchain_barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                             VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0, 0, nullptr, 0, nullptr, 1,
                             &swapchain_barrier);
        b_swapchain_image_layouts[b_image_index] = swapchain_barrier.newLayout;
        b_fxaa_technique.draw(command_buffer);
        swapchain_barrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        swapchain_barrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        swapchain_barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        swapchain_barrier.dstAccessMask = 0;
        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                             VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, nullptr, 0, nullptr, 1,
                             &swapchain_barrier);
        b_swapchain_image_layouts[b_image_index] = swapchain_barrier.newLayout;
    }
    TracyVkCollect(b_tracy_gpu_context, cmd);
    VK_CHECK(vkEndCommandBuffer(cmd));

    const VkSemaphore rendered = b_render_finished_semaphores[b_image_index];
    VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    VkSubmitInfo submit = {};
    submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit.waitSemaphoreCount = 1;
    submit.pWaitSemaphores = &acquired;
    submit.pWaitDstStageMask = &wait_stage;
    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &cmd;
    submit.signalSemaphoreCount = 1;
    submit.pSignalSemaphores = &rendered;
    VK_CHECK(vkResetFences(b_device, 1, &fence));
    VK_CHECK(vkQueueSubmit(b_graphics_queue, 1, &submit, fence));
    ++b_submitted_sequence;

    VkPresentInfoKHR present = {};
    present.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    present.waitSemaphoreCount = 1;
    present.pWaitSemaphores = &rendered;
    present.swapchainCount = 1;
    present.pSwapchains = &b_swapchain;
    present.pImageIndices = &b_image_index;
    const auto result = vkQueuePresentKHR(b_present_queue, &present);
    if (result != VK_SUCCESS && result != VK_ERROR_OUT_OF_DATE_KHR && result != VK_SUBOPTIMAL_KHR)
        VK_CHECK(result);
    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR ||
        acquire_result == VK_SUBOPTIMAL_KHR)
        recreate_swapchain();
    FrameMark;
}

