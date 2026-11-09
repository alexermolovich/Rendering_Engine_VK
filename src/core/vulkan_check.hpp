#pragma once
#include <spdlog/spdlog.h>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vulkan/vulkan.h>

/* Keep Vulkan failures visible in both debug and release builds. */
inline void check_vulkan(VkResult result, const char *call)
{
    if (result == VK_SUCCESS)
        return;
    const auto message = std::string(call) + " failed with VkResult " + std::to_string(result);
    spdlog::error("{}", message);
    throw std::runtime_error(message);
}

/* Report invalid inputs without terminating the process. */
inline void require(bool condition, std::string_view message)
{
    if (condition)
        return;
    spdlog::error("{}", message);
    throw std::runtime_error(std::string(message));
}

/* Resource wrappers require explicit Vulkan destruction before C++ deletion. */
inline void require_destroyed(bool destroyed, std::string_view resource)
{
    if (destroyed)
        return;
    const auto message =
        std::string(resource) + " deleted before its Vulkan object was destroyed: resource safety violation";
    spdlog::critical("{}", message);
    throw std::logic_error(message);
}

#define VK_CHECK(call) check_vulkan((call), #call)
