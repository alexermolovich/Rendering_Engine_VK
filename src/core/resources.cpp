#include "resources.hpp"
#include <cstring>

Buffer::~Buffer() noexcept(false)
{
    require_destroyed(!buffer && !memory, "Buffer");
}

/* Release the allocation before deleting the wrapper. */
void Buffer::destroy(VkDevice device)
{
    if (buffer)
        vkDestroyBuffer(device, std::exchange(buffer, VK_NULL_HANDLE), nullptr);
    if (memory)
        vkFreeMemory(device, std::exchange(memory, VK_NULL_HANDLE), nullptr);
    descInfo = {};
}

Image::~Image() noexcept(false)
{
    require_destroyed(!image && !view && !sampler && !memory, "Image");
}

Image::Image(Image &&other) noexcept
    : image(std::exchange(other.image, VK_NULL_HANDLE)), view(std::exchange(other.view, VK_NULL_HANDLE)),
      sampler(std::exchange(other.sampler, VK_NULL_HANDLE)),
      memory(std::exchange(other.memory, VK_NULL_HANDLE)), descInfo(std::exchange(other.descInfo, {})),
      width(other.width), height(other.height), mipLevels(other.mipLevels), format(other.format),
      currentLayout(std::exchange(other.currentLayout, VK_IMAGE_LAYOUT_UNDEFINED))
{
}

Image &Image::operator=(Image &&other)
{
    if (this == &other)
        return *this;
    require_destroyed(!image && !view && !sampler && !memory, "Image move destination");
    image = std::exchange(other.image, VK_NULL_HANDLE);
    view = std::exchange(other.view, VK_NULL_HANDLE);
    sampler = std::exchange(other.sampler, VK_NULL_HANDLE);
    memory = std::exchange(other.memory, VK_NULL_HANDLE);
    descInfo = std::exchange(other.descInfo, {});
    width = other.width;
    height = other.height;
    mipLevels = other.mipLevels;
    format = other.format;
    currentLayout = std::exchange(other.currentLayout, VK_IMAGE_LAYOUT_UNDEFINED);
    return *this;
}

/* Destroy image views before their image and allocation. */
void Image::destroy(VkDevice device)
{
    if (view)
        vkDestroyImageView(device, std::exchange(view, VK_NULL_HANDLE), nullptr);
    if (sampler)
        vkDestroySampler(device, std::exchange(sampler, VK_NULL_HANDLE), nullptr);
    if (image)
        vkDestroyImage(device, std::exchange(image, VK_NULL_HANDLE), nullptr);
    if (memory)
        vkFreeMemory(device, std::exchange(memory, VK_NULL_HANDLE), nullptr);
    descInfo = {};
    currentLayout = VK_IMAGE_LAYOUT_UNDEFINED;
}

RenderTarget::~RenderTarget() noexcept(false)
{
    require_destroyed(!image && !imageView && !sampledView && !memory && !sampler, "RenderTarget");
}

/* Release target views before the image they reference. */
void RenderTarget::destroy(VkDevice device)
{
    if (sampledView)
        vkDestroyImageView(device, std::exchange(sampledView, VK_NULL_HANDLE), nullptr);
    if (imageView)
        vkDestroyImageView(device, std::exchange(imageView, VK_NULL_HANDLE), nullptr);
    if (sampler)
        vkDestroySampler(device, std::exchange(sampler, VK_NULL_HANDLE), nullptr);
    if (image)
        vkDestroyImage(device, std::exchange(image, VK_NULL_HANDLE), nullptr);
    if (memory)
        vkFreeMemory(device, std::exchange(memory, VK_NULL_HANDLE), nullptr);
    imageInfo = {};
    currentLayout = VK_IMAGE_LAYOUT_UNDEFINED;
}

Shader::~Shader() noexcept(false)
{
    require_destroyed(!vert && !geom && !frag && !comp, "Shader");
}

