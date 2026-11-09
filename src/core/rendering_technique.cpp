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

/* Record this pass after Renderer has transitioned its resources. */
void ShadowDepthTechnique::draw(CommandBuffer &command_buffer)
{
    assert(b_renderer.b_shadow_render_targets.cascadeDepth->currentLayout ==
           VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);

    VkCommandBuffer cmd = command_buffer.handle();
    RenderTarget *shadowDepth = b_renderer.b_shadow_render_targets.cascadeDepth;
    VkViewport shadowViewport = {};
    shadowViewport.width = static_cast<float>(SHADOW_MAP_SIZE);
    shadowViewport.height = static_cast<float>(SHADOW_MAP_SIZE);
    shadowViewport.minDepth = 0.0f;
    shadowViewport.maxDepth = 1.0f;

    VkRect2D shadowScissor = {};
    shadowScissor.extent = {SHADOW_MAP_SIZE, SHADOW_MAP_SIZE};

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, b_renderer.b_shadow_depth_pipeline->handle());
    VkDescriptorSet shadowSets[2] = {b_renderer.b_sets_persistent,
                                     b_renderer.b_sets_per_frame[b_renderer.b_frame_index]};
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, b_renderer.b_pipeline_layout, 0, 2,
                            shadowSets, 0, nullptr);
    VkDeviceSize shadowVertexOffset = 0;
    vkCmdBindVertexBuffers(cmd, 0, 1, &b_renderer.b_model_gltf.vertexBufferVk->buffer, &shadowVertexOffset);

    for (uint32_t cascadeIndex = 0; cascadeIndex < MAX_SHADOW_CASCADES; ++cascadeIndex)
    {
        VkRenderingAttachmentInfo shadowDepthAttachment = {};
        shadowDepthAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
        shadowDepthAttachment.imageView = b_renderer.b_shadow_render_targets.cascadeLayerViews[cascadeIndex];
        shadowDepthAttachment.imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
        shadowDepthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        shadowDepthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        shadowDepthAttachment.clearValue = shadowDepth->clearValue;

        VkRenderingInfo shadowRenderingInfo = {};
        shadowRenderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
        shadowRenderingInfo.renderArea = {{0, 0}, {SHADOW_MAP_SIZE, SHADOW_MAP_SIZE}};
        shadowRenderingInfo.layerCount = 1;
        shadowRenderingInfo.colorAttachmentCount = 0;
        shadowRenderingInfo.pDepthAttachment = &shadowDepthAttachment;

        vkCmdBeginRendering(cmd, &shadowRenderingInfo);
        vkCmdSetViewport(cmd, 0, 1, &shadowViewport);
        vkCmdSetScissor(cmd, 0, 1, &shadowScissor);

        ShadowDepthPushConstants shadowPushConstants = {};
        shadowPushConstants.cascadeIndex = cascadeIndex;
        vkCmdPushConstants(cmd, b_renderer.b_pipeline_layout,
                           VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT |
                               VK_SHADER_STAGE_COMPUTE_BIT,
                           0, sizeof(ShadowDepthPushConstants), &shadowPushConstants);

        if (b_renderer.b_t_filtered_triangles_cascade_shadows[cascadeIndex] &&
            b_renderer.b_t_filter_indirect_args_cascade_shadows[cascadeIndex])
        {
            vkCmdBindIndexBuffer(cmd, b_renderer.b_t_filtered_triangles_cascade_shadows[cascadeIndex]->buffer,
                                 0, VK_INDEX_TYPE_UINT32);
            vkCmdDrawIndexedIndirect(
                cmd, b_renderer.b_t_filter_indirect_args_cascade_shadows[cascadeIndex]->buffer, 0, 1,
                sizeof(VkDrawIndexedIndirectCommand));
        }
        else
        {
            vkCmdBindIndexBuffer(cmd, b_renderer.b_model_gltf.indexBufferVk->buffer, 0, VK_INDEX_TYPE_UINT32);
            vkCmdDrawIndexed(cmd, b_renderer.b_model_gltf.indextotalCount, 1, 0, 0, 0);
        }

        vkCmdEndRendering(cmd);
    }
}

