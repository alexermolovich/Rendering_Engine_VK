#pragma once
#include "vulkan_check.hpp"
#include <cstdint>

/* Distinct bits identify the passes a pipeline supports. */
enum class TechniqueFlags : uint32_t
{
    none = 0,
    depth_buffering = 1u << 0,
    g_buffer = 1u << 1,
    triangle_filtering = 1u << 2,
    cascade_filtering = 1u << 3,
    shadow_depth = 1u << 4,
    ssao = 1u << 5,
    ssao_blur = 1u << 6,
    reflections = 1u << 7,
    composition = 1u << 8,
    fxaa = 1u << 9
};

enum class PipelineType : uint32_t
{
    graphics,
    compute
};

constexpr TechniqueFlags operator|(TechniqueFlags left, TechniqueFlags right)
{
    return static_cast<TechniqueFlags>(static_cast<uint32_t>(left) | static_cast<uint32_t>(right));
}

/* Pack type and supported flags before hashing, preserving every combination. */
constexpr uint64_t pipeline_hash(PipelineType type, TechniqueFlags flags)
{
    const uint64_t identity = (uint64_t(static_cast<uint32_t>(type)) << 32) | static_cast<uint32_t>(flags);
    return identity * 0x9e3779b97f4a7c15ULL;
}

/* Graphics and compute pipeline handles share one explicit lifetime contract. */
class GraphicsPipeline
{
  public:
    GraphicsPipeline()
    {
    }
    GraphicsPipeline(PipelineType type, TechniqueFlags flags) : b_type(type), b_flags(flags)
    {
    }
    ~GraphicsPipeline() noexcept(false)
    {
        require_destroyed(!b_handle, "GraphicsPipeline");
    }
    GraphicsPipeline(const GraphicsPipeline &) = delete;
    GraphicsPipeline &operator=(const GraphicsPipeline &) = delete;
    void configure(PipelineType type, TechniqueFlags flags)
    {
        require_destroyed(!b_handle, "GraphicsPipeline configuration");
        b_type = type;
        b_flags = flags;
    }
    VkPipeline handle() const
    {
        return b_handle;
    }
    PipelineType type() const
    {
        return b_type;
    }
    TechniqueFlags flags() const
    {
        return b_flags;
    }
    VkPipeline *put()
    {
        require_destroyed(!b_handle, "GraphicsPipeline replacement");
        return &b_handle;
    }
    /* Create only the pipeline kind registered in the renderer. */
    void create(VkDevice device, const VkGraphicsPipelineCreateInfo &info)
    {
        require(b_type == PipelineType::graphics, "Graphics description used for compute pipeline");
        VK_CHECK(vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &info, nullptr, put()));
    }
    void create(VkDevice device, const VkComputePipelineCreateInfo &info)
    {
        require(b_type == PipelineType::compute, "Compute description used for graphics pipeline");
        VK_CHECK(vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &info, nullptr, put()));
    }
    void destroy(VkDevice device)
    {
        if (b_handle)
            vkDestroyPipeline(device, b_handle, nullptr);
        b_handle = VK_NULL_HANDLE;
    }

  private:
    VkPipeline b_handle = VK_NULL_HANDLE;
    PipelineType b_type = PipelineType::graphics;
    TechniqueFlags b_flags = TechniqueFlags::none;
};
