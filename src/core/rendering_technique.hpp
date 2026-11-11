#pragma once
#include "resources.hpp"

class Renderer;

/* Techniques record draw commands; Renderer owns resources, transitions, and pass order. */
class RenderingTechnique
{
  public:
    virtual ~RenderingTechnique()
    {
    }
    virtual void draw(CommandBuffer &command_buffer) = 0;

  protected:
    explicit RenderingTechnique(Renderer &renderer) : b_renderer(renderer)
    {
    }
    Renderer &b_renderer;
};

/* Records the CascadeFiltering pass. */
class CascadeFilteringTechnique final : public RenderingTechnique
{
  public:
    explicit CascadeFilteringTechnique(Renderer &renderer) : RenderingTechnique(renderer)
    {
    }
    void draw(CommandBuffer &command_buffer) override;
};

/* Records the TriangleFiltering pass. */
class TriangleFilteringTechnique final : public RenderingTechnique
{
  public:
    explicit TriangleFilteringTechnique(Renderer &renderer) : RenderingTechnique(renderer)
    {
    }
    void draw(CommandBuffer &command_buffer) override;
};

/* Records the ShadowDepth pass. */
class ShadowDepthTechnique final : public RenderingTechnique
{
  public:
    explicit ShadowDepthTechnique(Renderer &renderer) : RenderingTechnique(renderer)
    {
    }
    void draw(CommandBuffer &command_buffer) override;
};

/* Records the GBuffer pass. */
class GBufferTechnique final : public RenderingTechnique
{
  public:
    explicit GBufferTechnique(Renderer &renderer) : RenderingTechnique(renderer)
    {
    }
    void draw(CommandBuffer &command_buffer) override;
};

/* Records the Ssao pass. */
class SsaoTechnique final : public RenderingTechnique
{
  public:
    explicit SsaoTechnique(Renderer &renderer) : RenderingTechnique(renderer)
    {
    }
    void draw(CommandBuffer &command_buffer) override;
};

/* Records the Reflections pass. */
class ReflectionsTechnique final : public RenderingTechnique
{
  public:
    explicit ReflectionsTechnique(Renderer &renderer) : RenderingTechnique(renderer)
    {
    }
    void draw(CommandBuffer &command_buffer) override;
};

/* Records the Compose pass. */
class ComposeTechnique final : public RenderingTechnique
{
  public:
    explicit ComposeTechnique(Renderer &renderer) : RenderingTechnique(renderer)
    {
    }
    void draw(CommandBuffer &command_buffer) override;
};

/* Records the Fxaa pass. */
class FxaaTechnique final : public RenderingTechnique
{
  public:
    explicit FxaaTechnique(Renderer &renderer) : RenderingTechnique(renderer)
    {
    }
    void draw(CommandBuffer &command_buffer) override;
};

/* Records the SsaoBlur pass. */
class SsaoBlurTechnique final : public RenderingTechnique
{
  public:
    explicit SsaoBlurTechnique(Renderer &renderer) : RenderingTechnique(renderer)
    {
    }
    void draw(CommandBuffer &command_buffer) override;
};

class SkyboxTechnique: public RenderingTechnique
{
  public:
    explicit SkyboxTechnique(Renderer &renderer) : RenderingTechnique(renderer)
    {
    }
    void draw(CommandBuffer &command_buffer) override;
};
