#pragma once
#include "camera.hpp"
#include "render_types.hpp"
#include "vulkan_check.hpp"
#include <deque>
#include <future>
#include <utility>

/* Workers publish scene, camera, and lighting snapshots without sharing mutable state. */
struct PreparedFrame
{
    uint64_t sequence = 0;
    Camera camera;
    SceneBounds sceneBounds = {};
    Uniform uniform = {};
    ShadowGenerationUniform shadows = {};
    SkyLightUniform sky = {};
};

/* Bound CPU work and consume it in submission order, even when workers finish out of order. */
class FramePreparationQueue
{
  public:
    FramePreparationQueue()
    {
    }

    template <class Prepare> void enqueue(Prepare &&prepare)
    {
        require(b_pending.size() < FRAMES_IN_FLIGHT, "Frame preparation queue is full");
        b_pending.push_back(std::async(std::launch::async, std::forward<Prepare>(prepare)));
    }

    /* Worker exceptions propagate on the renderer thread. */
    PreparedFrame pop()
    {
        require(!b_pending.empty(), "Frame preparation queue is empty");
        auto next = std::move(b_pending.front());
        b_pending.pop_front();
        return next.get();
    }

    void clear()
    {
        for (auto &pending : b_pending)
            pending.wait();
        b_pending.clear();
    }

    size_t size() const
    {
        return b_pending.size();
    }
    bool empty() const
    {
        return b_pending.empty();
    }

  private:
    std::deque<std::future<PreparedFrame>> b_pending;
};
