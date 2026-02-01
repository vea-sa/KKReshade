#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "screenshot.hpp"

#define SCREENSHOT_SHM_NAME "KKReshade_Screenshot_SHM"
#define SCREENSHOT_CONTROL_SHM_NAME "KKReshade_Screenshot_Control_SHM"

namespace
{
    static HANDLE hMapFile = nullptr;
    static HANDLE hControlMapFile = nullptr;
    static screenshot::ScreenshotMemory* shm = nullptr;
    static reshade::api::resource staging_resource = { 0 };
    static bool* control_shm = nullptr;
    static bool last_control_state = false;
    static int screenshot_count = 0;
}

namespace screenshot
{
    void on_reshade_finish_effects(reshade::api::effect_runtime* runtime, reshade::api::command_list* cmd_list, reshade::api::resource_view, reshade::api::resource_view)
    {
        bool current_control_state = control_shm ? *control_shm : true;

        if (current_control_state != last_control_state)
        {
            if (current_control_state)
            {
                reshade::log::message(reshade::log::level::info, "Capture started via SHM trigger.");
                screenshot_count = 0;
            }
            else
            {
                char buffer[128];
                wsprintfA(buffer, "Capture stopped via SHM trigger. Total frames captured: %d", screenshot_count);
                reshade::log::message(reshade::log::level::info, buffer);
            }
            last_control_state = current_control_state;
        }

        if (!current_control_state)
        {
            return;
        }

        reshade::api::device* device = runtime->get_device();
        reshade::api::resource backbuffer = runtime->get_current_back_buffer();

        if (staging_resource.handle != 0)
        {
            reshade::api::resource_desc staging_desc = device->get_resource_desc(staging_resource);
            reshade::api::resource_desc backbuffer_desc = device->get_resource_desc(backbuffer);

            if (staging_desc.texture.width != backbuffer_desc.texture.width ||
                staging_desc.texture.height != backbuffer_desc.texture.height ||
                staging_desc.texture.format != backbuffer_desc.texture.format)
            {
                device->destroy_resource(staging_resource);
                staging_resource = { 0 };
            }
        }

        if (staging_resource.handle == 0)
        {
            reshade::api::resource_desc desc = device->get_resource_desc(backbuffer);
            desc.type = reshade::api::resource_type::texture_2d;
            desc.heap = reshade::api::memory_heap::gpu_to_cpu;
            desc.usage = reshade::api::resource_usage::copy_dest;

            if (!device->create_resource(desc, nullptr, reshade::api::resource_usage::copy_dest, &staging_resource))
            {
                reshade::log::message(reshade::log::level::error, "Failed to create staging resource!");
                return;
            }
        }

        reshade::api::subresource_data mapped_data;
        if (device->map_texture_region(staging_resource, 0, nullptr, reshade::api::map_access::read_only, &mapped_data))
        {
            if (mapped_data.row_pitch == shm->width * 4)
            {
                memcpy(shm->imageBytes, mapped_data.data, shm->width * shm->height * 4);  // NOLINT(bugprone-implicit-widening-of-multiplication-result)
            }
            else
            {
                for (uint32_t y = 0; y < shm->height; ++y)
                {
                    memcpy(shm->imageBytes + y * shm->width * 4, static_cast<uint8_t*>(mapped_data.data) + y * mapped_data.row_pitch, shm->width * 4);  // NOLINT(bugprone-implicit-widening-of-multiplication-result)
                }
            }
            device->unmap_texture_region(staging_resource, 0);
        }

        cmd_list->barrier(backbuffer, reshade::api::resource_usage::render_target, reshade::api::resource_usage::copy_source);
        cmd_list->copy_resource(backbuffer, staging_resource);  // NOLINT(readability-suspicious-call-argument)
        cmd_list->barrier(backbuffer, reshade::api::resource_usage::copy_source, reshade::api::resource_usage::render_target);

        screenshot_count++;
    }

    void on_init_effect_runtime(const reshade::api::effect_runtime* runtime)
    {
        runtime->get_screenshot_width_and_height(&shm->width, &shm->height);
    }

    void on_destroy_effect_runtime(reshade::api::effect_runtime* runtime)
    {
        shm->width = 0;
        shm->height = 0;
    }

    void start()
    {
        reshade::log::message(reshade::log::level::info, "Addon initializing...");

        hMapFile = CreateFileMappingA(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0, sizeof(ScreenshotMemory) + MAX_IMAGE_SIZE, SCREENSHOT_SHM_NAME);
        if (!hMapFile)
        {
            char buf[128];
            wsprintfA(buf, "Failed to create Image SHM! Error: %lu", GetLastError());
            reshade::log::message(reshade::log::level::error, buf);
            return;
        }

        shm = static_cast<ScreenshotMemory*>(MapViewOfFile(hMapFile, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(ScreenshotMemory) + MAX_IMAGE_SIZE));
        if (!shm)
        {
            CloseHandle(hMapFile);
            hMapFile = nullptr;
            reshade::log::message(reshade::log::level::error, "Failed to map Image SHM!");
            return;
        }

        shm->channels = COLOR_CHANNELS;

        hControlMapFile = CreateFileMappingA(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0, sizeof(bool), SCREENSHOT_CONTROL_SHM_NAME);
        if (hControlMapFile)
        {
            control_shm = static_cast<bool*>(MapViewOfFile(hControlMapFile, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(bool)));
            if (control_shm)
            {
                *control_shm = true;
            }
            reshade::log::message(reshade::log::level::info, "Control SHM created/opened successfully.");
        }
        else
        {
            char buf[128];
            wsprintfA(buf, "Failed to create Control SHM! Error: %lu", GetLastError());
            reshade::log::message(reshade::log::level::error, buf);
        }
    }

    void stop()
    {
        if (shm) UnmapViewOfFile(shm);
        if (hMapFile) CloseHandle(hMapFile);

        if (control_shm) UnmapViewOfFile(control_shm);
        if (hControlMapFile) CloseHandle(hControlMapFile);

        shm = nullptr;
        hMapFile = nullptr;
        control_shm = nullptr;
        hControlMapFile = nullptr;
    }
} // namespace screenshot