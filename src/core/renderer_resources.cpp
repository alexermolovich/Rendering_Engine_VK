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
#include <meshoptimizer.h>
#include <random>
#include <stb_image.h>

/* Find memory type. */
uint32_t Renderer::find_memory_type(VkPhysicalDevice b_physical_device, uint32_t typeFilter,
                                    VkMemoryPropertyFlags properties)
{
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(b_physical_device, &memProperties);

    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++)
    {
        if ((typeFilter & (1 << i)) &&
            (memProperties.memoryTypes[i].propertyFlags & properties) == properties)
        {
            return i;
        }
    }

    require(false, "Failed to find suitable memory type!");
    return 0;
}

/* Is depth format. */
bool Renderer::is_depth_format(VkFormat format)
{
    return format == VK_FORMAT_D16_UNORM || format == VK_FORMAT_D32_SFLOAT ||
           format == VK_FORMAT_D16_UNORM_S8_UINT || format == VK_FORMAT_D24_UNORM_S8_UINT ||
           format == VK_FORMAT_D32_SFLOAT_S8_UINT;
}

/* Add semaphore. */
void Renderer::add_semaphore(VkSemaphore &sem)
{
    VkSemaphoreCreateInfo semDesc{};
    semDesc.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkResult result = vkCreateSemaphore(b_device, &semDesc, nullptr, &sem);
    require(result == VK_SUCCESS, "add_semaphore precondition failed");
}

/* Add fence. */
void Renderer::add_fence(VkFence &fence)
{
    VkFenceCreateInfo fenceDesc{};
    fenceDesc.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceDesc.flags = VkFenceCreateFlagBits::VK_FENCE_CREATE_SIGNALED_BIT;
    VkResult result = vkCreateFence(b_device, &fenceDesc, nullptr, &fence);
    require(result == VK_SUCCESS, "add_fence precondition failed");
}

/* Add buffer. */
void Renderer::add_buffer(VkDevice b_device, VkPhysicalDevice b_physical_device, const BufferDesc &desc,
                          Buffer **ppBuffer)
{
    require(ppBuffer, "Resource output pointer is null");
    auto *resource = std::find_if(std::begin(b_buffers), std::end(b_buffers),
                                  [](const Buffer &buffer) { return !buffer.buffer && !buffer.memory; });
    require(resource != std::end(b_buffers), "RENDERER_BUFFER_COUNT capacity exceeded");
    resource->create(b_device, b_physical_device, desc);
    *ppBuffer = resource;
}

/* Remove buffer. */
void Renderer::remove_buffer(VkDevice b_device, Buffer *pBuffer)
{
    if (pBuffer)
        pBuffer->destroy(b_device);
}

/* Add texture. */
void Renderer::add_texture(VkDevice b_device, VkPhysicalDevice b_physical_device, const TextureDesc &desc,
                           Image **ppTexture)
{
    require(ppTexture, "Resource output pointer is null");
    auto *resource = std::find_if(std::begin(b_images), std::end(b_images), [](const Image &image) {
        return !image.image && !image.view && !image.sampler && !image.memory;
    });
    require(resource != std::end(b_images), "RENDERER_TEXTURE_COUNT capacity exceeded");
    resource->create(b_device, b_physical_device, desc);
    *ppTexture = resource;
}

/* Format byte size. */
VkDeviceSize Renderer::format_byte_size(VkFormat format)
{
    switch (format)
    {
    case VK_FORMAT_R8G8B8A8_UNORM:
    case VK_FORMAT_B8G8R8A8_UNORM:
    case VK_FORMAT_R8G8B8A8_SRGB:
        return 4;
    case VK_FORMAT_R16G16B16A16_SFLOAT:
        return 8;
    case VK_FORMAT_R32G32B32A32_SFLOAT:
        return 16;
    case VK_FORMAT_R8_UNORM:
        return 1;
    case VK_FORMAT_R8G8_UNORM:
        return 2;
    default:
        require(false, "formatByteSize: unknown format");
        return 0;
    }
}

