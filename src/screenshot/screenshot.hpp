#pragma once

#include <reshade.hpp>

#define COLOR_CHANNELS 4
#define MAX_IMAGE_SIZE (7680 * 4320 * COLOR_CHANNELS)

namespace screenshot
{
    #pragma pack(push, 1)
    struct ScreenshotMemory
    {
        uint32_t width;
        uint32_t height;
        uint32_t channels;
        uint8_t imageBytes[1];  // [width * height * channels] bytes
    };
    #pragma pack(pop)

    void on_reshade_finish_effects(reshade::api::effect_runtime* runtime, reshade::api::command_list*, reshade::api::resource_view, reshade::api::resource_view);
    void on_init_effect_runtime(const reshade::api::effect_runtime* runtime);

    void on_destroy_effect_runtime(reshade::api::effect_runtime* runtime);

    void start();
    void stop();
}  // namespace screenshot