#include "renderer.hpp"
#include "vulkan_check.hpp"
#include <glm/gtc/type_ptr.hpp>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>

void Renderer::init_imgui()
{
    IMGUI_CHECKVERSION();
    b_imgui_context = ImGui::CreateContext();
    ImGui::GetIO().IniFilename = nullptr;
    ImGui::StyleColorsDark();
    require(ImGui_ImplGlfw_InitForVulkan(b_window, true), "Failed to initialize ImGui GLFW backend");
    init_imgui_vulkan();
}

/* Called with the device idle, including after swapchain recreation. */
void Renderer::init_imgui_vulkan()
{
    if (ImGui::GetIO().BackendRendererUserData)
        ImGui_ImplVulkan_Shutdown();

    require(b_swapchain_image_count >= 2, "ImGui requires at least two swapchain images");
    ImGui_ImplVulkan_InitInfo info = {};
    info.ApiVersion = VK_API_VERSION_1_3;
    info.Instance = b_instance;
    info.PhysicalDevice = b_physical_device;
    info.Device = b_device;
    info.QueueFamily = b_indecies.graphicsFamily.value();
    info.Queue = b_graphics_queue;
    info.DescriptorPoolSize = IMGUI_IMPL_VULKAN_MINIMUM_SAMPLED_IMAGE_POOL_SIZE;
    info.MinImageCount = 2;
    info.ImageCount = b_swapchain_image_count;
    info.UseDynamicRendering = true;
    info.PipelineInfoMain.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
    auto &rendering = info.PipelineInfoMain.PipelineRenderingCreateInfo;
    rendering.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
    rendering.colorAttachmentCount = 1;
    rendering.pColorAttachmentFormats = b_swapchain_format;
    info.CheckVkResultFn = [](VkResult result) { check_vulkan(result, "ImGui Vulkan backend"); };
    require(ImGui_ImplVulkan_Init(&info), "Failed to initialize ImGui Vulkan backend");
}

void Renderer::update_imgui()
{
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    ImGui::SetNextWindowPos(ImVec2(10.0f, 10.0f), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Point light", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::SetNextItemWidth(300.0f);
        ImGui::DragFloat3("Position", glm::value_ptr(b_point_light_position), 1.0f, 0.0f, 0.0f, "%.1f");
    }
    ImGui::End();
    ImGui::Render();
}

/* Draw inside the final color attachment after FXAA, before presentation. */
void Renderer::draw_imgui(VkCommandBuffer cmd)
{
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);
}

void Renderer::shutdown_imgui()
{
    if (!b_imgui_context)
        return;
    ImGui::SetCurrentContext(b_imgui_context);
    if (ImGui::GetIO().BackendRendererUserData)
        ImGui_ImplVulkan_Shutdown();
    if (ImGui::GetIO().BackendPlatformUserData)
        ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext(b_imgui_context);
    b_imgui_context = nullptr;
}
