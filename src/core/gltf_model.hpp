#pragma once
#include "resources.hpp"
#include <cstring>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <memory>
#include <stdexcept>
#include <tiny_gltf.h>
#include <vector>

/* Scene geometry, materials, and uploaded textures. */
class GltfModel
{
  public:
    GltfModel()
    {
    }

    struct Vertex
    {
        glm::vec3 pos;
        glm::vec3 normal;
        glm::vec2 uv;
        glm::vec3 color;
        uint32_t textureIndex;
    };
    struct Node;
    struct Primitive
    {
        uint32_t firstIndex;
        uint32_t indexCount;
        int32_t materialIndex;
    };

    uint32_t vertextotalCount = 0;
    uint32_t indextotalCount = 0;
    struct Mesh
    {
        std::vector<Primitive> primitives;
    };
    struct Node
    {
        Node *parent;
        std::vector<Node *> children;
        Mesh mesh;
        glm::mat4 matrix;
        ~Node()
        {
            for (auto &child : children)
            {
                delete child;
            }
        }
    };
    struct Material
    {
        glm::vec4 baseColorFactor = glm::vec4(1.0f);
        uint32_t baseColorTextureIndex = 0;
    };

    struct ModelImage
    {
        ::Image texture;
        VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
    };
    struct Texture
    {
        int32_t imageIndex;
    };

    ModelImage images[BISTRO_TEXTURE_COUNT];
    size_t image_count = 0;
    std::vector<Texture> textures;
    std::vector<Material> materials;
    std::vector<Node *> nodes;
    Buffer *vertexBufferVk = nullptr;
    Buffer *indexBufferVk = nullptr;

    ~GltfModel()
    {
        for (auto *node : nodes)
            delete node;
    }

    void destroy(VkDevice device)
    {
        for (size_t i = 0; i < image_count; ++i)
            images[i].texture.destroy(device);
        image_count = 0;
        for (auto *node : nodes)
            delete node;
        nodes.clear();
        materials.clear();
        textures.clear();
        vertexBufferVk = nullptr;
        indexBufferVk = nullptr;
        vertextotalCount = 0;
        indextotalCount = 0;
    }

    /* Convert source pixels to RGBA before uploading scene images. */
    void load_images(tinygltf::Model &input, VkDevice device, VkPhysicalDevice physicalDevice,
                     VkQueue graphicsQueue, VkCommandPool graphicsCommandPool)
    {
        require(image_count == 0, "Scene images have already been allocated");
        require(input.images.size() <= BISTRO_TEXTURE_COUNT, "Scene texture array capacity exceeded");
        image_count = input.images.size();
        for (size_t i = 0; i < input.images.size(); i++)
        {
            tinygltf::Image &glTFImage = input.images[i];
            unsigned char *buffer = nullptr;
            VkDeviceSize bufferSize = 0;
            unsigned char *converted = nullptr;
            if (glTFImage.component == 3)
            {
                bufferSize = glTFImage.width * glTFImage.height * 4;
                converted = new unsigned char[bufferSize];
                buffer = converted;
                unsigned char *rgba = buffer;
                unsigned char *rgb = &glTFImage.image[0];
                for (size_t i = 0; i < glTFImage.width * glTFImage.height; ++i)
                {
                    memcpy(rgba, rgb, sizeof(unsigned char) * 3);
                    rgba[3] = 255;
                    rgba += 4;
                    rgb += 3;
                }
            }
            else
            {
                buffer = &glTFImage.image[0];
                bufferSize = glTFImage.image.size();
            }
            try
            {
                images[i].texture.from_buffer(buffer, bufferSize, VK_FORMAT_R8G8B8A8_UNORM, glTFImage.width,
                                              glTFImage.height, device, physicalDevice, graphicsQueue,
                                              graphicsCommandPool);
            }
            catch (...)
            {
                delete[] converted;
                throw;
            }
            delete[] converted;
        }
    }

