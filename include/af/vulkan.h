#ifndef AF_VK_RENDERER_H
#define AF_VK_RENDERER_H

#include "af/platform/platform_sdl.h"
#include "af/renderer/vk_particles.h"
#include "af/result.h"
#include "af/scene/scene.h"

#include <stdbool.h>
#include <vulkan/vulkan.h>

typedef struct AfVkRenderer AfVkRenderer;

typedef struct AfVkRendererCreateInfo {
    AfPlatformWindow *window;
    /* NULL uses the build's shaders, then shaders/ beside the executable. */
    const char *shader_directory;
} AfVkRendererCreateInfo;

/* Borrowed Vulkan handles; their lifetime belongs to the renderer. */
typedef struct AfVkDeviceContext {
    VkInstance instance;
    VkPhysicalDevice physical_device;
    VkDevice device;
    VkQueue graphics_queue;
    uint32_t graphics_queue_family;
    uint32_t frames_in_flight;
} AfVkDeviceContext;

typedef struct AfVkFrameMetrics {
    double upload_ms;
    /* Last completed frame slot; unavailable until its fence and timestamp queries complete. */
    double gpu_ms;
    uint32_t path_rebuilds;
    uint32_t draw_batches;
    bool gpu_valid;
} AfVkFrameMetrics;

typedef struct AfVkRenderOptions {
    const AfScene *particle_scene;
    AfAffine2 particle_transform;
    int32_t particle_layer_order;
    uint32_t particle_clip_x;
    uint32_t particle_clip_y;
    uint32_t particle_clip_width;
    uint32_t particle_clip_height;
    float image_time;
    bool particle_clip_enabled;
    bool image_time_override;
} AfVkRenderOptions;

AfResult af_vk_renderer_create(AfPlatformWindow *window, AfVkRenderer **out_renderer);
AfResult af_vk_renderer_create_with_info(const AfVkRendererCreateInfo *info,
                                         AfVkRenderer **out_renderer);
void af_vk_renderer_destroy(AfVkRenderer *renderer);
AfVkDeviceContext af_vk_renderer_device_context(const AfVkRenderer *renderer);
AfResult af_vk_renderer_draw(AfVkRenderer *renderer, const AfScene *scene);
AfResult af_vk_renderer_draw_timed(AfVkRenderer *renderer, const AfScene *scene, float delta_time);
AfResult af_vk_renderer_draw_timed_with_options(AfVkRenderer *renderer, const AfScene *scene,
                                                float delta_time, const AfVkRenderOptions *options);
AfResult af_vk_renderer_capture_next(AfVkRenderer *renderer, const char *path);
void af_vk_renderer_request_resize(AfVkRenderer *renderer);
bool af_vk_renderer_has_validation_errors(const AfVkRenderer *renderer);
AfVkVfxMetrics af_vk_renderer_vfx_metrics(const AfVkRenderer *renderer);
AfVkFrameMetrics af_vk_renderer_frame_metrics(const AfVkRenderer *renderer);

#endif
