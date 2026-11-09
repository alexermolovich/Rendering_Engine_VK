#pragma once
#include <cstddef>
#include <cstdint>
#include <glm/glm.hpp>
#include <optional>
#include <vulkan/vulkan.h>

inline constexpr uint32_t MAX_SHADOW_CASCADES = 4;
inline constexpr uint32_t SHADOW_MAP_SIZE = 2048;
inline constexpr uint32_t COMMAND_BUFFER_COUNT = 2;
inline constexpr uint32_t BISTRO_TEXTURE_COUNT = 255;
inline constexpr uint32_t SSAO_KERNEL_SIZE = 32;
inline constexpr uint32_t SSAO_NOISE_DIM = 4;
inline constexpr uint32_t MAX_POINT_LIGHTS = 1;
inline constexpr uint32_t MAX_DIRECTIONAL_LIGHT = 1;
inline constexpr uint32_t PERSISTENT_BINDING_COUNT = 14;
inline constexpr uint32_t PER_FRAME_BINDING_COUNT = 3;
inline constexpr uint32_t FRAMES_IN_FLIGHT = 2;
inline constexpr uint32_t PERSISTENT = 0;
inline constexpr uint32_t PER_FRAME = 1;

/* Buffer storage and its role are separate from Vulkan allocation flags. */
enum class BufferType
{
    general,
    uniform,
    storage
};
enum class BufferUsage
{
    vertex_data,
    index_data,
    rendering_data,
    rendering_output,
    compute_data,
    staging
};

struct alignas(16) ShadowCascadeData
{
    alignas(16) glm::mat4 viewProj = glm::mat4(1.0f);
    alignas(4) float splitDepth = 0.0f;
    alignas(4) float padding[3] = {};
};

struct alignas(16) ShadowGenerationUniform
{
    alignas(16) ShadowCascadeData cascades[MAX_SHADOW_CASCADES];
    alignas(16) glm::vec4 lightDirection = glm::vec4(0.0f);
    alignas(16) glm::uvec4 shadowMapSizeCascadeCount = glm::uvec4(0);
    alignas(16) glm::vec4 bias = glm::vec4(0.0f);
};

struct SkyLightUniform
{
    alignas(16) glm::vec3 lightDirection;
    alignas(16) glm::vec3 lightColor;
    alignas(4) float lightIntensity;
};

struct QueueFamilyIndices
{
    std::optional<uint32_t> graphicsFamily;
    std::optional<uint32_t> presentFamily;
    std::optional<uint32_t> computeFamily;

    bool is_complete()
    {
        return graphicsFamily.has_value() && presentFamily.has_value();
    }
};

struct Uniform
{
    alignas(16) glm::mat4 proj;
    alignas(16) glm::mat4 model;
    alignas(16) glm::mat4 view;
    alignas(16) glm::vec3 cameraPos;
    float nearPlane;
    float farPlane;
    alignas(16) glm::mat4 invProjection;
};

struct AppSettings
{
    uint32_t height;
    uint32_t width;
};

struct RenderTargetDesc
{
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t depth = 1;
    uint32_t arraySize = 1;
    uint32_t mipLevels = 1;
    VkFormat format = VK_FORMAT_UNDEFINED;
    VkSampleCountFlagBits sampleCount = VK_SAMPLE_COUNT_1_BIT;

    VkClearValue clearValue = {};

    VkImageUsageFlags usageFlags = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;

    const char *pName = nullptr;

    VkImageLayout startLayout = VK_IMAGE_LAYOUT_UNDEFINED;
};

struct SamplerDesc
{
    VkFilter magFilter = VK_FILTER_LINEAR;
    VkFilter minFilter = VK_FILTER_LINEAR;
    VkSamplerMipmapMode mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    VkSamplerAddressMode addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    VkSamplerAddressMode addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    VkSamplerAddressMode addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    float mipLodBias = 0.0f;
    VkBool32 anisotropyEnable = VK_FALSE;
    float maxAnisotropy = 1.0f;
    VkBool32 compareEnable = VK_FALSE;
    VkCompareOp compareOp = VK_COMPARE_OP_NEVER;
    float minLod = 0.0f;
    float maxLod = VK_LOD_CLAMP_NONE;
    VkBorderColor borderColor = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK;
    VkBool32 unnormalizedCoordinates = VK_FALSE;
};

struct BufferDesc
{
    VkDeviceSize size = 0;
    VkBufferUsageFlags usage = 0;
    VkMemoryPropertyFlags memoryProperties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    VkSharingMode sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    BufferType type = BufferType::general;
    BufferUsage purpose = BufferUsage::rendering_data;
};

struct TriangleFilteringPushConstants
{
    VkDeviceAddress indexBuffer;
    VkDeviceAddress vertexBuffer;
    VkDeviceAddress filteredBuffers;
    VkDeviceAddress drawCommand;
    alignas(16) glm::mat4 modelViewProjection;
    uint32_t vertexStride;
    uint32_t indexCount;
    uint32_t vertexCount;
};

struct TriangleFilteringPushConstantsCascadeShadows
{
    VkDeviceAddress indexBuffer;
    VkDeviceAddress vertexBuffer;
    VkDeviceAddress filteredBuffers[MAX_SHADOW_CASCADES];
    VkDeviceAddress drawCommands[MAX_SHADOW_CASCADES];
    alignas(16) glm::mat4 modelViewProjection;
    uint32_t vertexStride;
    uint32_t indexCount;
    uint32_t vertexCount;
};

struct ShadowDepthPushConstants
{
    uint32_t cascadeIndex;
};

struct PointLightPushConstants
{
    alignas(16) glm::vec4 positionRadius;
    alignas(16) glm::vec4 colorIntensity;
};

struct DirectionalLightPushConstants
{
    alignas(16) glm::vec4 directionIntensity;
    alignas(16) glm::vec4 color;
};

struct ComposePushConstants
{
    DirectionalLightPushConstants directionalLight[MAX_DIRECTIONAL_LIGHT];
    int32_t active_directionalLight = 0;
    PointLightPushConstants pointLight[MAX_POINT_LIGHTS];
    int32_t active_pointlight = 0;
};

struct LightsDataBuffer
{

    DirectionalLightPushConstants directionalLight[MAX_DIRECTIONAL_LIGHT];
    // int active_directionalLight = 0;

    PointLightPushConstants pointLight[MAX_POINT_LIGHTS];
    // int active_pointlight       = 0;
};

struct TextureDesc
{
    int width = 0;
    int height = 0;
    int channels = 0;
    VkFormat format = VK_FORMAT_R8G8B8A8_UNORM;
    uint32_t mipLevels = 1;
    VkImageUsageFlags usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
};

class RenderTarget;

struct ShadowRenderTargets
{
    RenderTarget *cascadeDepth = nullptr;
    VkImageView cascadeLayerViews[MAX_SHADOW_CASCADES] = {};
};

static_assert(sizeof(ShadowCascadeData) % 16 == 0);
static_assert(sizeof(ShadowGenerationUniform) % 16 == 0);
static_assert(offsetof(ComposePushConstants, active_directionalLight) == 32);
static_assert(offsetof(ComposePushConstants, pointLight) == 48);
static_assert(offsetof(ComposePushConstants, active_pointlight) == 80);