/* Map texture data. */
void Renderer::map_texture_data(VkDevice b_device, VkPhysicalDevice b_physical_device, VkCommandPool cmdPool,
                                VkQueue queue, Image *pTexture, const void *pData, VkDeviceSize dataSize)
{
    require(pTexture, "map_texture_data precondition failed");
    require(pData, "map_texture_data precondition failed");

    VkDeviceSize expectedSize =
        (VkDeviceSize)pTexture->width * pTexture->height * format_byte_size(pTexture->format);
    if (dataSize != expectedSize)
    {
        throw std::runtime_error("Texture upload size does not match its dimensions");
    }
    Buffer *pStaging = nullptr;
    BufferDesc stagingDesc;
    stagingDesc.size = dataSize;
    stagingDesc.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    stagingDesc.memoryProperties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    add_buffer(b_device, b_physical_device, stagingDesc, &pStaging);

    void *pMapped = nullptr;
    VK_CHECK(vkMapMemory(b_device, pStaging->memory, 0, dataSize, 0, &pMapped));
    memcpy(pMapped, pData, (size_t)dataSize);
    vkUnmapMemory(b_device, pStaging->memory);
    VkCommandBufferAllocateInfo cmdAlloc = {};
    cmdAlloc.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    cmdAlloc.commandPool = cmdPool;
    cmdAlloc.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cmdAlloc.commandBufferCount = 1;

    VkCommandBuffer cmd = VK_NULL_HANDLE;
    VK_CHECK(vkAllocateCommandBuffers(b_device, &cmdAlloc, &cmd));

    VkCommandBufferBeginInfo beginInfo = {};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    VK_CHECK(vkBeginCommandBuffer(cmd, &beginInfo));
    VkImageMemoryBarrier barrier = {};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = pTexture->image;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = pTexture->mipLevels;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;
    barrier.srcAccessMask = 0;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0,
                         nullptr, 0, nullptr, 1, &barrier);
    VkBufferImageCopy region = {};
    region.bufferOffset = 0;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;
    region.imageExtent = {pTexture->width, pTexture->height, 1};
    vkCmdCopyBufferToImage(cmd, pStaging->buffer, pTexture->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1,
                           &region);
    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0,
                         nullptr, 0, nullptr, 1, &barrier);

    VK_CHECK(vkEndCommandBuffer(cmd));

    VkSubmitInfo submit = {};
    submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &cmd;
    VK_CHECK(vkQueueSubmit(queue, 1, &submit, VK_NULL_HANDLE));
    VK_CHECK(vkQueueWaitIdle(queue));
    pTexture->currentLayout = barrier.newLayout;

    vkFreeCommandBuffers(b_device, cmdPool, 1, &cmd);
    remove_buffer(b_device, pStaging);
}

/* Lerp. */
float Renderer::lerp(float a, float b, float f)
{
    return a + f * (b - a);
}

/* Generate ssao kernel. */
void Renderer::generate_ssao_kernel(bool deterministicSeed)
{
    std::default_random_engine rndEngine(deterministicSeed ? 0 : (unsigned)time(nullptr));
    std::uniform_real_distribution<float> rndDist(0.0f, 1.0f);

    for (uint32_t i = 0; i < SSAO_KERNEL_SIZE; ++i)
    {
        glm::vec3 sample(rndDist(rndEngine) * 2.0f - 1.0f, rndDist(rndEngine) * 2.0f - 1.0f,
                         rndDist(rndEngine));
        sample = glm::normalize(sample);
        sample *= rndDist(rndEngine);

        float scale = float(i) / float(SSAO_KERNEL_SIZE);
        scale = lerp(0.1f, 1.0f, scale * scale);

        b_ssao_kernels[i] = glm::vec4(sample * scale, 0.0f);
    }
}

/* Allocate engine uniforms shared by the techniques. */
void Renderer::add_buffers()
{
    for (uint32_t frame = 0; frame < FRAMES_IN_FLIGHT; ++frame)
    {
        BufferDesc desc;
        desc.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
        desc.memoryProperties = VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
        desc.size = sizeof(Uniform);
        add_buffer(b_device, b_physical_device, desc, &b_ubo_buffer[frame]);
        map_buffer(b_device, b_ubo_buffer[frame], &b_uniform_data);
        desc.size = sizeof(SkyLightUniform);
        add_buffer(b_device, b_physical_device, desc, &b_sky_lights_data_buffer[frame]);
        map_buffer(b_device, b_sky_lights_data_buffer[frame], &b_sky_light_uniform_data);
    }
}