/* Record this pass after Renderer has transitioned its resources. */
void GBufferTechnique::draw(CommandBuffer &command_buffer)
{
    assert(b_renderer.b_g_buffer_normal_specular_rt->currentLayout ==
           VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    assert(b_renderer.b_g_buffer_albedo_rt->currentLayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    assert(b_renderer.b_depth_buffer_rt->currentLayout == VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);
    if (b_renderer.b_model_gltf.image_count == 0)
    {
        assert(b_renderer.b_default_mesh_texture->currentLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    }
    else
    {
        for (size_t i = 0; i < b_renderer.b_model_gltf.image_count; ++i)
            assert(b_renderer.b_model_gltf.images[i].texture.currentLayout ==
                   VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    }

    VkCommandBuffer cmd = command_buffer.handle();
    VkRenderingAttachmentInfo gbufferColorAttachments[2] = {};
    gbufferColorAttachments[0].sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    gbufferColorAttachments[0].imageView = b_renderer.b_g_buffer_normal_specular_rt->imageView;
    gbufferColorAttachments[0].imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    gbufferColorAttachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    gbufferColorAttachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    gbufferColorAttachments[0].clearValue = b_renderer.b_g_buffer_normal_specular_rt->clearValue;

    gbufferColorAttachments[1].sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    gbufferColorAttachments[1].imageView = b_renderer.b_g_buffer_albedo_rt->imageView;
    gbufferColorAttachments[1].imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    gbufferColorAttachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    gbufferColorAttachments[1].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    gbufferColorAttachments[1].clearValue = b_renderer.b_g_buffer_albedo_rt->clearValue;

    VkRenderingAttachmentInfo depthAttachment = {};
    depthAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    depthAttachment.imageView = b_renderer.b_depth_buffer_rt->imageView;
    depthAttachment.imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
    depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    depthAttachment.clearValue = {.depthStencil = {1.0f, 0}};

    VkRenderingInfo gbufferRenderingInfo = {};
    gbufferRenderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
    gbufferRenderingInfo.renderArea = {{0, 0},
                                       {b_renderer.b_app_settings->width, b_renderer.b_app_settings->height}};
    gbufferRenderingInfo.layerCount = 1;
    gbufferRenderingInfo.colorAttachmentCount = 2;
    gbufferRenderingInfo.pColorAttachments = gbufferColorAttachments;
    gbufferRenderingInfo.pDepthAttachment = &depthAttachment;

    VkViewport viewport = {};
    viewport.width = (float)b_renderer.b_app_settings->width;
    viewport.height = (float)b_renderer.b_app_settings->height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    VkRect2D scissor = {};
    scissor.extent = {b_renderer.b_app_settings->width, b_renderer.b_app_settings->height};

    vkCmdBeginRendering(cmd, &gbufferRenderingInfo);
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, b_renderer.b_g_buffer_pipeline->handle());
    vkCmdSetViewport(cmd, 0, 1, &viewport);
    vkCmdSetScissor(cmd, 0, 1, &scissor);

    VkDescriptorSet gbufferSets[2] = {b_renderer.b_sets_persistent,
                                      b_renderer.b_sets_per_frame[b_renderer.b_frame_index]};
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, b_renderer.b_pipeline_layout, 0, 2,
                            gbufferSets, 0, nullptr);

    VkDeviceSize offset = 0;

    {
        vkCmdSetDepthWriteEnable(cmd, VK_TRUE);
        VkBool32 colorWriteEnables[2] = {VK_TRUE, VK_TRUE};
        b_renderer.b_pfn_cmd_set_color_write_enable_ext(cmd, 2, colorWriteEnables);
    }

    vkCmdBindVertexBuffers(cmd, 0, 1, &b_renderer.b_model_gltf.vertexBufferVk->buffer, &offset);
    if (b_renderer.b_t_filtered_triangles && b_renderer.b_t_filter_indirect_args)
    {
        vkCmdBindIndexBuffer(cmd, b_renderer.b_t_filtered_triangles->buffer, 0, VK_INDEX_TYPE_UINT32);
        vkCmdDrawIndexedIndirect(cmd, b_renderer.b_t_filter_indirect_args->buffer, 0, 1,
                                 sizeof(VkDrawIndexedIndirectCommand));
    }
    else
    {
        vkCmdBindIndexBuffer(cmd, b_renderer.b_model_gltf.indexBufferVk->buffer, 0, VK_INDEX_TYPE_UINT32);
        vkCmdDrawIndexed(cmd, b_renderer.b_model_gltf.indextotalCount, 1, 0, 0, 0);
    }

    vkCmdEndRendering(cmd);
}

