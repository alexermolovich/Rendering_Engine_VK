#pragma once
#include "render_types.hpp"
#include "vulkan_check.hpp"
#include <span>
#include <utility>

/* Explicitly released GPU buffer and its descriptor. */
class Buffer
{
  public:
    Buffer()
    {
    }
    ~Buffer() noexcept(false);
    Buffer(const Buffer &) = delete;
    Buffer &operator=(const Buffer &) = delete;
    void create(VkDevice device, VkPhysicalDevice physical_device, const BufferDesc &desc);
    void destroy(VkDevice device);
    VkDescriptorBufferInfo descInfo = {};
    VkBuffer buffer = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    VkDeviceSize size = 0;
    void write(VkDevice device, const void *pData);
    BufferType type = BufferType::general;
    BufferUsage purpose = BufferUsage::rendering_data;

  private:
    VkMemoryPropertyFlags b_memory_properties = 0;
};

/* Uploaded image; moving transfers handles without releasing them. */
class Image
{
  public:
    Image()
    {
    }
    ~Image() noexcept(false);
    Image(const Image &) = delete;
    Image &operator=(const Image &) = delete;
    Image(Image &&other) noexcept;
    Image &operator=(Image &&other);
    void create(VkDevice device, VkPhysicalDevice physical_device, const TextureDesc &desc);
    void destroy(VkDevice device);
    void from_buffer(unsigned char *buffer, VkDeviceSize size, VkFormat format, uint32_t width,
                     uint32_t height, VkDevice logicalDevice, VkPhysicalDevice physicalDevice,
                     VkQueue copyQueue, VkCommandPool commandPool);
    static uint32_t find_memory_type(VkPhysicalDevice physicalDevice, uint32_t filter,
                                     VkMemoryPropertyFlags properties);
    VkImage image = VK_NULL_HANDLE;
    VkImageView view = VK_NULL_HANDLE;
    VkSampler sampler = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    VkDescriptorImageInfo descInfo = {};
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t mipLevels = 1;
    VkFormat format = VK_FORMAT_UNDEFINED;
    VkImageLayout currentLayout = VK_IMAGE_LAYOUT_UNDEFINED;
};

/* Renderable image with tracked layout and optional array sampling view. */
class RenderTarget
{
  public:
    RenderTarget()
    {
    }
    ~RenderTarget() noexcept(false);
    RenderTarget(const RenderTarget &) = delete;
    RenderTarget &operator=(const RenderTarget &) = delete;
    void create(VkDevice device, VkPhysicalDevice physical_device, const RenderTargetDesc &desc);
    void destroy(VkDevice device);
    VkImage image = VK_NULL_HANDLE;
    VkImageView imageView = VK_NULL_HANDLE;
    VkImageView sampledView = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t depth = 1;
    uint32_t arraySize = 1;
    uint32_t mipLevels = 1;
    VkFormat format = VK_FORMAT_UNDEFINED;
    VkSampleCountFlagBits sampleCount = VK_SAMPLE_COUNT_1_BIT;
    VkClearValue clearValue = {};
    VkImageLayout currentLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    const char *pName = nullptr;
    VkSampler sampler = VK_NULL_HANDLE;
    VkDescriptorImageInfo imageInfo = {};
};

/* Shader modules stay alive until the renderer explicitly releases them. */
class Shader
{
  public:
    Shader()
    {
    }
    ~Shader() noexcept(false);
    Shader(const Shader &) = delete;
    Shader &operator=(const Shader &) = delete;
    void load(VkDevice device, VkShaderStageFlagBits stage, std::span<const uint32_t> code);
    void destroy(VkDevice device);
    VkShaderModule vert = VK_NULL_HANDLE;
    VkShaderModule geom = VK_NULL_HANDLE;
    VkShaderModule frag = VK_NULL_HANDLE;
    VkShaderModule comp = VK_NULL_HANDLE;
};

/* Shared sampling state with an explicit release operation. */
class Sampler
{
  public:
    Sampler()
    {
    }
    ~Sampler() noexcept(false);
    Sampler(const Sampler &) = delete;
    Sampler &operator=(const Sampler &) = delete;
    void create(VkDevice device, const SamplerDesc &desc);
    void destroy(VkDevice device);
    VkDescriptorImageInfo descInfo = {};
    VkSampler sampler = VK_NULL_HANDLE;
};

/* Command buffers are freed before their pool is destroyed. */
class CommandBuffer
{
  public:
    CommandBuffer()
    {
    }
    ~CommandBuffer() noexcept(false);
    CommandBuffer(const CommandBuffer &) = delete;
    CommandBuffer &operator=(const CommandBuffer &) = delete;
    VkCommandBuffer handle() const
    {
        return b_handle;
    }
    void assign(VkCommandBuffer handle);
    void destroy(VkDevice device, VkCommandPool pool);

  private:
    VkCommandBuffer b_handle = VK_NULL_HANDLE;
};