/* Pipeline creation no longer needs these modules after shutdown begins. */
void Shader::destroy(VkDevice device)
{
    for (auto *module : {&vert, &geom, &frag, &comp})
        if (*module)
            vkDestroyShaderModule(device, std::exchange(*module, VK_NULL_HANDLE), nullptr);
}

RayTracingShader::~RayTracingShader() noexcept(false)
{
    require_destroyed(!raygen && !miss && !closestHit && !anyHit && !intersection && !callable,
                      "RayTracingShader");
}

/* Release every loaded ray-tracing stage, including partially loaded programs. */
void RayTracingShader::destroy(VkDevice device)
{
    for (auto *module : {&raygen, &miss, &closestHit, &anyHit, &intersection, &callable})
        if (*module)
            vkDestroyShaderModule(device, std::exchange(*module, VK_NULL_HANDLE), nullptr);
}

Sampler::~Sampler() noexcept(false)
{
    require_destroyed(!sampler, "Sampler");
}

/* Samplers may be shared by descriptors until device work has finished. */
void Sampler::destroy(VkDevice device)
{
    if (sampler)
        vkDestroySampler(device, std::exchange(sampler, VK_NULL_HANDLE), nullptr);
    descInfo = {};
}

CommandBuffer::~CommandBuffer() noexcept(false)
{
    require_destroyed(!b_handle, "CommandBuffer");
}

void CommandBuffer::assign(VkCommandBuffer handle)
{
    require_destroyed(!b_handle, "CommandBuffer assignment");
    b_handle = handle;
}

/* Free recorded commands while their pool is still alive. */
void CommandBuffer::destroy(VkDevice device, VkCommandPool pool)
{
    if (b_handle)
        vkFreeCommandBuffers(device, pool, 1, &b_handle);
    b_handle = VK_NULL_HANDLE;
}
uint32_t Image::find_memory_type(VkPhysicalDevice physicalDevice, uint32_t typeFilter,
                                 VkMemoryPropertyFlags properties)
{
    VkPhysicalDeviceMemoryProperties memProps;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProps);
    for (uint32_t i = 0; i < memProps.memoryTypeCount; i++)
        if ((typeFilter & (1 << i)) && (memProps.memoryTypes[i].propertyFlags & properties) == properties)
            return i;
    throw std::runtime_error("Failed to find suitable memory type");
}

