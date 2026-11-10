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

/* Add descriptor sets. */
void Renderer::add_descriptor_sets()
{

    VkDescriptorPoolCreateInfo poolInfo = {};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.pNext = nullptr;
    poolInfo.flags = 0; // see flags explanation below

    VkDescriptorPoolSize poolSizes[] = {
        {.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, .descriptorCount = 3 * FRAMES_IN_FLIGHT + 1},

        {.type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, .descriptorCount = 9 + BISTRO_TEXTURE_COUNT},

        {.type = VK_DESCRIPTOR_TYPE_SAMPLER, .descriptorCount = 3},
    };

    poolInfo.maxSets = FRAMES_IN_FLIGHT + 1;
    poolInfo.poolSizeCount = static_cast<uint32_t>(std::size(poolSizes));
    poolInfo.pPoolSizes = poolSizes;

    VK_CHECK(vkCreateDescriptorPool(b_device, &poolInfo, nullptr, &b_descriptor_pool));

    {
        VkDescriptorSetLayoutBinding perFrameBinding[PER_FRAME_BINDING_COUNT] = {};
        perFrameBinding[0].binding = 0;
        perFrameBinding[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        perFrameBinding[0].descriptorCount = 1;
        perFrameBinding[0].stageFlags = VK_SHADER_STAGE_ALL_GRAPHICS | VK_SHADER_STAGE_COMPUTE_BIT;
        perFrameBinding[0].pImmutableSamplers = nullptr;
        perFrameBinding[1].binding = 1;
        perFrameBinding[1].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        perFrameBinding[1].descriptorCount = 1;
        perFrameBinding[1].stageFlags = VK_SHADER_STAGE_ALL_GRAPHICS | VK_SHADER_STAGE_COMPUTE_BIT;
        perFrameBinding[1].pImmutableSamplers = nullptr;

        perFrameBinding[2].binding = 2;
        perFrameBinding[2].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        perFrameBinding[2].descriptorCount = 1;
        perFrameBinding[2].stageFlags = VK_SHADER_STAGE_ALL_GRAPHICS | VK_SHADER_STAGE_COMPUTE_BIT;
        perFrameBinding[2].pImmutableSamplers = nullptr;

        VkDescriptorSetLayoutCreateInfo lPerFrameDesc = {};
        lPerFrameDesc.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        lPerFrameDesc.pNext = nullptr;
        lPerFrameDesc.flags = 0;
        lPerFrameDesc.bindingCount = PER_FRAME_BINDING_COUNT;
        lPerFrameDesc.pBindings = perFrameBinding;

        VK_CHECK(vkCreateDescriptorSetLayout(b_device, &lPerFrameDesc, nullptr, &b_per_frame_layout));
    }

    {

        VkDescriptorSetLayoutBinding persistentBinding[PERSISTENT_BINDING_COUNT] = {};
        persistentBinding[0].binding = 0;
        persistentBinding[0].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        persistentBinding[0].descriptorCount = 1;
        persistentBinding[0].stageFlags = VK_SHADER_STAGE_ALL_GRAPHICS;
        persistentBinding[0].pImmutableSamplers = nullptr;
        persistentBinding[1].binding = 1;
        persistentBinding[1].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        persistentBinding[1].descriptorCount = 1;
        persistentBinding[1].stageFlags = VK_SHADER_STAGE_ALL_GRAPHICS;
        persistentBinding[1].pImmutableSamplers = nullptr;
        persistentBinding[2].binding = 2;
        persistentBinding[2].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        persistentBinding[2].descriptorCount = 1;
        persistentBinding[2].stageFlags = VK_SHADER_STAGE_ALL_GRAPHICS;
        persistentBinding[2].pImmutableSamplers = nullptr;
        persistentBinding[3].binding = 3;
        persistentBinding[3].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        persistentBinding[3].descriptorCount = 1;
        persistentBinding[3].stageFlags = VK_SHADER_STAGE_ALL_GRAPHICS;
        persistentBinding[3].pImmutableSamplers = nullptr;
        persistentBinding[4].binding = 4;
        persistentBinding[4].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        persistentBinding[4].descriptorCount = 1;
        persistentBinding[4].stageFlags = VK_SHADER_STAGE_ALL_GRAPHICS;
        persistentBinding[4].pImmutableSamplers = nullptr;
        persistentBinding[5].binding = 5;
        persistentBinding[5].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        persistentBinding[5].descriptorCount = BISTRO_TEXTURE_COUNT;
        persistentBinding[5].stageFlags = VK_SHADER_STAGE_ALL_GRAPHICS;
        persistentBinding[5].pImmutableSamplers = nullptr;
        persistentBinding[6].binding = 6;
        persistentBinding[6].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
        persistentBinding[6].descriptorCount = 1;
        persistentBinding[6].stageFlags = VK_SHADER_STAGE_ALL_GRAPHICS;
        persistentBinding[6].pImmutableSamplers = nullptr;
        persistentBinding[7].binding = 7;
        persistentBinding[7].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
        persistentBinding[7].descriptorCount = 1;
        persistentBinding[7].stageFlags = VK_SHADER_STAGE_ALL_GRAPHICS;
        persistentBinding[7].pImmutableSamplers = nullptr;
        persistentBinding[8].binding = 8;
        persistentBinding[8].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
        persistentBinding[8].descriptorCount = 1;
        persistentBinding[8].stageFlags = VK_SHADER_STAGE_ALL_GRAPHICS;
        persistentBinding[8].pImmutableSamplers = nullptr;
        persistentBinding[9].binding = 9;
        persistentBinding[9].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        persistentBinding[9].descriptorCount = 1;
        persistentBinding[9].stageFlags = VK_SHADER_STAGE_ALL_GRAPHICS;
        persistentBinding[9].pImmutableSamplers = nullptr;
        persistentBinding[10].binding = 10;
        persistentBinding[10].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        persistentBinding[10].descriptorCount = 1;
        persistentBinding[10].stageFlags = VK_SHADER_STAGE_ALL_GRAPHICS;
        persistentBinding[10].pImmutableSamplers = nullptr;
        persistentBinding[11].binding = 11;
        persistentBinding[11].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        persistentBinding[11].descriptorCount = 1;
        persistentBinding[11].stageFlags = VK_SHADER_STAGE_ALL_GRAPHICS;
        persistentBinding[11].pImmutableSamplers = nullptr;
        persistentBinding[12].binding = 12;
        persistentBinding[12].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        persistentBinding[12].descriptorCount = 1;
        persistentBinding[12].stageFlags = VK_SHADER_STAGE_ALL_GRAPHICS;
        persistentBinding[12].pImmutableSamplers = nullptr;
        persistentBinding[13].binding = 13;
        persistentBinding[13].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        persistentBinding[13].descriptorCount = 1;
        persistentBinding[13].stageFlags = VK_SHADER_STAGE_ALL_GRAPHICS;
        persistentBinding[13].pImmutableSamplers = nullptr;

        VkDescriptorSetLayoutCreateInfo lPersistentDesc = {};
        lPersistentDesc.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        lPersistentDesc.pNext = nullptr;
        lPersistentDesc.flags = 0;
        lPersistentDesc.bindingCount = PERSISTENT_BINDING_COUNT;
        lPersistentDesc.pBindings = persistentBinding;

        VK_CHECK(vkCreateDescriptorSetLayout(b_device, &lPersistentDesc, nullptr, &b_persistent_layout));
    }

    VkDescriptorSetLayout layouts[] = {b_persistent_layout, b_per_frame_layout};

    VkPushConstantRange pushConstantRanges[1];
    pushConstantRanges->offset = 0;
    pushConstantRanges->size =
        std::max(sizeof(ComposePushConstants), sizeof(TriangleFilteringPushConstantsCascadeShadows));

    pushConstantRanges->stageFlags =
        VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

    VkPipelineLayoutCreateInfo pipelineLayoutInfo = {};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 2;
    pipelineLayoutInfo.pSetLayouts = layouts;
    pipelineLayoutInfo.pushConstantRangeCount = 1;
    pipelineLayoutInfo.pPushConstantRanges = pushConstantRanges;

    VK_CHECK(vkCreatePipelineLayout(b_device, &pipelineLayoutInfo, nullptr, &b_pipeline_layout));
}

/* Prepare frame descriptors. */
void Renderer::prepare_frame_descriptors()
{

    for (uint32_t cur_set = 0; cur_set < FRAMES_IN_FLIGHT; cur_set++)
    {

        VkWriteDescriptorSet writePerFrame[PER_FRAME_BINDING_COUNT] = {};
        writePerFrame[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writePerFrame[0].pNext = nullptr;
        writePerFrame[0].dstSet = b_sets_per_frame[cur_set];
        writePerFrame[0].dstBinding = 0;
        writePerFrame[0].dstArrayElement = 0;
        writePerFrame[0].descriptorCount = 1;
        writePerFrame[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        writePerFrame[0].pBufferInfo = &b_ubo_buffer[cur_set]->descInfo;
        writePerFrame[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writePerFrame[1].pNext = nullptr;
        writePerFrame[1].dstSet = b_sets_per_frame[cur_set];
        writePerFrame[1].dstBinding = 1;
        writePerFrame[1].dstArrayElement = 0;
        writePerFrame[1].descriptorCount = 1;
        writePerFrame[1].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        writePerFrame[1].pBufferInfo = &b_shadow_gen_buffer[cur_set]->descInfo;
        writePerFrame[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writePerFrame[2].pNext = nullptr;
        writePerFrame[2].dstSet = b_sets_per_frame[cur_set];
        writePerFrame[2].dstBinding = 2;
        writePerFrame[2].dstArrayElement = 0;
        writePerFrame[2].descriptorCount = 1;
        writePerFrame[2].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        writePerFrame[2].pBufferInfo = &b_sky_lights_data_buffer[cur_set]->descInfo;

        vkUpdateDescriptorSets(b_device, PER_FRAME_BINDING_COUNT, writePerFrame, 0, nullptr);
    }
}

/* Prepare persistent descriptors. */
void Renderer::prepare_persistent_descriptors()
{

    if (!b_sets_persistent)
    {
        VkDescriptorSetLayout layouts[] = {b_per_frame_layout, b_per_frame_layout, b_persistent_layout};

        VkDescriptorSetAllocateInfo allocInfo = {};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.pNext = nullptr;
        allocInfo.descriptorPool = b_descriptor_pool;
        allocInfo.descriptorSetCount = static_cast<uint32_t>(std::size(layouts));
        allocInfo.pSetLayouts = layouts;
        VkDescriptorSet crSets[3] = {};
        VK_CHECK(vkAllocateDescriptorSets(b_device, &allocInfo, crSets));
        b_sets_per_frame[0] = crSets[0];
        b_sets_per_frame[1] = crSets[1];
        b_sets_persistent = crSets[2];
    }

    VkWriteDescriptorSet writePersistent[PERSISTENT_BINDING_COUNT] = {};

    writePersistent[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writePersistent[0].pNext = nullptr;
    writePersistent[0].dstSet = b_sets_persistent;
    writePersistent[0].dstBinding = 0;
    writePersistent[0].dstArrayElement = 0;
    writePersistent[0].descriptorCount = 1;
    writePersistent[0].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    writePersistent[0].pImageInfo = &b_depth_buffer_rt->imageInfo;

    writePersistent[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writePersistent[1].pNext = nullptr;
    writePersistent[1].dstSet = b_sets_persistent;
    writePersistent[1].dstBinding = 1;
    writePersistent[1].dstArrayElement = 0;
    writePersistent[1].descriptorCount = 1;
    writePersistent[1].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    writePersistent[1].pImageInfo = &b_ssao_rt->imageInfo;
    writePersistent[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writePersistent[2].pNext = nullptr;
    writePersistent[2].dstSet = b_sets_persistent;
    writePersistent[2].dstBinding = 2;
    writePersistent[2].dstArrayElement = 0;
    writePersistent[2].descriptorCount = 1;
    writePersistent[2].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    writePersistent[2].pImageInfo = &b_ssao_blur_rt->imageInfo;
    writePersistent[3].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writePersistent[3].pNext = nullptr;
    writePersistent[3].dstSet = b_sets_persistent;
    writePersistent[3].dstBinding = 3;
    writePersistent[3].dstArrayElement = 0;
    writePersistent[3].descriptorCount = 1;
    writePersistent[3].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    writePersistent[3].pImageInfo = &b_g_buffer_albedo_rt->imageInfo;
    writePersistent[4].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writePersistent[4].pNext = nullptr;
    writePersistent[4].dstSet = b_sets_persistent;
    writePersistent[4].dstBinding = 4;
    writePersistent[4].dstArrayElement = 0;
    writePersistent[4].descriptorCount = 1;
    writePersistent[4].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    writePersistent[4].pImageInfo = &b_g_buffer_normal_specular_rt->imageInfo;
    writePersistent[5].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writePersistent[5].pNext = nullptr;
    writePersistent[5].dstSet = b_sets_persistent;
    writePersistent[5].dstBinding = 5;
    writePersistent[5].dstArrayElement = 0;
    writePersistent[5].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;

    VkDescriptorImageInfo meshImageInfos[BISTRO_TEXTURE_COUNT] = {};
    const size_t textureCount = std::min<size_t>(b_model_gltf.image_count, BISTRO_TEXTURE_COUNT);
    const auto fallback = textureCount ? b_model_gltf.images[0].texture.descInfo
                                       : b_default_mesh_texture->descInfo;
    for (uint32_t i = 0; i < BISTRO_TEXTURE_COUNT; ++i)
        meshImageInfos[i] = i < textureCount ? b_model_gltf.images[i].texture.descInfo : fallback;
    writePersistent[5].descriptorCount = BISTRO_TEXTURE_COUNT;
    writePersistent[5].pImageInfo = meshImageInfos;
    writePersistent[6].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writePersistent[6].pNext = nullptr;
    writePersistent[6].dstSet = b_sets_persistent;
    writePersistent[6].dstBinding = 6;
    writePersistent[6].dstArrayElement = 0;
    writePersistent[6].descriptorCount = 1;
    writePersistent[6].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
    writePersistent[6].pImageInfo = &b_render_target_sampler->descInfo;
    writePersistent[7].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writePersistent[7].pNext = nullptr;
    writePersistent[7].dstSet = b_sets_persistent;
    writePersistent[7].dstBinding = 7;
    writePersistent[7].dstArrayElement = 0;
    writePersistent[7].descriptorCount = 1;
    writePersistent[7].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
    writePersistent[7].pImageInfo = &b_render_target_sampler->descInfo;
    writePersistent[8].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writePersistent[8].pNext = nullptr;
    writePersistent[8].dstSet = b_sets_persistent;
    writePersistent[8].dstBinding = 8;
    writePersistent[8].dstArrayElement = 0;
    writePersistent[8].descriptorCount = 1;
    writePersistent[8].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
    writePersistent[8].pImageInfo = &b_texture_sampler->descInfo;
    writePersistent[9].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writePersistent[9].pNext = nullptr;
    writePersistent[9].dstSet = b_sets_persistent;
    writePersistent[9].dstBinding = 9;
    writePersistent[9].dstArrayElement = 0;
    writePersistent[9].descriptorCount = 1;
    writePersistent[9].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    writePersistent[9].pImageInfo = &b_ssao_noise_texture->descInfo;
    writePersistent[10].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writePersistent[10].pNext = nullptr;
    writePersistent[10].dstSet = b_sets_persistent;
    writePersistent[10].dstBinding = 10;
    writePersistent[10].dstArrayElement = 0;
    writePersistent[10].descriptorCount = 1;
    writePersistent[10].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    writePersistent[10].pBufferInfo = &b_ssao_kernel_buffer->descInfo;
    writePersistent[11].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writePersistent[11].pNext = nullptr;
    writePersistent[11].dstSet = b_sets_persistent;
    writePersistent[11].dstBinding = 11;
    writePersistent[11].dstArrayElement = 0;
    writePersistent[11].descriptorCount = 1;
    writePersistent[11].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    writePersistent[11].pImageInfo = &b_shadow_render_targets.cascadeDepth->imageInfo;
    writePersistent[12].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writePersistent[12].pNext = nullptr;
    writePersistent[12].dstSet = b_sets_persistent;
    writePersistent[12].dstBinding = 12;
    writePersistent[12].dstArrayElement = 0;
    writePersistent[12].descriptorCount = 1;
    writePersistent[12].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    writePersistent[12].pImageInfo = &b_compose_image_post_processing_rt->imageInfo;
    writePersistent[13].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writePersistent[13].pNext = nullptr;
    writePersistent[13].dstSet = b_sets_persistent;
    writePersistent[13].dstBinding = 13;
    writePersistent[13].dstArrayElement = 0;
    writePersistent[13].descriptorCount = 1;
    writePersistent[13].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    writePersistent[13].pImageInfo = &b_screen_space_reflections_rt->imageInfo;

    vkUpdateDescriptorSets(b_device, PERSISTENT_BINDING_COUNT, writePersistent, 0, nullptr);
}

/* Create resources for each technique; rebuild missing targets after resize. */
void Renderer::init_rendering_tecniques()
{
    using F = TechniqueFlags;
    using P = PipelineType;

    /* shared resource creation helpers */
    auto load_shader = [this](Shader *&shader, const char *vertex, const char *fragment,
                              const char *compute) {
        if (shader)
            return;
        auto *slot = std::find_if(std::begin(b_shaders), std::end(b_shaders), [](const Shader &resource) {
            return !resource.vert && !resource.frag && !resource.geom && !resource.comp;
        });
        require(slot != std::end(b_shaders), "RENDERER_SHADER_COUNT capacity exceeded");
        shader = slot;
        if (vertex)
            load_shader_code(*shader, VK_SHADER_STAGE_VERTEX_BIT, vertex);
        if (fragment)
            load_shader_code(*shader, VK_SHADER_STAGE_FRAGMENT_BIT, fragment);
        if (compute)
            load_shader_code(*shader, VK_SHADER_STAGE_COMPUTE_BIT, compute);
    };
    auto color_target = [this](RenderTarget *&target, VkFormat format, const char *name) {
        if (target)
            return;
        RenderTargetDesc desc;
        desc.width = b_settings.width;
        desc.height = b_settings.height;
        desc.format = format;
        desc.pName = name;
        add_render_target(b_device, b_physical_device, &desc, &target);
    };
    auto create_buffer = [this](Buffer *&buffer, VkDeviceSize size, VkBufferUsageFlags usage,
                                VkMemoryPropertyFlags memory) {
        if (buffer)
            return;
        BufferDesc desc;
        desc.size = size;
        desc.usage = usage;
        desc.memoryProperties = memory;
        add_buffer(b_device, b_physical_device, desc, &buffer);
    };

    /* shared pipeline state */
    VkPipelineMultisampleStateCreateInfo multisampleState = {};
    multisampleState.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampleState.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT; // Or match your main pass
    multisampleState.sampleShadingEnable = VK_FALSE;
    multisampleState.minSampleShading = 1.0f;
    multisampleState.pSampleMask = nullptr;
    multisampleState.alphaToCoverageEnable = VK_FALSE;
    multisampleState.alphaToOneEnable = VK_FALSE;

    VkPipelineRasterizationStateCreateInfo pRasterizationState{};
    pRasterizationState.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    pRasterizationState.pNext = nullptr;
    pRasterizationState.flags = 0;
    pRasterizationState.depthClampEnable = VK_FALSE;
    pRasterizationState.rasterizerDiscardEnable = VK_FALSE;
    pRasterizationState.polygonMode = VK_POLYGON_MODE_FILL;
    pRasterizationState.cullMode = VK_CULL_MODE_BACK_BIT;
    pRasterizationState.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    pRasterizationState.depthBiasEnable = VK_FALSE;
    pRasterizationState.depthBiasConstantFactor = 0.0f;
    pRasterizationState.depthBiasClamp = 0.0f;
    pRasterizationState.depthBiasSlopeFactor = 0.0f;
    pRasterizationState.lineWidth = 1.0f;

    /* triangle filtering resources */
    {
        require(b_model_gltf.indexBufferVk, "Scene geometry is required for triangle filtering");
        create_buffer(b_t_filtered_triangles, b_model_gltf.indexBufferVk->size,
                      VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                          VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                      VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        create_buffer(b_t_filter_indirect_args, sizeof(VkDrawIndexedIndirectCommand),
                      VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                          VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        load_shader(b_shader_computer_filtering, nullptr, nullptr, "shaders/filtering.comp.spv");
        if (!b_t_filtering_pipeline)
            b_t_filtering_pipeline = add_pipeline(P::compute, F::triangle_filtering);
        if (!b_t_filtering_pipeline->handle())
        {
            VkPipelineShaderStageCreateInfo shaderStageDesc{};

            shaderStageDesc.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
            shaderStageDesc.pNext = nullptr;
            shaderStageDesc.flags = 0;
            shaderStageDesc.stage = VK_SHADER_STAGE_COMPUTE_BIT;
            shaderStageDesc.module = b_shader_computer_filtering->comp;
            shaderStageDesc.pName = "main";
            shaderStageDesc.pSpecializationInfo = nullptr;
            VkPipelineRenderingCreateInfo renderingInfo = {};
            renderingInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
            renderingInfo.colorAttachmentCount = 2;

            VkComputePipelineCreateInfo computeDesc{};
            computeDesc.layout = b_pipeline_layout;
            computeDesc.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
            VkPipelineShaderStageCreateInfo shaderComputeStageCreateInfo{};
            shaderComputeStageCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
            shaderComputeStageCreateInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
            shaderComputeStageCreateInfo.module = b_shader_computer_filtering->comp;
            shaderComputeStageCreateInfo.pName = "main";
            computeDesc.stage = shaderComputeStageCreateInfo;

            b_t_filtering_pipeline->create(b_device, computeDesc);
        }
    }

    /* cascade filtering resources */
    {
        load_shader(b_shader_cascade_shadow_mapping_filtering, nullptr, nullptr, "shaders/filtering_cascade.comp.spv");
        if (!b_t_filtering_pipeline_cascade_shadow_mapping)
            b_t_filtering_pipeline_cascade_shadow_mapping = add_pipeline(P::compute, F::cascade_filtering);
        if (!b_t_filtering_pipeline_cascade_shadow_mapping->handle())
        {
            VkPipelineShaderStageCreateInfo shaderStageDesc{};
            shaderStageDesc.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
            shaderStageDesc.pNext = nullptr;
            shaderStageDesc.flags = 0;
            shaderStageDesc.stage = VK_SHADER_STAGE_COMPUTE_BIT;
            shaderStageDesc.module = b_shader_cascade_shadow_mapping_filtering->comp;
            shaderStageDesc.pName = "main";
            shaderStageDesc.pSpecializationInfo = nullptr;
            VkPipelineRenderingCreateInfo renderingInfo = {};
            renderingInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
            renderingInfo.colorAttachmentCount = 2;

            VkComputePipelineCreateInfo computeDesc{};
            computeDesc.layout = b_pipeline_layout;
            computeDesc.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;

            VkPipelineShaderStageCreateInfo shaderComputeStageCreateInfo{};
            shaderComputeStageCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
            shaderComputeStageCreateInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
            shaderComputeStageCreateInfo.module = b_shader_cascade_shadow_mapping_filtering->comp;
            shaderComputeStageCreateInfo.pName = "main";

            computeDesc.stage = shaderComputeStageCreateInfo;

            b_t_filtering_pipeline_cascade_shadow_mapping->create(b_device, computeDesc);
        }
    }

    /* g buffer resources */
    {
        color_target(b_g_buffer_albedo_rt, VK_FORMAT_R8G8B8A8_SNORM, "GBuffer_AlbedoRT");
        color_target(b_g_buffer_normal_specular_rt, VK_FORMAT_R16G16B16A16_SFLOAT, "GBuffer_NormalSpecularRT");
        if (!b_depth_buffer_rt)
        {
            RenderTargetDesc depth;
            depth.width = b_settings.width;
            depth.height = b_settings.height;
            depth.format = VK_FORMAT_D32_SFLOAT;
            depth.usageFlags = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
            depth.clearValue = {.depthStencil = {1.0f, 0}};
            depth.pName = "DepthBuffer";
            add_render_target(b_device, b_physical_device, &depth, &b_depth_buffer_rt);
        }
        load_shader(b_g_buffer_shader, "shaders/gbuffer.vert.spv", "shaders/gbuffer.frag.spv", nullptr);
        if (!b_g_buffer_pipeline)
            b_g_buffer_pipeline = add_pipeline(P::graphics, F::g_buffer | F::depth_buffering);
        if (!b_g_buffer_pipeline->handle())
        {
            VkPipelineDepthStencilStateCreateInfo depthStencilState = {};
            depthStencilState.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
            depthStencilState.depthTestEnable = VK_TRUE;
            depthStencilState.depthWriteEnable = VK_TRUE;
            depthStencilState.depthCompareOp = VK_COMPARE_OP_LESS;
            depthStencilState.depthBoundsTestEnable = VK_FALSE;
            depthStencilState.stencilTestEnable = VK_FALSE;
            depthStencilState.minDepthBounds = 0.0f;
            depthStencilState.maxDepthBounds = 1.0f;

            VkPipelineViewportStateCreateInfo viewportState = {};
            viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
            viewportState.viewportCount = 1;
            viewportState.pViewports = nullptr;
            viewportState.scissorCount = 1;
            viewportState.pScissors = nullptr;
            VkVertexInputBindingDescription bindingDescription = {};
            bindingDescription.binding = 0;
            bindingDescription.stride = sizeof(GltfModel::Vertex); // 32 bytes (8 floats)
            bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

            VkVertexInputAttributeDescription attributeDescriptions[4] = {};

            attributeDescriptions[0].binding = 0;
            attributeDescriptions[0].location = 0;
            attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
            attributeDescriptions[0].offset = offsetof(GltfModel::Vertex, pos); // 0

            attributeDescriptions[1].binding = 0;
            attributeDescriptions[1].location = 1;
            attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
            attributeDescriptions[1].offset = offsetof(GltfModel::Vertex, normal); // 12

            attributeDescriptions[2].binding = 0;
            attributeDescriptions[2].location = 2;
            attributeDescriptions[2].format = VK_FORMAT_R32G32_SFLOAT;
            attributeDescriptions[2].offset = offsetof(GltfModel::Vertex, uv); // 24

            attributeDescriptions[3].binding = 0;
            attributeDescriptions[3].location = 3;
            attributeDescriptions[3].format = VK_FORMAT_R32_UINT;
            attributeDescriptions[3].offset = offsetof(GltfModel::Vertex, textureIndex);

            VkPipelineVertexInputStateCreateInfo vertexInputInfo = {};
            vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
            vertexInputInfo.vertexBindingDescriptionCount = 1;
            vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
            vertexInputInfo.vertexAttributeDescriptionCount = 4;
            vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions;
            VkPipelineInputAssemblyStateCreateInfo inputAssemblyState;
            inputAssemblyState.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
            inputAssemblyState.primitiveRestartEnable = VK_FALSE;
            inputAssemblyState.topology = VkPrimitiveTopology::VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
            inputAssemblyState.flags = 0;
            inputAssemblyState.pNext = NULL;
            VkPipelineShaderStageCreateInfo shaderStageDesc[2]{};
            shaderStageDesc[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
            shaderStageDesc[0].pNext = nullptr;
            shaderStageDesc[0].flags = 0;
            shaderStageDesc[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
            shaderStageDesc[0].module = b_g_buffer_shader->vert;
            shaderStageDesc[0].pName = "main";
            shaderStageDesc[0].pSpecializationInfo = nullptr;
            shaderStageDesc[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
            shaderStageDesc[1].pNext = nullptr;
            shaderStageDesc[1].flags = 0;
            shaderStageDesc[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
            shaderStageDesc[1].module = b_g_buffer_shader->frag;
            shaderStageDesc[1].pName = "main";
            shaderStageDesc[1].pSpecializationInfo = nullptr;
            VkPipelineRenderingCreateInfo renderingInfo = {};
            renderingInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
            renderingInfo.colorAttachmentCount = 2;
            VkFormat colorsAttachments[2] = {b_g_buffer_normal_specular_rt->format, b_g_buffer_albedo_rt->format};

            renderingInfo.pColorAttachmentFormats = colorsAttachments;
            renderingInfo.depthAttachmentFormat = VK_FORMAT_D32_SFLOAT;
            renderingInfo.stencilAttachmentFormat = VK_FORMAT_UNDEFINED;

            VkDynamicState dynamicStates[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR,
                                              VK_DYNAMIC_STATE_COLOR_WRITE_ENABLE_EXT,
                                              VK_DYNAMIC_STATE_DEPTH_WRITE_ENABLE};

            VkPipelineDynamicStateCreateInfo dynamicState = {};
            dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
            dynamicState.dynamicStateCount = 4;
            dynamicState.pDynamicStates = dynamicStates;

            VkPipelineColorBlendAttachmentState blendAttachments[2] = {};
            blendAttachments[0].colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                                 VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
            blendAttachments[0].blendEnable = VK_FALSE;
            blendAttachments[1] = blendAttachments[0];

            VkPipelineColorBlendStateCreateInfo colorBlendState{};
            colorBlendState.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
            colorBlendState.attachmentCount = 2;
            colorBlendState.pAttachments = blendAttachments;
            colorBlendState.logicOpEnable = VK_FALSE;

            VkGraphicsPipelineCreateInfo gBufferPipelineDesc{};
            gBufferPipelineDesc.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
            gBufferPipelineDesc.pMultisampleState = &multisampleState;
            gBufferPipelineDesc.pNext = &renderingInfo;
            gBufferPipelineDesc.stageCount = 2;
            gBufferPipelineDesc.renderPass = VK_NULL_HANDLE;
            gBufferPipelineDesc.pStages = shaderStageDesc;
            gBufferPipelineDesc.pInputAssemblyState = &inputAssemblyState;
            gBufferPipelineDesc.pTessellationState = nullptr;
            gBufferPipelineDesc.pViewportState = &viewportState;
            gBufferPipelineDesc.pRasterizationState = &pRasterizationState;
            gBufferPipelineDesc.pDynamicState = &dynamicState;
            gBufferPipelineDesc.pDepthStencilState = &depthStencilState;
            gBufferPipelineDesc.pVertexInputState = &vertexInputInfo;
            gBufferPipelineDesc.layout = b_pipeline_layout;
            gBufferPipelineDesc.subpass = 0;
            gBufferPipelineDesc.pColorBlendState = &colorBlendState;
            gBufferPipelineDesc.flags = 0;

            b_g_buffer_pipeline->create(b_device, gBufferPipelineDesc);
        }
    }

    /* shadow depth resources */
    {
        if (!b_shadow_render_targets.cascadeDepth)
        {
            RenderTargetDesc shadow;
            shadow.width = SHADOW_MAP_SIZE;
            shadow.height = SHADOW_MAP_SIZE;
            shadow.format = VK_FORMAT_D32_SFLOAT;
            shadow.usageFlags = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
            shadow.clearValue = {.depthStencil = {1.0f, 0}};
            shadow.arraySize = MAX_SHADOW_CASCADES;
            shadow.pName = "Shadow_CascadeDepthRT";
            add_render_target(b_device, b_physical_device, &shadow, &b_shadow_render_targets.cascadeDepth);
            add_shadow_cascade_layer_views();
        }
        require(has_shadow_render_targets(), "Shadow render targets were not created correctly");
        update_shadow_generation_data(true);
        for (uint32_t frame = 0; frame < FRAMES_IN_FLIGHT; ++frame)
        {
            if (!b_shadow_gen_buffer[frame])
            {
                create_buffer(b_shadow_gen_buffer[frame], sizeof(ShadowGenerationUniform),
                              VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                              VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
                map_buffer(b_device, b_shadow_gen_buffer[frame], &b_shadow_gen_uniform_data);
            }
        }
        load_shader(b_shadow_depth_shader, "shaders/shadow_depth.vert.spv", "shaders/shadow_depth.frag.spv", nullptr);
        if (!b_shadow_depth_pipeline)
            b_shadow_depth_pipeline = add_pipeline(P::graphics, F::shadow_depth | F::depth_buffering);
        if (!b_shadow_depth_pipeline->handle())
        {
            VkPipelineDepthStencilStateCreateInfo depthStencilState = {};
            depthStencilState.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
            depthStencilState.depthTestEnable = VK_TRUE;
            depthStencilState.depthWriteEnable = VK_TRUE;
            depthStencilState.depthCompareOp = VK_COMPARE_OP_LESS;
            depthStencilState.depthBoundsTestEnable = VK_FALSE;
            depthStencilState.stencilTestEnable = VK_FALSE;
            depthStencilState.minDepthBounds = 0.0f;
            depthStencilState.maxDepthBounds = 1.0f;

            VkPipelineViewportStateCreateInfo viewportState = {};
            viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
            viewportState.viewportCount = 1;
            viewportState.pViewports = nullptr;
            viewportState.scissorCount = 1;
            viewportState.pScissors = nullptr;

            VkVertexInputBindingDescription bindingDescription = {};
            bindingDescription.binding = 0;
            bindingDescription.stride = sizeof(GltfModel::Vertex);
            bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

            VkVertexInputAttributeDescription attributeDescriptions[4] = {};
            attributeDescriptions[0].binding = 0;
            attributeDescriptions[0].location = 0;
            attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
            attributeDescriptions[0].offset = offsetof(GltfModel::Vertex, pos);

            attributeDescriptions[1].binding = 0;
            attributeDescriptions[1].location = 1;
            attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
            attributeDescriptions[1].offset = offsetof(GltfModel::Vertex, normal);

            attributeDescriptions[2].binding = 0;
            attributeDescriptions[2].location = 2;
            attributeDescriptions[2].format = VK_FORMAT_R32G32_SFLOAT;
            attributeDescriptions[2].offset = offsetof(GltfModel::Vertex, uv);

            attributeDescriptions[3].binding = 0;
            attributeDescriptions[3].location = 3;
            attributeDescriptions[3].format = VK_FORMAT_R32_UINT;
            attributeDescriptions[3].offset = offsetof(GltfModel::Vertex, textureIndex);

            VkPipelineVertexInputStateCreateInfo vertexInputInfo = {};
            vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
            vertexInputInfo.vertexBindingDescriptionCount = 1;
            vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
            vertexInputInfo.vertexAttributeDescriptionCount = 4;
            vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions;

            VkPipelineInputAssemblyStateCreateInfo inputAssemblyState = {};
            inputAssemblyState.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
            inputAssemblyState.primitiveRestartEnable = VK_FALSE;
            inputAssemblyState.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

            VkPipelineShaderStageCreateInfo shaderStageDesc[2] = {};
            shaderStageDesc[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
            shaderStageDesc[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
            shaderStageDesc[0].module = b_shadow_depth_shader->vert;
            shaderStageDesc[0].pName = "main";

            shaderStageDesc[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
            shaderStageDesc[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
            shaderStageDesc[1].module = b_shadow_depth_shader->frag;
            shaderStageDesc[1].pName = "main";

            VkPipelineRenderingCreateInfo renderingInfo = {};
            renderingInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
            renderingInfo.colorAttachmentCount = 0;
            renderingInfo.pColorAttachmentFormats = nullptr;
            renderingInfo.depthAttachmentFormat = b_shadow_render_targets.cascadeDepth->format;
            renderingInfo.stencilAttachmentFormat = VK_FORMAT_UNDEFINED;

            VkDynamicState dynamicStates[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};

            VkPipelineDynamicStateCreateInfo dynamicState = {};
            dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
            dynamicState.dynamicStateCount = static_cast<uint32_t>(std::size(dynamicStates));
            dynamicState.pDynamicStates = dynamicStates;

            VkPipelineRasterizationStateCreateInfo rasterizationState = pRasterizationState;
            rasterizationState.cullMode = VK_CULL_MODE_BACK_BIT;
            rasterizationState.depthBiasEnable = VK_FALSE;

            VkPipelineColorBlendStateCreateInfo colorBlendState = {};
            colorBlendState.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
            colorBlendState.attachmentCount = 0;
            colorBlendState.pAttachments = nullptr;

            VkGraphicsPipelineCreateInfo shadowDepthPipelineDesc = {};
            shadowDepthPipelineDesc.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
            shadowDepthPipelineDesc.pNext = &renderingInfo;
            shadowDepthPipelineDesc.stageCount = 2;
            shadowDepthPipelineDesc.pStages = shaderStageDesc;
            shadowDepthPipelineDesc.pVertexInputState = &vertexInputInfo;
            shadowDepthPipelineDesc.pInputAssemblyState = &inputAssemblyState;
            shadowDepthPipelineDesc.pViewportState = &viewportState;
            shadowDepthPipelineDesc.pRasterizationState = &rasterizationState;
            shadowDepthPipelineDesc.pMultisampleState = &multisampleState;
            shadowDepthPipelineDesc.pDepthStencilState = &depthStencilState;
            shadowDepthPipelineDesc.pColorBlendState = &colorBlendState;
            shadowDepthPipelineDesc.pDynamicState = &dynamicState;
            shadowDepthPipelineDesc.layout = b_pipeline_layout;
            shadowDepthPipelineDesc.renderPass = VK_NULL_HANDLE;
            shadowDepthPipelineDesc.subpass = 0;

            b_shadow_depth_pipeline->create(b_device, shadowDepthPipelineDesc);
        }
    }

    /* ssao resources */
    {
        color_target(b_ssao_rt, VK_FORMAT_R32_SFLOAT, "SSAO_GenRT");
        if (!b_ssao_kernel_buffer)
        {
            generate_ssao_kernel(true);
            create_buffer(b_ssao_kernel_buffer, sizeof(b_ssao_kernels), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                          VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
            map_buffer(b_device, b_ssao_kernel_buffer, b_ssao_kernels);
        }
        if (!b_ssao_noise_texture)
        {
            generate_ssao_noise();
            TextureDesc noise;
            noise.width = SSAO_NOISE_DIM;
            noise.height = SSAO_NOISE_DIM;
            noise.format = VK_FORMAT_R32G32B32A32_SFLOAT;
            noise.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
            add_texture(b_device, b_physical_device, noise, &b_ssao_noise_texture);
            map_texture_data(b_device, b_physical_device, b_graphics_command_pool, b_graphics_queue,
                             b_ssao_noise_texture, b_ssao_noise, sizeof(b_ssao_noise));
        }
        load_shader(b_ssao_shader, "shaders/fullscreen.vert.spv", "shaders/ssao.frag.spv", nullptr);
        if (!b_ssao_pipeline)
            b_ssao_pipeline = add_pipeline(P::graphics, F::ssao);
        if (!b_ssao_pipeline->handle())
        {
            VkPipelineDepthStencilStateCreateInfo depthStencilState = {};
            depthStencilState.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
            depthStencilState.depthTestEnable = VK_FALSE;
            depthStencilState.depthWriteEnable = VK_FALSE;
            depthStencilState.depthCompareOp = VK_COMPARE_OP_LESS;
            depthStencilState.depthBoundsTestEnable = VK_FALSE;
            depthStencilState.stencilTestEnable = VK_FALSE;

            VkPipelineViewportStateCreateInfo viewportState = {};
            viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
            viewportState.viewportCount = 1;
            viewportState.pViewports = nullptr;
            viewportState.scissorCount = 1;
            viewportState.pScissors = nullptr;
            VkVertexInputBindingDescription bindingDescription = {};
            bindingDescription.binding = 0;
            bindingDescription.stride = sizeof(GltfModel::Vertex); // 32 bytes (8 floats)
            bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
            VkPipelineInputAssemblyStateCreateInfo inputAssemblyState{};
            inputAssemblyState.primitiveRestartEnable = VK_FALSE;
            inputAssemblyState.topology = VkPrimitiveTopology::VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
            inputAssemblyState.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
            VkPipelineShaderStageCreateInfo shaderStageDescSSAO[2]{};
            shaderStageDescSSAO[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
            shaderStageDescSSAO[0].pNext = nullptr;
            shaderStageDescSSAO[0].flags = 0;
            shaderStageDescSSAO[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
            shaderStageDescSSAO[0].module = b_ssao_shader->vert;
            shaderStageDescSSAO[0].pName = "main";
            shaderStageDescSSAO[0].pSpecializationInfo = nullptr;
            shaderStageDescSSAO[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
            shaderStageDescSSAO[1].pNext = nullptr;
            shaderStageDescSSAO[1].flags = 0;
            shaderStageDescSSAO[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
            shaderStageDescSSAO[1].module = b_ssao_shader->frag;
            shaderStageDescSSAO[1].pName = "main";
            shaderStageDescSSAO[1].pSpecializationInfo = nullptr;

            VkPipelineVertexInputStateCreateInfo emptyVI = {};
            emptyVI.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
            VkPipelineRenderingCreateInfo renderingInfo = {};
            renderingInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
            renderingInfo.colorAttachmentCount = 1;
            renderingInfo.pColorAttachmentFormats = &b_ssao_rt->format;

            VkDynamicState dynamicStates[] = {
                VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR, VK_DYNAMIC_STATE_COLOR_WRITE_ENABLE_EXT,
                VK_DYNAMIC_STATE_VERTEX_INPUT_EXT, VK_DYNAMIC_STATE_COLOR_BLEND_ENABLE_EXT};

            VkPipelineDynamicStateCreateInfo dynamicState = {};
            dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
            dynamicState.dynamicStateCount = 5;
            dynamicState.pDynamicStates = dynamicStates;

            VkPipelineColorBlendAttachmentState blendAttachments[1] = {};
            blendAttachments[0].colorWriteMask = VK_COLOR_COMPONENT_R_BIT;
            blendAttachments[0].blendEnable = VK_FALSE;

            VkPipelineColorBlendStateCreateInfo colorBlendState{};
            colorBlendState.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
            colorBlendState.attachmentCount = 1;
            colorBlendState.pAttachments = blendAttachments;
            colorBlendState.logicOpEnable = VK_FALSE;

            VkGraphicsPipelineCreateInfo gSSAOPipelineDesc{};
            gSSAOPipelineDesc.pMultisampleState = &multisampleState;
            gSSAOPipelineDesc.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;

            gSSAOPipelineDesc.pNext = &renderingInfo;
            gSSAOPipelineDesc.stageCount = 2;
            gSSAOPipelineDesc.pStages = shaderStageDescSSAO;
            gSSAOPipelineDesc.pInputAssemblyState = &inputAssemblyState;
            gSSAOPipelineDesc.pTessellationState = nullptr;
            gSSAOPipelineDesc.pViewportState = &viewportState;
            gSSAOPipelineDesc.pDynamicState = &dynamicState;
            gSSAOPipelineDesc.pDepthStencilState = &depthStencilState;
            pRasterizationState.cullMode = VK_CULL_MODE_NONE;
            gSSAOPipelineDesc.pRasterizationState = &pRasterizationState;
            gSSAOPipelineDesc.pVertexInputState = VK_NULL_HANDLE;
            gSSAOPipelineDesc.renderPass = VK_NULL_HANDLE;
            gSSAOPipelineDesc.layout = b_pipeline_layout;
            gSSAOPipelineDesc.pVertexInputState = &emptyVI;
            gSSAOPipelineDesc.flags = 0;
            gSSAOPipelineDesc.pColorBlendState = &colorBlendState;

            VK_CHECK(
                vkCreateGraphicsPipelines(b_device, nullptr, 1, &gSSAOPipelineDesc, nullptr, b_ssao_pipeline->put()));
        }
    }

    /* ssao blur resources */
    {
        color_target(b_ssao_blur_rt, VK_FORMAT_R32_SFLOAT, "SSAO_BlurRT");
        load_shader(b_ssao_blur_shader, nullptr, nullptr, "shaders/ssao_blur.comp.spv");
        if (!b_ssao_blur_pipeline)
            b_ssao_blur_pipeline = add_pipeline(P::compute, F::ssao_blur);
        if (!b_ssao_blur_pipeline->handle())
        {
            VkPipelineShaderStageCreateInfo shaderStageDesc{};
            shaderStageDesc.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
            shaderStageDesc.pNext = nullptr;
            shaderStageDesc.flags = 0;
            shaderStageDesc.stage = VK_SHADER_STAGE_COMPUTE_BIT;
            shaderStageDesc.module = b_ssao_blur_shader->comp;
            shaderStageDesc.pName = "main";
            shaderStageDesc.pSpecializationInfo = nullptr;
            VkPipelineRenderingCreateInfo renderingInfo = {};
            renderingInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
            renderingInfo.colorAttachmentCount = 1;
            renderingInfo.pColorAttachmentFormats = &b_ssao_rt->format;

            VkDynamicState dynamicStates[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR,
                                              VK_DYNAMIC_STATE_COLOR_WRITE_ENABLE_EXT,
                                              VK_DYNAMIC_STATE_COLOR_BLEND_ENABLE_EXT};

            VkPipelineDynamicStateCreateInfo dynamicState = {};
            dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
            dynamicState.dynamicStateCount = 4;
            dynamicState.pDynamicStates = dynamicStates;

            VkComputePipelineCreateInfo blurPipelineDesc{};
            blurPipelineDesc.layout = NULL; //
            blurPipelineDesc.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
            blurPipelineDesc.stage = shaderStageDesc;
            blurPipelineDesc.layout = b_pipeline_layout;
            blurPipelineDesc.flags = 0;
            blurPipelineDesc.pNext = nullptr;
            b_ssao_blur_pipeline->create(b_device, blurPipelineDesc);
        }
    }

    /* screen space reflections resources */
    {
        color_target(b_screen_space_reflections_rt, VK_FORMAT_R16G16B16A16_SFLOAT, "ScreenSpaceReflections");
        load_shader(b_screen_space_reflections_shader, "shaders/fullscreen.vert.spv", "shaders/screen_space_ref.frag.spv", nullptr);
        pRasterizationState.cullMode = VK_CULL_MODE_NONE;
        if (!b_screen_space_reflections_pipeline)
            b_screen_space_reflections_pipeline = add_pipeline(P::graphics, F::reflections);
        if (!b_screen_space_reflections_pipeline->handle())
        {
            VkPipelineDepthStencilStateCreateInfo depthStencilState = {};
            depthStencilState.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
            depthStencilState.depthTestEnable = VK_FALSE;
            depthStencilState.depthWriteEnable = VK_FALSE;

            VkPipelineViewportStateCreateInfo viewportState = {};
            viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
            viewportState.viewportCount = 1;
            viewportState.scissorCount = 1;

            VkPipelineInputAssemblyStateCreateInfo inputAssemblyState = {};
            inputAssemblyState.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
            inputAssemblyState.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

            VkPipelineShaderStageCreateInfo shaderStages[2] = {};
            shaderStages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
            shaderStages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
            shaderStages[0].module = b_screen_space_reflections_shader->vert;
            shaderStages[0].pName = "main";
            shaderStages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
            shaderStages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
            shaderStages[1].module = b_screen_space_reflections_shader->frag;
            shaderStages[1].pName = "main";

            VkPipelineRenderingCreateInfo renderingInfo = {};
            renderingInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
            renderingInfo.colorAttachmentCount = 1;
            renderingInfo.pColorAttachmentFormats = &b_screen_space_reflections_rt->format;

            VkDynamicState dynamicStates[] = {
                VK_DYNAMIC_STATE_VIEWPORT,
                VK_DYNAMIC_STATE_SCISSOR,
                VK_DYNAMIC_STATE_COLOR_WRITE_ENABLE_EXT,
            };
            VkPipelineDynamicStateCreateInfo dynamicState = {};
            dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
            dynamicState.dynamicStateCount = static_cast<uint32_t>(std::size(dynamicStates));
            dynamicState.pDynamicStates = dynamicStates;

            VkPipelineVertexInputStateCreateInfo vertexInputInfo = {};
            vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

            VkPipelineColorBlendAttachmentState blendAttachment = {};
            blendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                             VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
            VkPipelineColorBlendStateCreateInfo colorBlendState = {};
            colorBlendState.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
            colorBlendState.attachmentCount = 1;
            colorBlendState.pAttachments = &blendAttachment;

            VkGraphicsPipelineCreateInfo pipelineDesc = {};
            pipelineDesc.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
            pipelineDesc.pNext = &renderingInfo;
            pipelineDesc.stageCount = 2;
            pipelineDesc.pStages = shaderStages;
            pipelineDesc.pVertexInputState = &vertexInputInfo;
            pipelineDesc.pInputAssemblyState = &inputAssemblyState;
            pipelineDesc.pViewportState = &viewportState;
            pipelineDesc.pRasterizationState = &pRasterizationState;
            pipelineDesc.pMultisampleState = &multisampleState;
            pipelineDesc.pDepthStencilState = &depthStencilState;
            pipelineDesc.pColorBlendState = &colorBlendState;
            pipelineDesc.pDynamicState = &dynamicState;
            pipelineDesc.layout = b_pipeline_layout;
            b_screen_space_reflections_pipeline->create(b_device, pipelineDesc);
        }
    }

    /* composition resources */
    {
        color_target(b_compose_image_post_processing_rt, VK_FORMAT_B8G8R8A8_SRGB, "ComposeImage");
        load_shader(b_compose_shader, "shaders/fullscreen.vert.spv", "shaders/compose.frag.spv", nullptr);
        pRasterizationState.cullMode = VK_CULL_MODE_NONE;
        if (!b_compose_pipeline)
            b_compose_pipeline = add_pipeline(P::graphics, F::composition);
        if (!b_compose_pipeline->handle())
        {
            VkPipelineDepthStencilStateCreateInfo depthStencilState = {};
            depthStencilState.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
            depthStencilState.depthTestEnable = VK_FALSE;
            depthStencilState.depthWriteEnable = VK_FALSE;
            depthStencilState.depthCompareOp = VK_COMPARE_OP_LESS;
            depthStencilState.depthBoundsTestEnable = VK_FALSE;
            depthStencilState.stencilTestEnable = VK_FALSE;

            VkPipelineViewportStateCreateInfo viewportState = {};
            viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
            viewportState.viewportCount = 1;
            viewportState.pViewports = nullptr;
            viewportState.scissorCount = 1;
            viewportState.pScissors = nullptr;
            VkVertexInputBindingDescription bindingDescription = {};
            bindingDescription.binding = 0;
            bindingDescription.stride = sizeof(GltfModel::Vertex); // 32 bytes (8 floats)
            bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
            VkPipelineInputAssemblyStateCreateInfo inputAssemblyState{};
            inputAssemblyState.primitiveRestartEnable = VK_FALSE;
            inputAssemblyState.topology = VkPrimitiveTopology::VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
            inputAssemblyState.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
            VkPipelineShaderStageCreateInfo shaderStageDesc[2]{};
            shaderStageDesc[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
            shaderStageDesc[0].pNext = nullptr;
            shaderStageDesc[0].flags = 0;
            shaderStageDesc[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
            shaderStageDesc[0].module = b_compose_shader->vert;
            shaderStageDesc[0].pName = "main";
            shaderStageDesc[0].pSpecializationInfo = nullptr;
            shaderStageDesc[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
            shaderStageDesc[1].pNext = nullptr;
            shaderStageDesc[1].flags = 0;
            shaderStageDesc[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
            shaderStageDesc[1].module = b_compose_shader->frag;
            shaderStageDesc[1].pName = "main";
            shaderStageDesc[1].pSpecializationInfo = nullptr;
            VkPipelineRenderingCreateInfo renderingInfo = {};
            renderingInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
            renderingInfo.colorAttachmentCount = 1;
            renderingInfo.pColorAttachmentFormats = b_swapchain_format;

            VkDynamicState dynamicStates[] = {
                VK_DYNAMIC_STATE_VIEWPORT,
                VK_DYNAMIC_STATE_SCISSOR,
                VK_DYNAMIC_STATE_COLOR_WRITE_ENABLE_EXT,
            };

            VkPipelineDynamicStateCreateInfo dynamicState = {};
            dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
            dynamicState.dynamicStateCount = 3;
            dynamicState.pDynamicStates = dynamicStates;

            VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
            vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
            vertexInputInfo.vertexBindingDescriptionCount = 0;
            vertexInputInfo.vertexAttributeDescriptionCount = 0;

            VkPipelineColorBlendAttachmentState blendAttachments[1] = {};
            blendAttachments[0].colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                                 VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
            blendAttachments[0].blendEnable = VK_FALSE;

            VkPipelineColorBlendStateCreateInfo colorBlendState{};
            colorBlendState.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
            colorBlendState.attachmentCount = 1;
            colorBlendState.pAttachments = blendAttachments;
            colorBlendState.logicOpEnable = VK_FALSE;

            VkGraphicsPipelineCreateInfo gComposePipelineDesc{};
            gComposePipelineDesc.pMultisampleState = &multisampleState;
            gComposePipelineDesc.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
            gComposePipelineDesc.pVertexInputState = &vertexInputInfo;
            gComposePipelineDesc.pNext = &renderingInfo;
            gComposePipelineDesc.stageCount = 2;
            gComposePipelineDesc.pStages = shaderStageDesc;
            gComposePipelineDesc.pInputAssemblyState = &inputAssemblyState;
            gComposePipelineDesc.pTessellationState = nullptr;
            gComposePipelineDesc.pViewportState = &viewportState;
            gComposePipelineDesc.pDynamicState = &dynamicState;
            gComposePipelineDesc.pDepthStencilState = &depthStencilState;
            gComposePipelineDesc.pRasterizationState = &pRasterizationState;
            gComposePipelineDesc.pColorBlendState = &colorBlendState;
            gComposePipelineDesc.renderPass = VK_NULL_HANDLE;
            gComposePipelineDesc.layout = b_pipeline_layout;
            gComposePipelineDesc.flags = 0;
            b_compose_pipeline->create(b_device, gComposePipelineDesc);
        }
    }

    /* fxaa resources */
    {
        load_shader(b_fxaa_pass_shader, "shaders/fullscreen.vert.spv", "shaders/fxaa_pass.frag.spv", nullptr);
        if (!b_fxaa_pipeline)
            b_fxaa_pipeline = add_pipeline(P::graphics, F::fxaa);
        if (!b_fxaa_pipeline->handle())
        {
            VkPipelineDepthStencilStateCreateInfo depthStencilState = {};
            depthStencilState.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
            depthStencilState.depthTestEnable = VK_FALSE;
            depthStencilState.depthWriteEnable = VK_FALSE;
            depthStencilState.depthCompareOp = VK_COMPARE_OP_LESS;
            depthStencilState.depthBoundsTestEnable = VK_FALSE;
            depthStencilState.stencilTestEnable = VK_FALSE;

            VkPipelineViewportStateCreateInfo viewportState = {};
            viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
            viewportState.viewportCount = 1;
            viewportState.pViewports = nullptr;
            viewportState.scissorCount = 1;
            viewportState.pScissors = nullptr;

            VkPipelineInputAssemblyStateCreateInfo inputAssemblyState = {};
            inputAssemblyState.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
            inputAssemblyState.primitiveRestartEnable = VK_FALSE;
            inputAssemblyState.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

            VkPipelineShaderStageCreateInfo shaderStageDesc[2] = {};
            shaderStageDesc[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
            shaderStageDesc[0].pNext = nullptr;
            shaderStageDesc[0].flags = 0;
            shaderStageDesc[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
            shaderStageDesc[0].module = b_fxaa_pass_shader->vert;
            shaderStageDesc[0].pName = "main";
            shaderStageDesc[0].pSpecializationInfo = nullptr;

            shaderStageDesc[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
            shaderStageDesc[1].pNext = nullptr;
            shaderStageDesc[1].flags = 0;
            shaderStageDesc[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
            shaderStageDesc[1].module = b_fxaa_pass_shader->frag;
            shaderStageDesc[1].pName = "main";
            shaderStageDesc[1].pSpecializationInfo = nullptr;

            VkPipelineRenderingCreateInfo renderingInfo = {};
            renderingInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
            renderingInfo.colorAttachmentCount = 1;
            renderingInfo.pColorAttachmentFormats = b_swapchain_format;

            VkDynamicState dynamicStates[] = {
                VK_DYNAMIC_STATE_VIEWPORT,
                VK_DYNAMIC_STATE_SCISSOR,
                VK_DYNAMIC_STATE_COLOR_WRITE_ENABLE_EXT,
            };

            VkPipelineDynamicStateCreateInfo dynamicState = {};
            dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
            dynamicState.dynamicStateCount = static_cast<uint32_t>(std::size(dynamicStates));
            dynamicState.pDynamicStates = dynamicStates;

            VkPipelineVertexInputStateCreateInfo vertexInputInfo = {};
            vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
            vertexInputInfo.vertexBindingDescriptionCount = 0;
            vertexInputInfo.vertexAttributeDescriptionCount = 0;

            VkPipelineRasterizationStateCreateInfo rasterizationState = pRasterizationState;
            rasterizationState.cullMode = VK_CULL_MODE_NONE;

            VkPipelineColorBlendAttachmentState blendAttachment = {};
            blendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                             VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
            blendAttachment.blendEnable = VK_FALSE;

            VkPipelineColorBlendStateCreateInfo colorBlendState = {};
            colorBlendState.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
            colorBlendState.attachmentCount = 1;
            colorBlendState.pAttachments = &blendAttachment;
            colorBlendState.logicOpEnable = VK_FALSE;

            VkGraphicsPipelineCreateInfo fxaaPipelineDesc = {};
            fxaaPipelineDesc.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
            fxaaPipelineDesc.pNext = &renderingInfo;
            fxaaPipelineDesc.stageCount = 2;
            fxaaPipelineDesc.pStages = shaderStageDesc;
            fxaaPipelineDesc.pVertexInputState = &vertexInputInfo;
            fxaaPipelineDesc.pInputAssemblyState = &inputAssemblyState;
            fxaaPipelineDesc.pTessellationState = nullptr;
            fxaaPipelineDesc.pViewportState = &viewportState;
            fxaaPipelineDesc.pRasterizationState = &rasterizationState;
            fxaaPipelineDesc.pMultisampleState = &multisampleState;
            fxaaPipelineDesc.pDepthStencilState = &depthStencilState;
            fxaaPipelineDesc.pColorBlendState = &colorBlendState;
            fxaaPipelineDesc.pDynamicState = &dynamicState;
            fxaaPipelineDesc.layout = b_pipeline_layout;
            fxaaPipelineDesc.renderPass = VK_NULL_HANDLE;
            fxaaPipelineDesc.subpass = 0;
            fxaaPipelineDesc.flags = 0;

            b_fxaa_pipeline->create(b_device, fxaaPipelineDesc);
        }
    }

}
