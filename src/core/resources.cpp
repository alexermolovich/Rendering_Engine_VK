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