    /* Resolve material texture slots to scene images. */
    void load_textures(tinygltf::Model &input)
    {
        textures.resize(input.textures.size());
        for (size_t i = 0; i < input.textures.size(); i++)
        {
            textures[i].imageIndex = input.textures[i].source;
        }
    }

    /* Read the base colors used by the G-buffer. */
    void load_materials(tinygltf::Model &input)
    {
        materials.resize(input.materials.size());
        for (size_t i = 0; i < input.materials.size(); i++)
        {
            tinygltf::Material glTFMaterial = input.materials[i];
            if (glTFMaterial.values.find("baseColorFactor") != glTFMaterial.values.end())
            {
                materials[i].baseColorFactor =
                    glm::make_vec4(glTFMaterial.values["baseColorFactor"].ColorFactor().data());
            }
            if (glTFMaterial.values.find("baseColorTexture") != glTFMaterial.values.end())
            {
                materials[i].baseColorTextureIndex = glTFMaterial.values["baseColorTexture"].TextureIndex();
            }
        }
    }

    /* Flatten primitive data while retaining the scene hierarchy. */
    void load_node(const tinygltf::Node &inputNode, const tinygltf::Model &input, GltfModel::Node *parent,
                   std::vector<uint32_t> &indexBuffer, std::vector<GltfModel::Vertex> &vertexBuffer)
    {
        auto node_owner = std::make_unique<Node>();
        Node *node = node_owner.get();
        node->matrix = glm::mat4(1.0f);
        node->parent = parent;

        if (inputNode.translation.size() == 3)
        {
            node->matrix =
                glm::translate(node->matrix, glm::vec3(glm::make_vec3(inputNode.translation.data())));
        }
        if (inputNode.rotation.size() == 4)
        {
            glm::quat q = glm::make_quat(inputNode.rotation.data());
            node->matrix *= glm::mat4(q);
        }
        if (inputNode.scale.size() == 3)
        {
            node->matrix = glm::scale(node->matrix, glm::vec3(glm::make_vec3(inputNode.scale.data())));
        }
        if (inputNode.matrix.size() == 16)
        {
            node->matrix = glm::make_mat4x4(inputNode.matrix.data());
        };

        if (inputNode.children.size() > 0)
        {
            for (size_t i = 0; i < inputNode.children.size(); i++)
            {
                load_node(input.nodes[inputNode.children[i]], input, node, indexBuffer, vertexBuffer);
            }
        }

        if (inputNode.mesh > -1)
        {
            const tinygltf::Mesh mesh = input.meshes[inputNode.mesh];
            for (size_t i = 0; i < mesh.primitives.size(); i++)
            {
                const tinygltf::Primitive &glTFPrimitive = mesh.primitives[i];
                uint32_t firstIndex = static_cast<uint32_t>(indexBuffer.size());
                uint32_t vertexStart = static_cast<uint32_t>(vertexBuffer.size());
                uint32_t indexCount = 0;
                uint32_t textureIndex = 0;
                if (glTFPrimitive.material >= 0 &&
                    static_cast<size_t>(glTFPrimitive.material) < materials.size())
                {
                    uint32_t gltfTextureIndex = materials[glTFPrimitive.material].baseColorTextureIndex;
                    if (gltfTextureIndex < textures.size() && textures[gltfTextureIndex].imageIndex >= 0)
                    {
                        textureIndex = static_cast<uint32_t>(textures[gltfTextureIndex].imageIndex);
                    }
                }
                {
                    const float *positionBuffer = nullptr;
                    const float *normalsBuffer = nullptr;
                    const float *texCoordsBuffer = nullptr;
                    size_t vertexCount = 0;
                    if (glTFPrimitive.attributes.find("POSITION") != glTFPrimitive.attributes.end())
                    {
                        const tinygltf::Accessor &accessor =
                            input.accessors[glTFPrimitive.attributes.find("POSITION")->second];
                        const tinygltf::BufferView &view = input.bufferViews[accessor.bufferView];
                        positionBuffer = reinterpret_cast<const float *>(
                            &(input.buffers[view.buffer].data[accessor.byteOffset + view.byteOffset]));
                        vertexCount = accessor.count;
                    }
                    if (glTFPrimitive.attributes.find("NORMAL") != glTFPrimitive.attributes.end())
                    {
                        const tinygltf::Accessor &accessor =
                            input.accessors[glTFPrimitive.attributes.find("NORMAL")->second];
                        const tinygltf::BufferView &view = input.bufferViews[accessor.bufferView];
                        normalsBuffer = reinterpret_cast<const float *>(
                            &(input.buffers[view.buffer].data[accessor.byteOffset + view.byteOffset]));
                    }
                    if (glTFPrimitive.attributes.find("TEXCOORD_0") != glTFPrimitive.attributes.end())
                    {
                        const tinygltf::Accessor &accessor =
                            input.accessors[glTFPrimitive.attributes.find("TEXCOORD_0")->second];
                        const tinygltf::BufferView &view = input.bufferViews[accessor.bufferView];
                        texCoordsBuffer = reinterpret_cast<const float *>(
                            &(input.buffers[view.buffer].data[accessor.byteOffset + view.byteOffset]));
                    }
                    for (size_t v = 0; v < vertexCount; v++)
                    {
                        Vertex vert{};
                        vert.pos = glm::vec4(glm::make_vec3(&positionBuffer[v * 3]), 1.0f);
                        vert.normal = glm::normalize(glm::vec3(
                            normalsBuffer ? glm::make_vec3(&normalsBuffer[v * 3]) : glm::vec3(0.0f)));
                        vert.uv = texCoordsBuffer ? glm::make_vec2(&texCoordsBuffer[v * 2]) : glm::vec3(0.0f);
                        vert.color = glm::vec3(1.0f);
                        vert.textureIndex = textureIndex;
                        vertexBuffer.push_back(vert);
                    }
                }
                {
                    const tinygltf::Accessor &accessor = input.accessors[glTFPrimitive.indices];
                    const tinygltf::BufferView &bufferView = input.bufferViews[accessor.bufferView];
                    const tinygltf::Buffer &buffer = input.buffers[bufferView.buffer];

                    indexCount += static_cast<uint32_t>(accessor.count);
                    switch (accessor.componentType)
                    {
                    case TINYGLTF_PARAMETER_TYPE_UNSIGNED_INT: {
                        const uint32_t *buf = reinterpret_cast<const uint32_t *>(
                            &buffer.data[accessor.byteOffset + bufferView.byteOffset]);
                        for (size_t index = 0; index < accessor.count; index++)
                        {
                            indexBuffer.push_back(buf[index] + vertexStart);
                        }
                        break;
                    }
                    case TINYGLTF_PARAMETER_TYPE_UNSIGNED_SHORT: {
                        const uint16_t *buf = reinterpret_cast<const uint16_t *>(
                            &buffer.data[accessor.byteOffset + bufferView.byteOffset]);
                        for (size_t index = 0; index < accessor.count; index++)
                        {
                            indexBuffer.push_back(buf[index] + vertexStart);
                        }
                        break;
                    }
                    case TINYGLTF_PARAMETER_TYPE_UNSIGNED_BYTE: {
                        const uint8_t *buf = reinterpret_cast<const uint8_t *>(
                            &buffer.data[accessor.byteOffset + bufferView.byteOffset]);
                        for (size_t index = 0; index < accessor.count; index++)
                        {
                            indexBuffer.push_back(buf[index] + vertexStart);
                        }
                        break;
                    }
                    default:
                        throw std::runtime_error("Unsupported glTF index component type: " +
                                                 std::to_string(accessor.componentType));
                    }
                }
                Primitive primitive{};
                primitive.firstIndex = firstIndex;
                primitive.indexCount = indexCount;
                primitive.materialIndex = glTFPrimitive.material;
                node->mesh.primitives.push_back(primitive);
            }
        }

        if (parent)
        {
            parent->children.push_back(node);
            node_owner.release();
        }
        else
        {
            nodes.push_back(node);
            node_owner.release();
        }
    }
};