void Image::from_buffer(unsigned char *buffer, VkDeviceSize size, VkFormat format, uint32_t width,
                        uint32_t height, VkDevice logicalDevice, VkPhysicalDevice physicalDevice,
                        VkQueue copyQueue, VkCommandPool commandPool)
{
    require_destroyed(!image && !view && !memory && !sampler, "Image upload destination");
    VkBuffer stagingBuffer = VK_NULL_HANDLE;
    VkDeviceMemory stagingMemory = VK_NULL_HANDLE;
    VkCommandBuffer cmdBuffer = VK_NULL_HANDLE;
    VkFence fence = VK_NULL_HANDLE;
    bool submitted = false;
    auto cleanup = [&] {
        if (submitted)
            vkQueueWaitIdle(copyQueue);
        submitted = false;
        if (fence)
            vkDestroyFence(logicalDevice, std::exchange(fence, VK_NULL_HANDLE), nullptr);
        if (cmdBuffer)
            vkFreeCommandBuffers(logicalDevice, commandPool, 1, &cmdBuffer);
        cmdBuffer = VK_NULL_HANDLE;
        if (stagingBuffer)
            vkDestroyBuffer(logicalDevice, std::exchange(stagingBuffer, VK_NULL_HANDLE), nullptr);
        if (stagingMemory)
            vkFreeMemory(logicalDevice, std::exchange(stagingMemory, VK_NULL_HANDLE), nullptr);
    };
    try
    {

        this->width = width;
        this->height = height;
        this->mipLevels = 1;
        this->format = format;

        VkBufferCreateInfo bufferInfo{};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = size;
        bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        VK_CHECK(vkCreateBuffer(logicalDevice, &bufferInfo, nullptr, &stagingBuffer));

        VkMemoryRequirements memReqs;
        vkGetBufferMemoryRequirements(logicalDevice, stagingBuffer, &memReqs);

        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memReqs.size;
        allocInfo.memoryTypeIndex =
            find_memory_type(physicalDevice, memReqs.memoryTypeBits,
                             VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

        VK_CHECK(vkAllocateMemory(logicalDevice, &allocInfo, nullptr, &stagingMemory));
        VK_CHECK(vkBindBufferMemory(logicalDevice, stagingBuffer, stagingMemory, 0));

        void *mapped;
        VK_CHECK(vkMapMemory(logicalDevice, stagingMemory, 0, size, 0, &mapped));
        memcpy(mapped, buffer, static_cast<size_t>(size));
        vkUnmapMemory(logicalDevice, stagingMemory);

        VkImageCreateInfo imageDesc{};
        imageDesc.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageDesc.imageType = VK_IMAGE_TYPE_2D;
        imageDesc.format = format;
        imageDesc.extent = {width, height, 1};
        imageDesc.mipLevels = 1;
        imageDesc.arrayLayers = 1;
        imageDesc.samples = VK_SAMPLE_COUNT_1_BIT;
        imageDesc.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageDesc.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        imageDesc.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        imageDesc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        VK_CHECK(vkCreateImage(logicalDevice, &imageDesc, nullptr, &image));
        currentLayout = imageDesc.initialLayout;

        VkMemoryRequirements imgMemReqs;
        vkGetImageMemoryRequirements(logicalDevice, image, &imgMemReqs);

        VkMemoryAllocateInfo imgAllocInfo{};
        imgAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        imgAllocInfo.allocationSize = imgMemReqs.size;
        imgAllocInfo.memoryTypeIndex =
            find_memory_type(physicalDevice, imgMemReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        VK_CHECK(vkAllocateMemory(logicalDevice, &imgAllocInfo, nullptr, &memory));
        VK_CHECK(vkBindImageMemory(logicalDevice, image, memory, 0));

        VkCommandBufferAllocateInfo cmdAllocInfo{};
        cmdAllocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        cmdAllocInfo.commandPool = commandPool;
        cmdAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        cmdAllocInfo.commandBufferCount = 1;

        VK_CHECK(vkAllocateCommandBuffers(logicalDevice, &cmdAllocInfo, &cmdBuffer));

        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        VK_CHECK(vkBeginCommandBuffer(cmdBuffer, &beginInfo));

        VkImageMemoryBarrier barrierToTransfer{};
        barrierToTransfer.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrierToTransfer.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        barrierToTransfer.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrierToTransfer.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrierToTransfer.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrierToTransfer.image = image;
        barrierToTransfer.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        barrierToTransfer.srcAccessMask = 0;
        barrierToTransfer.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

        vkCmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0,
                             0, nullptr, 0, nullptr, 1, &barrierToTransfer);

        VkBufferImageCopy2 buffer2image_desc{};
        buffer2image_desc.sType = VK_STRUCTURE_TYPE_BUFFER_IMAGE_COPY_2;
        buffer2image_desc.bufferOffset = 0;
        buffer2image_desc.bufferRowLength = 0;
        buffer2image_desc.bufferImageHeight = 0;
        buffer2image_desc.imageExtent = {width, height, 1};
        buffer2image_desc.imageOffset = {0, 0, 0};
        buffer2image_desc.imageSubresource = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .mipLevel = 0, .baseArrayLayer = 0, .layerCount = 1};

        VkCopyBufferToImageInfo2 copyInfo{};
        copyInfo.sType = VK_STRUCTURE_TYPE_COPY_BUFFER_TO_IMAGE_INFO_2;
        copyInfo.srcBuffer = stagingBuffer;
        copyInfo.dstImage = image;
        copyInfo.dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        copyInfo.regionCount = 1;
        copyInfo.pRegions = &buffer2image_desc;

        vkCmdCopyBufferToImage2(cmdBuffer, &copyInfo);

        VkImageMemoryBarrier barrierToShader{};
        barrierToShader.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrierToShader.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrierToShader.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        barrierToShader.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrierToShader.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrierToShader.image = image;
        barrierToShader.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        barrierToShader.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrierToShader.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        vkCmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                             0, 0, nullptr, 0, nullptr, 1, &barrierToShader);

        VK_CHECK(vkEndCommandBuffer(cmdBuffer));

        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &cmdBuffer;

        VkFenceCreateInfo fenceInfo{};
        fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;

        VK_CHECK(vkCreateFence(logicalDevice, &fenceInfo, nullptr, &fence));

        VK_CHECK(vkQueueSubmit(copyQueue, 1, &submitInfo, fence));
        submitted = true;
        VK_CHECK(vkWaitForFences(logicalDevice, 1, &fence, VK_TRUE, UINT64_MAX));
        currentLayout = barrierToShader.newLayout;

        cleanup();

        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = image;
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = format;
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = 1;
        VK_CHECK(vkCreateImageView(logicalDevice, &viewInfo, nullptr, &view));

        descInfo.imageView = view;
        descInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        descInfo.sampler = VK_NULL_HANDLE;
    }
    catch (...)
    {
        cleanup();
        destroy(logicalDevice);
        throw;
    }
}

