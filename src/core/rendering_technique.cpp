#include "renderer.hpp"
#include "vulkan_check.hpp"
#include <algorithm>
#include <cassert>
#include <cfloat>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <glm/gtc/type_ptr.hpp>
#include <meshoptimizer/src/meshoptimizer.h>
#include <random>
#include <stb_image.h>

/* Record this pass after Renderer has transitioned its resources. */
void CascadeFilteringTechnique::draw(CommandBuffer &command_buffer)
{
    VkCommandBuffer cmd = command_buffer.handle();
    VkDescriptorSet sets[] = {b_renderer.b_sets_persistent,
                              b_renderer.b_sets_per_frame[b_renderer.b_frame_index]};
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, b_renderer.b_pipeline_layout, 0, 2, sets, 0,
                            nullptr);
    auto get_buffer_address = [this](VkBuffer buffer) {
        VkBufferDeviceAddressInfo addressInfo = {};
        addressInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
        addressInfo.buffer = buffer;
        return vkGetBufferDeviceAddress(b_renderer.b_device, &addressInfo);
    };

    TriangleFilteringPushConstantsCascadeShadows filteringPushConstants = {};
    filteringPushConstants.indexBuffer = get_buffer_address(b_renderer.b_model_gltf.indexBufferVk->buffer);
    filteringPushConstants.vertexBuffer = get_buffer_address(b_renderer.b_model_gltf.vertexBufferVk->buffer);

    bool hasCascadeShadowFilteringBuffers = true;
    for (int32_t cascade_index = 0; cascade_index < MAX_SHADOW_CASCADES; ++cascade_index)
    {
        if (b_renderer.b_t_filtered_triangles_cascade_shadows[cascade_index])
        {
            filteringPushConstants.filteredBuffers[cascade_index] =
                get_buffer_address(b_renderer.b_t_filtered_triangles_cascade_shadows[cascade_index]->buffer);
        }
        else
        {
            hasCascadeShadowFilteringBuffers = false;
        }

        if (b_renderer.b_t_filter_indirect_args_cascade_shadows[cascade_index])
        {
            filteringPushConstants.drawCommands[cascade_index] = get_buffer_address(
                b_renderer.b_t_filter_indirect_args_cascade_shadows[cascade_index]->buffer);
        }
        else
        {
            hasCascadeShadowFilteringBuffers = false;
        }
    }

    filteringPushConstants.modelViewProjection =
        b_renderer.b_uniform_data.proj * b_renderer.b_uniform_data.view * b_renderer.b_uniform_data.model;
    filteringPushConstants.vertexStride = sizeof(GltfModel::Vertex);
    filteringPushConstants.indexCount = b_renderer.b_model_gltf.indextotalCount;
    filteringPushConstants.vertexCount = b_renderer.b_model_gltf.vertextotalCount;

    constexpr uint32_t filteringThreadGroupSize = 256;
    const uint32_t triangleCount = (b_renderer.b_model_gltf.indextotalCount + 2) / 3;
    const uint32_t groupCount = (triangleCount + filteringThreadGroupSize - 1) / filteringThreadGroupSize;

    if (b_renderer.b_t_filtering_pipeline_cascade_shadow_mapping && hasCascadeShadowFilteringBuffers &&
        groupCount > 0)
    {
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, b_renderer.b_t_filtering_pipeline_cascade_shadow_mapping->handle());
        vkCmdPushConstants(cmd, b_renderer.b_pipeline_layout,
                           VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_FRAGMENT_BIT |
                               VK_SHADER_STAGE_VERTEX_BIT,
                           0, sizeof(TriangleFilteringPushConstantsCascadeShadows), &filteringPushConstants);

        vkCmdDispatch(cmd, groupCount, 1, 1);
    }
}

/* Record this pass after Renderer has transitioned its resources. */
void TriangleFilteringTechnique::draw(CommandBuffer &command_buffer)
{
    VkCommandBuffer cmd = command_buffer.handle();
    VkDescriptorSet sets[] = {b_renderer.b_sets_persistent,
                              b_renderer.b_sets_per_frame[b_renderer.b_frame_index]};
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, b_renderer.b_pipeline_layout, 0, 2, sets, 0,
                            nullptr);
    auto get_buffer_address = [this](VkBuffer buffer) {
        VkBufferDeviceAddressInfo addressInfo = {};
        addressInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
        addressInfo.buffer = buffer;
        return vkGetBufferDeviceAddress(b_renderer.b_device, &addressInfo);
    };

    TriangleFilteringPushConstants filteringPushConstants = {};

    filteringPushConstants.indexBuffer = get_buffer_address(b_renderer.b_model_gltf.indexBufferVk->buffer);
    filteringPushConstants.vertexBuffer = get_buffer_address(b_renderer.b_model_gltf.vertexBufferVk->buffer);

    if (b_renderer.b_t_filtered_triangles)
    {
        filteringPushConstants.filteredBuffers =
            get_buffer_address(b_renderer.b_t_filtered_triangles->buffer);
    }
    if (b_renderer.b_t_filter_indirect_args)
    {
        filteringPushConstants.drawCommand = get_buffer_address(b_renderer.b_t_filter_indirect_args->buffer);
    }
    filteringPushConstants.modelViewProjection =
        b_renderer.b_uniform_data.proj * b_renderer.b_uniform_data.view * b_renderer.b_uniform_data.model;
    filteringPushConstants.vertexStride = sizeof(GltfModel::Vertex);
    filteringPushConstants.indexCount = b_renderer.b_model_gltf.indextotalCount;
    filteringPushConstants.vertexCount = b_renderer.b_model_gltf.vertextotalCount;

    constexpr uint32_t filteringThreadGroupSize = 256;
    const uint32_t triangleCount = (b_renderer.b_model_gltf.indextotalCount + 2) / 3;
    const uint32_t groupCount = (triangleCount + filteringThreadGroupSize - 1) / filteringThreadGroupSize;

    if (b_renderer.b_t_filtering_pipeline && b_renderer.b_t_filtered_triangles &&
        b_renderer.b_t_filter_indirect_args && groupCount > 0)
    {
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, b_renderer.b_t_filtering_pipeline->handle());
        vkCmdPushConstants(cmd, b_renderer.b_pipeline_layout,
                           VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_FRAGMENT_BIT |
                               VK_SHADER_STAGE_VERTEX_BIT,
                           0, sizeof(TriangleFilteringPushConstants), &filteringPushConstants);

        vkCmdDispatch(cmd, groupCount, 1, 1);
    }
}