/* Add sampler. */
void Renderer::add_sampler(VkDevice b_device, const SamplerDesc &desc, Sampler **ppSampler)
{
    require(ppSampler, "Resource output pointer is null");
    auto *resource = std::find_if(std::begin(b_samplers), std::end(b_samplers),
                                  [](const Sampler &sampler) { return !sampler.sampler; });
    require(resource != std::end(b_samplers), "RENDERER_SAMPLER_COUNT capacity exceeded");
    resource->create(b_device, desc);
    *ppSampler = resource;
}

/* Allocate the shared fallback mesh texture. */
void Renderer::add_textures()
{
    const uint8_t whitePixel[4] = {255, 255, 255, 255};
    TextureDesc desc;
    desc.width = 1;
    desc.height = 1;
    desc.channels = 4;
    desc.format = VK_FORMAT_R8G8B8A8_UNORM;
    desc.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    add_texture(b_device, b_physical_device, desc, &b_default_mesh_texture);
    map_texture_data(b_device, b_physical_device, b_graphics_command_pool, b_graphics_queue,
                     b_default_mesh_texture, whitePixel, sizeof(whitePixel));
}

/* Generate ssao noise. */
void Renderer::generate_ssao_noise()
{
    std::default_random_engine rndEngine(std::random_device{}());
    std::uniform_real_distribution<float> rndDist(0.0f, 1.0f);

    for (auto &n : b_ssao_noise)
    {
        n = glm::vec4(rndDist(rndEngine) * 2.0f - 1.0f, rndDist(rndEngine) * 2.0f - 1.0f, 0.0f, 0.0f);
    }
}

/* Map buffer. */
void Renderer::map_buffer(VkDevice b_device, Buffer *pBuffer, const void *pData)
{
    require(pBuffer, "Buffer pointer is null");
    pBuffer->write(b_device, pData);
}

/* Add samplers. */
void Renderer::add_samplers()
{
    {
        SamplerDesc textureSamplerDesc;
        textureSamplerDesc.magFilter = VK_FILTER_LINEAR;
        textureSamplerDesc.minFilter = VK_FILTER_LINEAR;
        textureSamplerDesc.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        textureSamplerDesc.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        textureSamplerDesc.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        textureSamplerDesc.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        textureSamplerDesc.anisotropyEnable = VK_TRUE;
        textureSamplerDesc.maxAnisotropy = 16.0f;
        textureSamplerDesc.minLod = 0.0f;
        textureSamplerDesc.maxLod = VK_LOD_CLAMP_NONE;
        textureSamplerDesc.borderColor = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK;
        add_sampler(b_device, textureSamplerDesc, &b_texture_sampler);
    }
    {
        SamplerDesc renderTargetSamplerDesc;
        renderTargetSamplerDesc.magFilter = VK_FILTER_LINEAR;
        renderTargetSamplerDesc.minFilter = VK_FILTER_LINEAR;
        renderTargetSamplerDesc.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
        renderTargetSamplerDesc.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        renderTargetSamplerDesc.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        renderTargetSamplerDesc.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        renderTargetSamplerDesc.anisotropyEnable = VK_FALSE;
        renderTargetSamplerDesc.maxAnisotropy = 1.0f;
        renderTargetSamplerDesc.minLod = 0.0f;
        renderTargetSamplerDesc.maxLod = 0.0f; // single mip, no LOD
        renderTargetSamplerDesc.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK;
        add_sampler(b_device, renderTargetSamplerDesc, &b_render_target_sampler);
    }
}

/* Add render target. */
void Renderer::add_render_target(VkDevice b_device, VkPhysicalDevice b_physical_device,
                                 const RenderTargetDesc *pDesc, RenderTarget **ppRenderTarget)
{
    require(ppRenderTarget, "Resource output pointer is null");
    require(pDesc, "Render target description is null");
    auto *resource = std::find_if(std::begin(b_render_targets), std::end(b_render_targets),
                                  [](const RenderTarget &target) {
                                      return !target.image && !target.imageView && !target.sampledView &&
                                             !target.memory && !target.sampler;
                                  });
    require(resource != std::end(b_render_targets), "RENDERER_RENDER_TARGET_COUNT capacity exceeded");
    resource->create(b_device, b_physical_device, *pDesc);
    *ppRenderTarget = resource;
}

