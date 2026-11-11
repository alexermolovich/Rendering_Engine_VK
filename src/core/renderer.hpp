#pragma once
#include "camera.hpp"
#include "frame_preparation.hpp"
#include "gltf_model.hpp"
#include "graphics_pipeline.hpp"
#include "rendering_technique.hpp"
#include <GLFW/glfw3.h>
#include <string>
#include <tracy/TracyVulkan.hpp>
#include <vulkan/vulkan_core.h>

/* engine resource counts */
#define VALIDATION_LAYER_COUNT 1
#define DEVICE_EXTENSION_COUNT 8
#define INSTANCE_EXTENSION_CAPACITY 16
#define PHYSICAL_DEVICE_CAPACITY 16
#define DEVICE_EXTENSION_CAPACITY 512
#define QUEUE_FAMILY_CAPACITY 64
#define SWAPCHAIN_IMAGE_CAPACITY 8
#define SCENE_BUFFER_COUNT 2
#define FRAME_UNIFORM_BUFFER_COUNT (3 * FRAMES_IN_FLIGHT)
#define TEXTURE_UPLOAD_BUFFER_COUNT 1
#define TRIANGLE_FILTERING_BUFFER_COUNT 2
#define CASCADE_FILTERING_BUFFER_COUNT (2 * MAX_SHADOW_CASCADES)
#define SSAO_BUFFER_COUNT 1
#define RENDERER_BUFFER_COUNT \
    (SCENE_BUFFER_COUNT + FRAME_UNIFORM_BUFFER_COUNT + TEXTURE_UPLOAD_BUFFER_COUNT + \
     TRIANGLE_FILTERING_BUFFER_COUNT + CASCADE_FILTERING_BUFFER_COUNT + SSAO_BUFFER_COUNT)
#define RENDERER_TEXTURE_COUNT 2
#define RENDERER_SAMPLER_COUNT 2
#define RENDERER_SHADER_COUNT 10
#define RENDERER_PIPELINE_COUNT 10

/* rendering technique resource counts */
#define G_BUFFER_RENDER_TARGET_COUNT 3
#define SHADOW_RENDER_TARGET_COUNT 1
#define SSAO_RENDER_TARGET_COUNT 2
#define REFLECTIONS_RENDER_TARGET_COUNT 1
#define COMPOSITION_RENDER_TARGET_COUNT 1
#define SKYBOX_RENDER_TARGET_COUNT 1
#define RENDERER_RENDER_TARGET_COUNT \
    (G_BUFFER_RENDER_TARGET_COUNT + SHADOW_RENDER_TARGET_COUNT + SSAO_RENDER_TARGET_COUNT + \
     REFLECTIONS_RENDER_TARGET_COUNT + COMPOSITION_RENDER_TARGET_COUNT + SKYBOX_RENDER_TARGET_COUNT)

/* Owns Vulkan state, explicit resource lifetime, and ordered presentation. */
class Renderer
{
  public:
    /* renderer lifetime */
    Renderer();
    ~Renderer();
    Renderer(const Renderer &) = delete;
    Renderer &operator=(const Renderer &) = delete;
    void init();
    void shutdown();

    /* frame execution */
    void run(uint64_t frame_limit = 0);
    void DrawFrame();

  private:
    friend class CascadeFilteringTechnique;
    friend class TriangleFilteringTechnique;
    friend class ShadowDepthTechnique;
    friend class GBufferTechnique;
    friend class SsaoTechnique;
    friend class ReflectionsTechnique;
    friend class ComposeTechnique;
    friend class FxaaTechnique;
    friend class SsaoBlurTechnique;
    friend class SkyboxTechnique;

    /* application and window state */
    AppSettings b_settings = {600, 800};
    AppSettings *b_app_settings = &b_settings;
    GLFWwindow *b_window = nullptr;
    bool b_initialized = false;
    bool b_glfw_initialized = false;

    /* vulkan instance and validation */
    const char *b_validation_layers[VALIDATION_LAYER_COUNT] = {"VK_LAYER_KHRONOS_validation"};
    const char *b_extensions[INSTANCE_EXTENSION_CAPACITY] = {};
    uint32_t b_extension_count = 0;
    VkInstance b_instance = VK_NULL_HANDLE;
    VkDebugUtilsMessengerEXT b_debug_messenger = VK_NULL_HANDLE;
    VkSurfaceKHR b_surface = VK_NULL_HANDLE;

