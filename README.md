# KKReshade

A Reshade addon for facilitating interaction between Reshade and KK/KKS/HS2/AI.

> [!NOTE]
> This addon currently only activates when running in Studio mode (`CharaStudio.exe` or `StudioNEOV2.exe`).

## Installation

Build the solution and copy `KKReshade.addon` to Reshade's addon directory (usually next to the game executable or `dxgi.dll`).

## Features

### Real-time Screenshot Access

This addon captures the final rendered frame (after all Reshade effects are applied) and makes it available via Shared Memory. This allows external applications (like a Unity plugin) to read the frame data.

*   **Shared Memory Name**: `KKReshade_Screenshot_SHM`
*   **Data Structure**:
    ```cpp
    #pragma pack(push, 1)
    struct ScreenshotMemory
    {
        uint32_t width;
        uint32_t height;
        uint32_t channels; // Always 4 (RGBA)
        uint8_t imageBytes[]; // Pixel data (Row-major)
    };
    #pragma pack(pop)
    ```

### Capture Control

To minimize performance impact, the capture process can be toggled on/off dynamically.

*   **Control Shared Memory**: `KKReshade_Screenshot_Control_SHM`
*   **Usage**: Write a single boolean byte (`1` or `0`) to this shared memory segment.
    *   `true` (1): Enable capture (updates `KKReshade_Screenshot_SHM` every frame).
    *   `false` (0): Disable capture (stops updating shared memory).

> [!NOTE]
> This feature is only supported by newer versions of the `VideoExport` plugin. It is fully backwards compatible: if no control signal is present, capture defaults to **enabled**.

### Technical Details

*   **Format Handling**: Handles backbuffer formats `R8G8B8A8_UNORM` and `R8G8B8A8_UNORM_SRGB` used by KK/KKS/HS2/AI.
*   **Resolution Support**: Supports up to 8K resolution (7680x4320).

## Building

Open `KKReshade.sln` in Visual Studio 2022 and build the `Release` configuration.