/* Allocate this resource; failed creation rolls back its Vulkan handles. */
void Buffer::create(VkDevice device, VkPhysicalDevice physical_device, const BufferDesc &desc)
{
    require_destroyed(!buffer && !memory, "Buffer creation destination");
    try
    {

        require(desc.size > 0, "add_buffer precondition failed");

        VkBufferCreateInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        info.size = desc.size;
        info.usage = desc.usage | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        info.sharingMode = desc.sharingMode;

        this->size = desc.size;
        this->type = (desc.usage & VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT)   ? BufferType::uniform
                     : (desc.usage & VK_BUFFER_USAGE_STORAGE_BUFFER_BIT) ? BufferType::storage
                                                                         : desc.type;
        this->purpose = (desc.usage & VK_BUFFER_USAGE_VERTEX_BUFFER_BIT)     ? BufferUsage::vertex_data
                        : (desc.usage & VK_BUFFER_USAGE_INDEX_BUFFER_BIT)    ? BufferUsage::index_data
                        : (desc.usage & VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT) ? BufferUsage::rendering_output
                        : (desc.usage & VK_BUFFER_USAGE_STORAGE_BUFFER_BIT)  ? BufferUsage::compute_data
                        : (desc.usage & VK_BUFFER_USAGE_TRANSFER_SRC_BIT)    ? BufferUsage::staging
                                                                             : desc.purpose;

        VK_CHECK(vkCreateBuffer(device, &info, nullptr, &this->buffer));
        VkMemoryRequirements memReqs = {};
        vkGetBufferMemoryRequirements(device, this->buffer, &memReqs);

        VkPhysicalDeviceMemoryProperties memProps = {};
        vkGetPhysicalDeviceMemoryProperties(physical_device, &memProps);

        uint32_t memTypeIndex = UINT32_MAX;
        for (uint32_t i = 0; i < memProps.memoryTypeCount; ++i)
        {
            if ((memReqs.memoryTypeBits & (1 << i)) &&
                (memProps.memoryTypes[i].propertyFlags & desc.memoryProperties) == desc.memoryProperties)
            {
                memTypeIndex = i;
                break;
            }
        }
        require(memTypeIndex != UINT32_MAX, "No suitable memory type found");
        b_memory_properties = memProps.memoryTypes[memTypeIndex].propertyFlags;

        VkMemoryAllocateInfo allocInfo = {};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memReqs.size;
        allocInfo.memoryTypeIndex = memTypeIndex;

        VkMemoryAllocateFlagsInfo allocFlagsInfo = {};
        if (desc.usage & VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT)
        {
            allocFlagsInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO;
            allocFlagsInfo.flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT;
            allocInfo.pNext = &allocFlagsInfo;
        }

        VK_CHECK(vkAllocateMemory(device, &allocInfo, nullptr, &this->memory));
        VK_CHECK(vkBindBufferMemory(device, this->buffer, this->memory, 0));

        this->descInfo.buffer = this->buffer;
        this->descInfo.offset = 0;
        this->descInfo.range = desc.size;
    }
    catch (...)
    {
        destroy(device);
        throw;
    }
}

