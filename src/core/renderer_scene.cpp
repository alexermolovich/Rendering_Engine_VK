#include "renderer.hpp"
#include "vulkan_check.hpp"
#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <glm/gtc/type_ptr.hpp>
#include <meshoptimizer.h>
#include <random>
#include <stb_image.h>

/* Setup ubo. */
void Renderer::setup_ubo()
{
    b_camera.set_position(glm::vec3(0.0f, 2.0f, 10.0f));
    b_camera.set_direction(glm::vec3(0.0f, 0.0f, -1.0f));
    b_camera.set_world_up(glm::vec3(0.0f, 1.0f, 0.0f));
    b_camera.change_lens(45.0f, 0.1f, 4000.0f);

    b_camera.update_aspect_ratio((float)b_app_settings->width / (float)b_app_settings->height);
    b_camera.update_proj_matrix();
    b_camera.update_view_matrix();
    b_uniform_data.cameraPos = b_camera.pos;
    b_uniform_data.nearPlane = 0.1f;
    b_uniform_data.farPlane = 4000.0f;
    b_uniform_data.model = b_identity_matrix;
    b_uniform_data.proj = b_camera.get_proj_matrix();
    b_uniform_data.invProjection = glm::inverse(b_uniform_data.proj);
    b_uniform_data.view = b_camera.get_view_matrix();
}

/* Update cascades. */
void Renderer::update_cascades(const Camera &b_camera, const glm::vec3 &lightDir, ShadowCascadeData *cascades,
                               uint32_t cascadeCount)
{
    cascadeCount = std::min<uint32_t>(cascadeCount, MAX_SHADOW_CASCADES);
    float nearPlane = b_camera.nearPlane;
    float farPlane = b_camera.farPlane;

    for (uint32_t i = 0; i < cascadeCount; ++i)
    {

        float p = float(i + 1) / float(cascadeCount);
        cascades[i].splitDepth = nearPlane + (farPlane - nearPlane) * p;
        glm::vec3 corners[8];
        b_camera.get_frustum_corners_world_space((i == 0) ? nearPlane : cascades[i - 1].splitDepth,
                                                 cascades[i].splitDepth, corners);
        glm::vec3 center(0.0f);
        for (int c = 0; c < 8; ++c)
            center += corners[c];
        center /= 8.0f;

        glm::vec3 lightPos = center - lightDir * 100.0f;
        glm::mat4 lightView = glm::lookAt(lightPos, center, glm::vec3(0.0f, 1.0f, 0.0f));
        float minX = FLT_MAX, minY = FLT_MAX, minZ = FLT_MAX;
        float maxX = -FLT_MAX, maxY = -FLT_MAX, maxZ = -FLT_MAX;

        for (int c = 0; c < 8; ++c)
        {
            glm::vec4 tr = lightView * glm::vec4(corners[c], 1.0f);
            minX = std::min(minX, tr.x);
            maxX = std::max(maxX, tr.x);
            minY = std::min(minY, tr.y);
            maxY = std::max(maxY, tr.y);
            minZ = std::min(minZ, tr.z);
            maxZ = std::max(maxZ, tr.z);
        }

        glm::mat4 lightProj = glm::ortho(minX, maxX, minY, maxY, -maxZ - 50.0f, -minZ);
        cascades[i].viewProj = lightProj * lightView;
    }
}

/* Update shadow generation data. */
void Renderer::update_shadow_generation_data(bool shadowRenderTargetsReady)
{
    auto &uniform = b_shadow_gen_uniform_data;
    const uint32_t cascadeCount = MAX_SHADOW_CASCADES;

    uniform.lightDirection = glm::vec4(b_sky_light_uniform_data.lightDirection, 0.0f);
    uniform.shadowMapSizeCascadeCount =
        glm::uvec4(SHADOW_MAP_SIZE, SHADOW_MAP_SIZE, cascadeCount, shadowRenderTargetsReady ? 1u : 0u);
    uniform.bias = glm::vec4(0.0005f, 0.0f, 0.0f, 0.0f);

    update_cascades(b_camera, b_sky_light_uniform_data.lightDirection, uniform.cascades,
                    cascadeCount);
}

/* Init sky light. */
void Renderer::init_sky_light()
{

    setup_ubo();

    b_sky_light_uniform_data.lightColor = glm::vec3(1.0f);
    b_sky_light_uniform_data.lightDirection = normalize(glm::vec3(-1, -1, -1));
    b_sky_light_uniform_data.lightIntensity = 1.0f;
}

