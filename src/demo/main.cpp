#include "core/renderer.hpp"
#include <charconv>
#include <string_view>

/* Keep application policy outside Vulkan resource and pass management. */
int main(int argc, char **argv)
{
    spdlog::set_pattern("[%H:%M:%S] [%^%l%$] %v");
    uint64_t frame_limit = 0;
    try
    {
        if (argc == 3 && std::string_view(argv[1]) == "--frames")
        {
            const std::string_view value(argv[2]);
            const auto [end, error] = std::from_chars(value.data(), value.data() + value.size(), frame_limit);
            require(error == std::errc{} && end == value.data() + value.size() && frame_limit > 0,
                    "--frames requires a positive integer");
        }
        else
            require(argc == 1, "Usage: vkml [--frames count]");
        Renderer renderer;
        renderer.init();
        renderer.run(frame_limit);
        renderer.shutdown();
        return 0;
    }
    catch (const std::exception &error)
    {
        spdlog::critical("Renderer failed: {}", error.what());
        return 1;
    }
}