/* Allocate this resource; failed creation rolls back its Vulkan handles. */
void Image::create(VkDevice device, VkPhysicalDevice physical_device, const TextureDesc &desc)
{
    require_destroyed(!image && !view && !memory && !sampler, "Image creation destination");
    try
    {

        require(desc.width > 0 && desc.height > 0, "add_texture precondition failed");

        this->width = desc.width;
        this->height = desc.height;
        this->mipLevels = desc.mipLevels;
        this->format = desc.format;
        VkImageCreateInfo imageInfo = {};
        imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.imageType = VK_IMAGE_TYPE_2D;
        imageInfo.format = desc.format;
        imageInfo.extent = {(uint32_t)desc.width, (uint32_t)desc.height, 1};
        imageInfo.mipLevels = desc.mipLevels;
        imageInfo.arrayLayers = 1;
        imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageInfo.usage = desc.usage;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

        VK_CHECK(vkCreateImage(device, &imageInfo, nullptr, &this->image));
        currentLayout = imageInfo.initialLayout;
        VkMemoryRequirements memReqs = {};
        vkGetImageMemoryRequirements(device, this->image, &memReqs);

        VkPhysicalDeviceMemoryProperties memProps = {};
        vkGetPhysicalDeviceMemoryProperties(physical_device, &memProps);

        uint32_t memTypeIndex = UINT32_MAX;
        for (uint32_t i = 0; i < memProps.memoryTypeCount; ++i)
        {
            if ((memReqs.memoryTypeBits & (1 << i)) &&
                (memProps.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT))
            {
                memTypeIndex = i;
                break;
            }
        }
        require(memTypeIndex != UINT32_MAX, "add_texture precondition failed");

        VkMemoryAllocateInfo allocInfo = {};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memReqs.size;
        allocInfo.memoryTypeIndex = memTypeIndex;
        VK_CHECK(vkAllocateMemory(device, &allocInfo, nullptr, &this->memory));
        VK_CHECK(vkBindImageMemory(device, this->image, this->memory, 0));
        VkImageViewCreateInfo viewInfo = {};

        VkImageAspectFlags aspect = VK_IMAGE_ASPECT_COLOR_BIT;
        if (desc.format == VK_FORMAT_D32_SFLOAT || desc.format == VK_FORMAT_D16_UNORM)
        {
            aspect = VK_IMAGE_ASPECT_DEPTH_BIT;
        }
        else if (desc.format == VK_FORMAT_D24_UNORM_S8_UINT || desc.format == VK_FORMAT_D32_SFLOAT_S8_UINT)
        {
            aspect = VK_IMAGE_ASPECT_DEPTH_BIT;
        }
        viewInfo.subresourceRange.aspectMask = aspect;

        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = this->image;
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = desc.format;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = desc.mipLevels;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = 1;
        VK_CHECK(vkCreateImageView(device, &viewInfo, nullptr, &this->view));
        this->descInfo.imageView = this->view;
        this->descInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        this->descInfo.sampler = VK_NULL_HANDLE; // set externally from your sampler
    }
    catch (...)
    {
        destroy(device);
        throw;
    }
}