/* Add shadow cascade layer views. */
void Renderer::add_shadow_cascade_layer_views()
{
    RenderTarget *shadowDepth = b_shadow_render_targets.cascadeDepth;
    require(shadowDepth && shadowDepth->image != VK_NULL_HANDLE,
            "add_shadow_cascade_layer_views precondition failed");

    for (uint32_t cascadeIndex = 0; cascadeIndex < MAX_SHADOW_CASCADES; ++cascadeIndex)
    {
        VkImageViewCreateInfo viewInfo = {};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = shadowDepth->image;
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = shadowDepth->format;
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.baseArrayLayer = cascadeIndex;
        viewInfo.subresourceRange.layerCount = 1;

        VK_CHECK(vkCreateImageView(b_device, &viewInfo, nullptr,
                                   &b_shadow_render_targets.cascadeLayerViews[cascadeIndex]));
    }
}

/* Has shadow render targets. */
bool Renderer::has_shadow_render_targets() const
{
    const RenderTarget *shadowDepth = b_shadow_render_targets.cascadeDepth;
    if (!shadowDepth || shadowDepth->image == VK_NULL_HANDLE || shadowDepth->imageView == VK_NULL_HANDLE ||
        shadowDepth->sampledView == VK_NULL_HANDLE)
    {
        return false;
    }

    for (uint32_t cascadeIndex = 0; cascadeIndex < MAX_SHADOW_CASCADES; ++cascadeIndex)
    {
        if (b_shadow_render_targets.cascadeLayerViews[cascadeIndex] == VK_NULL_HANDLE)
        {
            return false;
        }
    }

    return true;
}

/* Load shader code. */
void Renderer::load_shader_code(ShaderResource &shader, VkShaderStageFlagBits stage, const std::string &filepath)
{
    const std::filesystem::path shaderPath = std::filesystem::path(VKML_BINARY_DIR) / filepath;
    std::ifstream file(shaderPath, std::ios::ate | std::ios::binary);

    if (!file.is_open())
    {
        throw std::runtime_error("Failed to open shader file: " + shaderPath.string() +
                                 ". Build the shaders with: cmake --build \"" + VKML_BINARY_DIR +
                                 "\" --target shaders");
    }
    const auto end = file.tellg();
    require(end > 0 && static_cast<size_t>(end) % sizeof(uint32_t) == 0,
            "Invalid SPIR-V byte count: " + shaderPath.string());
    size_t fileSize = static_cast<size_t>(end);
    const size_t wordCount = fileSize / sizeof(uint32_t);
    auto *buffer = new uint32_t[wordCount];
    try
    {
        file.seekg(0);
        require(static_cast<bool>(file.read(reinterpret_cast<char *>(buffer), fileSize)),
                "Failed to read shader: " + shaderPath.string());
        shader.load(b_device, stage, {buffer, wordCount});
    }
    catch (...)
    {
        delete[] buffer;
        throw;
    }
    delete[] buffer;
}

/* Create command pool. */
void Renderer::create_command_pool()
{
    VkCommandPoolCreateInfo commandPoolDesc;
    commandPoolDesc.flags = VkCommandPoolCreateFlagBits::VK_COMMAND_POOL_CREATE_TRANSIENT_BIT |
                            VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    commandPoolDesc.pNext = nullptr;
    commandPoolDesc.queueFamilyIndex = b_indecies.graphicsFamily.value();
    commandPoolDesc.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    VK_CHECK(vkCreateCommandPool(b_device, &commandPoolDesc, nullptr, &b_graphics_command_pool));
}

/* Create command buffer. */
void Renderer::create_command_buffer()
{
    VkCommandBufferAllocateInfo commandBufferDesc;
    commandBufferDesc.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    commandBufferDesc.commandBufferCount = COMMAND_BUFFER_COUNT;
    commandBufferDesc.commandPool = b_graphics_command_pool;
    commandBufferDesc.level = VkCommandBufferLevel::VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    commandBufferDesc.pNext = nullptr;

    VkCommandBuffer handles[COMMAND_BUFFER_COUNT] = {};
    VK_CHECK(vkAllocateCommandBuffers(b_device, &commandBufferDesc, handles));
    for (uint32_t i = 0; i < COMMAND_BUFFER_COUNT; ++i)
        b_command_buffers[i].assign(handles[i]);
}