    /* vulkan device and features */
    const char *b_device_extensions[DEVICE_EXTENSION_COUNT] = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
        VK_KHR_PIPELINE_LIBRARY_EXTENSION_NAME,
        VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME,
        VK_EXT_GRAPHICS_PIPELINE_LIBRARY_EXTENSION_NAME,
        VK_EXT_COLOR_WRITE_ENABLE_EXTENSION_NAME,
        VK_EXT_VERTEX_INPUT_DYNAMIC_STATE_EXTENSION_NAME,
        VK_EXT_EXTENDED_DYNAMIC_STATE_3_EXTENSION_NAME,
        VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME};

    VkPhysicalDeviceColorWriteEnableFeaturesEXT b_color_write_feature = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_COLOR_WRITE_ENABLE_FEATURES_EXT};
    VkPhysicalDeviceExtendedDynamicState3FeaturesEXT b_eds3 = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTENDED_DYNAMIC_STATE_3_FEATURES_EXT};
    VkPhysicalDeviceBufferDeviceAddressFeatures b_buffer_device_address_feature = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES};
    VkPhysicalDeviceDescriptorIndexingFeatures b_descriptor_indexing_feature = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES};
    VkPhysicalDeviceDynamicRenderingFeaturesKHR b_dynamic_rendering_feature = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES, .pNext = &b_eds3};
    VkPhysicalDeviceVertexInputDynamicStateFeaturesEXT b_vertex_input_feature = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VERTEX_INPUT_DYNAMIC_STATE_FEATURES_EXT,
        .pNext = &b_dynamic_rendering_feature};

    VkPhysicalDeviceFeatures2 b_device_features2 = {};
    VkPhysicalDevice b_physical_device = VK_NULL_HANDLE;
    VkDevice b_device = VK_NULL_HANDLE;
    PFN_vkCmdSetColorWriteEnableEXT b_pfn_cmd_set_color_write_enable_ext = nullptr;
    PFN_vkCmdSetColorBlendEnableEXT b_pfn_cmd_set_color_blend_enable_ext = nullptr;
    PFN_vkCmdSetVertexInputEXT b_pfn_cmd_set_vertex_input_ext = nullptr;

    /* queues and command buffers */
    VkQueue b_graphics_queue = VK_NULL_HANDLE;
    VkQueue b_present_queue = VK_NULL_HANDLE;
    QueueFamilyIndices b_indecies;
    VkCommandPool b_graphics_command_pool = VK_NULL_HANDLE;
    CommandBuffer b_command_buffers[COMMAND_BUFFER_COUNT];

    /* frame preparation and synchronization */
    FramePreparationQueue b_prepared_frames;
    VkSemaphore b_frame_semaphores[COMMAND_BUFFER_COUNT] = {};
    VkFence b_command_buffer_fences[COMMAND_BUFFER_COUNT] = {};
    uint32_t b_frame_index = 0;
    uint32_t b_image_index = 0;
    uint64_t b_submitted_sequence = 0;

    /* swapchain resources */
    VkFormat b_swapchain_format[1] = {VK_FORMAT_B8G8R8A8_SRGB};
    VkSwapchainKHR b_swapchain = VK_NULL_HANDLE;
    VkImage b_swapchain_images[SWAPCHAIN_IMAGE_CAPACITY] = {};
    VkImageLayout b_swapchain_image_layouts[SWAPCHAIN_IMAGE_CAPACITY] = {};
    VkImageView b_swapchain_image_views[SWAPCHAIN_IMAGE_CAPACITY] = {};
    VkSemaphore b_render_finished_semaphores[SWAPCHAIN_IMAGE_CAPACITY] = {};
    uint32_t b_swapchain_image_count = 0;

    /* gpu profiling */
    TracyVkCtx b_tracy_gpu_context = nullptr;

    /* camera state */
    Camera b_camera;
    float b_camera_yaw = -90.0f;
    float b_camera_pitch = 0.0f;
    float b_camera_move_speed = 20.0f;
    float b_camera_turn_speed = 80.0f;
    double b_previous_time = 0.0;

    /* scene resources */
    GltfModel b_model_gltf;
    glm::mat4 b_identity_matrix = glm::mat4(1.0f);
    Uniform b_uniform_data = {};
    Buffer *b_ubo_buffer[FRAMES_IN_FLIGHT] = {};
    VkDeviceAddress b_vertex_buffer_address = 0;
    Image *b_default_mesh_texture = nullptr;

    /* sky lighting resources */
    SkyLightUniform b_sky_light_uniform_data = {};
    Buffer *b_sky_lights_data_buffer[FRAMES_IN_FLIGHT] = {};

    /* shared descriptors and samplers */
    VkDescriptorSetLayout b_per_frame_layout = VK_NULL_HANDLE;
    VkDescriptorSetLayout b_persistent_layout = VK_NULL_HANDLE;
    VkDescriptorPool b_descriptor_pool = VK_NULL_HANDLE;
    VkDescriptorSet b_sets_per_frame[FRAMES_IN_FLIGHT] = {};
    VkDescriptorSet b_sets_persistent = VK_NULL_HANDLE;
    VkPipelineLayout b_pipeline_layout = VK_NULL_HANDLE;
    Sampler *b_texture_sampler = nullptr;
    Sampler *b_render_target_sampler = nullptr;

    /* resource ownership */
    GraphicsPipeline b_pipelines[RENDERER_PIPELINE_COUNT];
    uint32_t b_pipeline_count = 0;
    Buffer b_buffers[RENDERER_BUFFER_COUNT];
    Image b_images[RENDERER_TEXTURE_COUNT];
    RenderTarget b_render_targets[RENDERER_RENDER_TARGET_COUNT];
    Shader b_shaders[RENDERER_SHADER_COUNT];
    Sampler b_samplers[RENDERER_SAMPLER_COUNT];

    /* triangle filtering resources */
    GraphicsPipeline *b_t_filtering_pipeline = nullptr;
    Shader *b_shader_computer_filtering = nullptr;
    Buffer *b_t_filtered_triangles = nullptr;
    Buffer *b_t_filter_indirect_args = nullptr;

    /* cascade filtering resources */
    GraphicsPipeline *b_t_filtering_pipeline_cascade_shadow_mapping = nullptr;
    Shader *b_shader_cascade_shadow_mapping_filtering = nullptr;
    Buffer *b_t_filtered_triangles_cascade_shadows[MAX_SHADOW_CASCADES] = {};
    Buffer *b_t_filter_indirect_args_cascade_shadows[MAX_SHADOW_CASCADES] = {};

    /* shadow depth resources */
    GraphicsPipeline *b_shadow_depth_pipeline = nullptr;
    Shader *b_shadow_depth_shader = nullptr;
    ShadowGenerationUniform b_shadow_gen_uniform_data = {};
    ShadowRenderTargets b_shadow_render_targets = {};
    Buffer *b_shadow_gen_buffer[FRAMES_IN_FLIGHT] = {};

    /* g buffer resources */
    GraphicsPipeline *b_g_buffer_pipeline = nullptr;
    Shader *b_g_buffer_shader = nullptr;
    RenderTarget *b_g_buffer_normal_specular_rt = nullptr;
    RenderTarget *b_g_buffer_albedo_rt = nullptr;
    RenderTarget *b_depth_buffer_rt = nullptr;

    /* ssao resources */
    GraphicsPipeline *b_ssao_pipeline = nullptr;
    GraphicsPipeline *b_ssao_blur_pipeline = nullptr;
    Shader *b_ssao_shader = nullptr;
    Shader *b_ssao_blur_shader = nullptr;
    RenderTarget *b_ssao_blur_rt = nullptr;
    RenderTarget *b_ssao_rt = nullptr;
    Buffer *b_ssao_kernel_buffer = nullptr;
    Image *b_ssao_noise_texture = nullptr;
    glm::vec4 b_ssao_kernels[SSAO_KERNEL_SIZE] = {};
    glm::vec4 b_ssao_noise[SSAO_NOISE_DIM * SSAO_NOISE_DIM] = {};

    /* screen space reflections resources */
    GraphicsPipeline *b_screen_space_reflections_pipeline = nullptr;
    Shader *b_screen_space_reflections_shader = nullptr;
    RenderTarget *b_screen_space_reflections_rt = nullptr;

    /* composition resources */
    GraphicsPipeline *b_compose_pipeline = nullptr;
    Shader *b_compose_shader = nullptr;
    RenderTarget *b_compose_image_post_processing_rt = nullptr;

    /* fxaa resources */
    GraphicsPipeline *b_fxaa_pipeline = nullptr;
    Shader *b_fxaa_pass_shader = nullptr;

    /* skybox resources */
    GraphicsPipeline *b_skybox_pipeline = nullptr;
    Shader *b_skybox_pass_shader = nullptr;
    Image b_skybox_image;
    RenderTarget *b_skybox_rt = nullptr;

    /* rendering techniques */
    CascadeFilteringTechnique b_cascade_filtering_technique;
    TriangleFilteringTechnique b_triangle_filtering_technique;
    ShadowDepthTechnique b_shadow_depth_technique;
    GBufferTechnique b_g_buffer_technique;
    SsaoTechnique b_ssao_technique;
    ReflectionsTechnique b_reflections_technique;
    ComposeTechnique b_compose_technique;
    FxaaTechnique b_fxaa_technique;
    SsaoBlurTechnique b_ssao_blur_technique;
    SkyboxTechnique b_skybox_rendering_technique;

    /* camera updates */
    void update_camera();

    /* frame preparation */
    static PreparedFrame prepare_frame(uint64_t sequence, Camera camera, Uniform uniform,
                                       SkyLightUniform sky);

    /* rendering technique and pipeline setup */
    void init_rendering_tecniques();
    GraphicsPipeline *add_pipeline(PipelineType type, TechniqueFlags flags);

    /* render target creation and transitions */
    void add_render_target(VkDevice b_device, VkPhysicalDevice b_physical_device,
                           const RenderTargetDesc *pDesc, RenderTarget **ppRenderTarget);
    void transition_image(VkCommandBuffer cmd, RenderTarget &target, VkImageLayout layout,
                          VkPipelineStageFlags source_stage, VkPipelineStageFlags target_stage,
                          VkAccessFlags source_access, VkAccessFlags target_access);
    void destroy_render_targets();

    /* swapchain management */
    void add_swap_chain();
    void recreate_swapchain();
    void destroy_swapchain();

    /* command buffer and synchronization setup */
    void create_command_pool();
    void create_command_buffer();
    void add_semaphore(VkSemaphore &sem);
    void add_fence(VkFence &fence);

    /* buffer allocation and uploads */
    void add_buffers();
    void add_buffer(VkDevice b_device, VkPhysicalDevice b_physical_device, const BufferDesc &desc,
                    Buffer **ppBuffer);
    void remove_buffer(VkDevice b_device, Buffer *pBuffer);
    void map_buffer(VkDevice b_device, Buffer *pBuffer, const void *pData);
    uint32_t find_memory_type(VkPhysicalDevice b_physical_device, uint32_t typeFilter,
                              VkMemoryPropertyFlags properties);

    /* texture allocation and uploads */
    void add_textures();
    void add_texture(VkDevice b_device, VkPhysicalDevice b_physical_device, const TextureDesc &desc,
                     Image **ppTexture);
    static VkDeviceSize format_byte_size(VkFormat format);
    bool is_depth_format(VkFormat format);
    void map_texture_data(VkDevice b_device, VkPhysicalDevice b_physical_device, VkCommandPool cmdPool,
                          VkQueue queue, Image *pTexture, const void *pData, VkDeviceSize dataSize);

    /* sampler creation */
    void add_samplers();
    void add_sampler(VkDevice b_device, const SamplerDesc &desc, Sampler **ppSampler);

    /* descriptor allocation and updates */
    void add_descriptor_sets();
    void prepare_frame_descriptors();
    void prepare_persistent_descriptors();

    /* ssao sample generation */
    float lerp(float a, float b, float f);
    void generate_ssao_kernel(bool deterministicSeed = false);
    void generate_ssao_noise();

    /* shadow cascade setup and updates */
    void add_shadow_cascade_layer_views();
    bool has_shadow_render_targets() const;
    static void update_cascades(const Camera &b_camera, const glm::vec3 &lightDir,
                                ShadowCascadeData *cascades, uint32_t cascadeCount);
    void update_shadow_generation_data(bool shadowRenderTargetsReady);

    /* scene and lighting setup */
    void setup_ubo();
    void init_sky_light();
    void load_sponza();
    void load_skybox(const std::string &filename);

    /* shader loading */
    void load_shader_code(Shader &shader, VkShaderStageFlagBits stage, const std::string &filepath);

    /* window creation */
    GLFWwindow *create_window(int width, int height, const char *title);

    /* vulkan instance, device, and extensions */
    void vulkan_init();
    void load_extension_functions();
    void pick_physical_device(VkInstance b_instance, VkPhysicalDevice &b_physical_device,
                              VkSurfaceKHR b_surface);
    void create_logical_device(VkPhysicalDevice b_physical_device, VkDevice &b_device,
                               VkQueue &b_graphics_queue, VkQueue &b_present_queue, VkSurfaceKHR b_surface);
    QueueFamilyIndices find_queue_families(VkPhysicalDevice b_device, VkSurfaceKHR b_surface);

    /* vulkan diagnostics */
    static VKAPI_ATTR VkBool32 VKAPI_CALL debug_callback(
        VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageType,
        const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData, void *pUserData);
};
