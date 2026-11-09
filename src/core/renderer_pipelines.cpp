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

}