/* Allocate this resource; failed creation rolls back its Vulkan handles. */
void Sampler::create(VkDevice device, const SamplerDesc &desc)
{
    require_destroyed(!sampler, "Sampler creation destination");
    try
    {

        VkSamplerCreateInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        info.magFilter = desc.magFilter;
        info.minFilter = desc.minFilter;
        info.mipmapMode = desc.mipmapMode;
        info.addressModeU = desc.addressModeU;
        info.addressModeV = desc.addressModeV;
        info.addressModeW = desc.addressModeW;
        info.mipLodBias = desc.mipLodBias;
        info.anisotropyEnable = desc.anisotropyEnable;
        info.maxAnisotropy = desc.maxAnisotropy;
        info.compareEnable = desc.compareEnable;
        info.compareOp = desc.compareOp;
        info.minLod = desc.minLod;
        info.maxLod = desc.maxLod;
        info.borderColor = desc.borderColor;
        info.unnormalizedCoordinates = desc.unnormalizedCoordinates;

        VK_CHECK(vkCreateSampler(device, &info, nullptr, &this->sampler));

        this->descInfo.sampler = this->sampler;
        this->descInfo.imageView = VK_NULL_HANDLE;
        this->descInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    }
    catch (...)
    {
        destroy(device);
        throw;
    }
}