/* Load sponza. */
void Renderer::load_sponza()
{

    tinygltf::Model model;
    tinygltf::TinyGLTF loader;
    std::string err;
    std::string warn;
    const std::filesystem::path filenamePath =
        std::filesystem::path(VKML_SOURCE_DIR) / "external/gltf-sample-assets/Models/Sponza/glTF/Sponza.gltf";
    const std::string filename = filenamePath.string();

    if (!std::filesystem::is_regular_file(filenamePath))
    {
        throw std::runtime_error("Sponza asset was not found at: " + filename);
    }

    bool ret = loader.LoadASCIIFromFile(&model, &err, &warn, filename);

    if (!warn.empty())
    {
        spdlog::warn("{}", warn);
    }

    if (!err.empty())
    {
        spdlog::error("{}", err);
    }

    if (!ret)
    {
        spdlog::error("Failed to parse glTF: {}", filename);
        throw std::runtime_error("Failed to parse glTF: " + filename);
    }

    b_model_gltf.nodes.reserve(model.nodes.size());
    std::vector<uint32_t> indexBuffer;
    std::vector<GltfModel::Vertex> vertexBuffer;

    b_model_gltf.load_materials(model);
    b_model_gltf.load_textures(model);
    b_model_gltf.load_images(model, b_device, b_physical_device, b_graphics_queue, b_graphics_command_pool);

    for (int cur_node : model.scenes.at(model.defaultScene >= 0 ? model.defaultScene : 0).nodes)
    {
        b_model_gltf.load_node(model.nodes[cur_node], // const tinygltf::Node&
                               model,                 // const tinygltf::Model&
                               nullptr,               // Node* parent  (null = root level)
                               indexBuffer, vertexBuffer);
    }

    b_model_gltf.indextotalCount = indexBuffer.size();
    b_model_gltf.vertextotalCount = vertexBuffer.size();

    size_t index_count = indexBuffer.size();
    size_t vertex_count = vertexBuffer.size();

    std::vector<unsigned int> remap(index_count);

    size_t unique_vertex_count =
        meshopt_generateVertexRemap(remap.data(), indexBuffer.data(), index_count, vertexBuffer.data(),
                                    vertex_count, sizeof(GltfModel::Vertex));

    std::vector<unsigned int> new_indices(index_count);
    meshopt_remapIndexBuffer(new_indices.data(), indexBuffer.data(), index_count, remap.data());

    std::vector<GltfModel::Vertex> new_vertices(unique_vertex_count);
    meshopt_remapVertexBuffer(new_vertices.data(), vertexBuffer.data(), vertex_count,
                              sizeof(GltfModel::Vertex), remap.data());

    indexBuffer = std::move(new_indices);
    vertexBuffer = std::move(new_vertices);
    vertex_count = unique_vertex_count;
    b_model_gltf.vertextotalCount = static_cast<uint32_t>(vertex_count);

    {
        {
            BufferDesc sponzaVertexBufDesc = {};
            sponzaVertexBufDesc.size = sizeof(GltfModel::Vertex) * vertexBuffer.size();
            sponzaVertexBufDesc.sharingMode = VkSharingMode::VK_SHARING_MODE_EXCLUSIVE;
            sponzaVertexBufDesc.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
                                        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                                        VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
            sponzaVertexBufDesc.memoryProperties =
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;

            add_buffer(b_device, b_physical_device, sponzaVertexBufDesc, &b_model_gltf.vertexBufferVk);

            map_buffer(b_device, b_model_gltf.vertexBufferVk, vertexBuffer.data());
        }
        {
            BufferDesc sponzaIndexBuffer = {};

            sponzaIndexBuffer.size = sizeof(uint32_t) * indexBuffer.size();
            sponzaIndexBuffer.sharingMode = VkSharingMode::VK_SHARING_MODE_EXCLUSIVE;
            sponzaIndexBuffer.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                                      VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
            sponzaIndexBuffer.memoryProperties =
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;

            add_buffer(b_device, b_physical_device, sponzaIndexBuffer, &b_model_gltf.indexBufferVk);

            map_buffer(b_device, b_model_gltf.indexBufferVk, indexBuffer.data());
        }

    }

    b_vertex_buffer_address = 0;
}