/* Allocate this resource; failed creation rolls back its Vulkan handles. */
void RenderTarget::create(VkDevice device, VkPhysicalDevice physical_device, const RenderTargetDesc &desc)
{
    require_destroyed(!image && !imageView && !sampledView && !memory && !sampler,
                      "RenderTarget creation destination");
    try
    {

        this->width = desc.width;
        this->height = desc.height;
        this->depth = desc.depth;
        this->arraySize = desc.arraySize;
        this->mipLevels = desc.mipLevels;
        this->format = desc.format;
        this->sampleCount = desc.sampleCount;
        this->clearValue = desc.clearValue;
        this->currentLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        this->pName = desc.pName;

        VkImageCreateInfo imageInfo = {};
        imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.imageType = desc.depth > 1 ? VK_IMAGE_TYPE_3D : VK_IMAGE_TYPE_2D;
        imageInfo.format = desc.format;
        imageInfo.extent.width = desc.width;
        imageInfo.extent.height = desc.height;
        imageInfo.extent.depth = desc.depth;
        imageInfo.mipLevels = desc.mipLevels;
        imageInfo.arrayLayers = desc.arraySize;
        imageInfo.samples = desc.sampleCount;
        imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageInfo.usage = desc.usageFlags;
        imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

        VK_CHECK(vkCreateImage(device, &imageInfo, nullptr, &this->image));

        VkMemoryRequirements memReqs;
        vkGetImageMemoryRequirements(device, this->image, &memReqs);

        VkMemoryAllocateInfo allocInfo = {};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memReqs.size;
        allocInfo.memoryTypeIndex = Image::find_memory_type(physical_device, memReqs.memoryTypeBits,
                                                            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        VK_CHECK(vkAllocateMemory(device, &allocInfo, nullptr, &this->memory));
        VK_CHECK(vkBindImageMemory(device, this->image, this->memory, 0));

        VkImageViewCreateInfo viewInfo = {};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = this->image;
        viewInfo.viewType = desc.arraySize > 1 ? VK_IMAGE_VIEW_TYPE_2D_ARRAY : VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = desc.format;
        viewInfo.subresourceRange.aspectMask =
            (desc.format == VK_FORMAT_D16_UNORM || desc.format == VK_FORMAT_D32_SFLOAT ||
             desc.format == VK_FORMAT_D16_UNORM_S8_UINT || desc.format == VK_FORMAT_D24_UNORM_S8_UINT ||
             desc.format == VK_FORMAT_D32_SFLOAT_S8_UINT)
                ? VK_IMAGE_ASPECT_DEPTH_BIT
                : VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = desc.mipLevels;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = desc.arraySize;

        VK_CHECK(vkCreateImageView(device, &viewInfo, nullptr, &this->imageView));

        if (desc.usageFlags & VK_IMAGE_USAGE_SAMPLED_BIT)
        {
            VK_CHECK(vkCreateImageView(device, &viewInfo, nullptr, &this->sampledView));
        }

        VkImageLayout descriptorLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        this->imageInfo.imageLayout = descriptorLayout;
        this->imageInfo.imageView = this->sampledView != VK_NULL_HANDLE ? this->sampledView : this->imageView;
        this->imageInfo.sampler = VK_NULL_HANDLE; // set externally if COMBINED_IMAGE_SAMPLER
    }
    catch (...)
    {
        destroy(device);
        throw;
    }
}

/* Upload mapped data and flush allocations that are not host coherent. */
void Buffer::write(VkDevice device, const void *pData)
{
    require(buffer && memory && size, "Buffer write requires a live allocation");
    require(b_memory_properties & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
            "Buffer allocation is not host visible");

    require(pData, "map_buffer precondition failed");

    void *pMapped = nullptr;
    VK_CHECK(vkMapMemory(device, this->memory, 0, VK_WHOLE_SIZE, 0, &pMapped));
    memcpy(pMapped, pData, (size_t)this->size);
    VkResult flush = VK_SUCCESS;
    if (!(b_memory_properties & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT))
    {
        VkMappedMemoryRange range = {};
        range.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
        range.memory = memory;
        range.size = VK_WHOLE_SIZE;
        flush = vkFlushMappedMemoryRanges(device, 1, &range);
    }
    vkUnmapMemory(device, this->memory);
    VK_CHECK(flush);
}

/* Keep each stage separate so partially loaded programs can be explicitly released. */
void Shader::load(VkDevice device, VkShaderStageFlagBits stage, std::span<const uint32_t> code)
{
    require(!code.empty(), "Shader code is empty");
    VkShaderModule *module = nullptr;
    switch (stage)
    {
    case VK_SHADER_STAGE_VERTEX_BIT:
        module = &vert;
        break;
    case VK_SHADER_STAGE_FRAGMENT_BIT:
        module = &frag;
        break;
    case VK_SHADER_STAGE_GEOMETRY_BIT:
        module = &geom;
        break;
    case VK_SHADER_STAGE_COMPUTE_BIT:
        module = &comp;
        break;
    default:
        throw std::runtime_error("Unsupported shader stage");
    }
    require_destroyed(!*module, "Shader stage replacement");
    VkShaderModuleCreateInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    info.codeSize = code.size() * sizeof(uint32_t);
    info.pCode = code.data();
    VK_CHECK(vkCreateShaderModule(device, &info, nullptr, module));
}

/* Reject raster and compute stages before creating a ray-tracing module. */
void RayTracingShader::load(VkDevice device, VkShaderStageFlagBits stage,
                           std::span<const uint32_t> code)
{
    require(!code.empty(), "Ray tracing shader code is empty");
    VkShaderModule *module = nullptr;
    switch (stage)
    {
    case VK_SHADER_STAGE_RAYGEN_BIT_KHR:
        module = &raygen;
        break;
    case VK_SHADER_STAGE_MISS_BIT_KHR:
        module = &miss;
        break;
    case VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR:
        module = &closestHit;
        break;
    case VK_SHADER_STAGE_ANY_HIT_BIT_KHR:
        module = &anyHit;
        break;
    case VK_SHADER_STAGE_INTERSECTION_BIT_KHR:
        module = &intersection;
        break;
    case VK_SHADER_STAGE_CALLABLE_BIT_KHR:
        module = &callable;
        break;
    default:
        throw std::runtime_error("Unsupported ray tracing shader stage");
    }
    require_destroyed(!*module, "Ray tracing shader stage replacement");
    VkShaderModuleCreateInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    info.codeSize = code.size() * sizeof(uint32_t);
    info.pCode = code.data();
    VK_CHECK(vkCreateShaderModule(device, &info, nullptr, module));
}
