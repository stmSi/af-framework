#ifndef AF_CANVAS_RENDERER_H
#define AF_CANVAS_RENDERER_H

#include "af/platform/platform_sdl.h"
#include "af/scene/scene.h"

typedef struct AfCanvasRenderer AfCanvasRenderer;

typedef struct AfCanvasFrameMetrics {
    double upload_ms;
    double gpu_ms;
    uint32_t path_rebuilds;
    uint32_t draw_batches;
    bool gpu_valid;
} AfCanvasFrameMetrics;

typedef struct AfCanvasVfxMetrics {
    float cpu_milliseconds;
    float gpu_milliseconds;
    float lod_factor;
    uint32_t simulated_steps;
    uint32_t effective_capacity;
    bool gpu_timing_supported;
} AfCanvasVfxMetrics;

typedef struct AfCanvasRenderOptions {
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
} AfCanvasRenderOptions;

AfResult af_canvas_renderer_create(AfPlatformWindow *window, AfCanvasRenderer **out_renderer);
void af_canvas_renderer_destroy(AfCanvasRenderer *renderer);
AfResult af_canvas_renderer_draw(AfCanvasRenderer *renderer, const AfScene *scene);
AfResult af_canvas_renderer_draw_timed(AfCanvasRenderer *renderer, const AfScene *scene,
                                       float delta_time);
AfResult af_canvas_renderer_draw_timed_with_options(AfCanvasRenderer *renderer,
                                                    const AfScene *scene, float delta_time,
                                                    const AfCanvasRenderOptions *options);
AfResult af_canvas_renderer_capture_next(AfCanvasRenderer *renderer, const char *path);
void af_canvas_renderer_request_resize(AfCanvasRenderer *renderer);
bool af_canvas_renderer_has_errors(const AfCanvasRenderer *renderer);
AfCanvasFrameMetrics af_canvas_renderer_frame_metrics(const AfCanvasRenderer *renderer);
AfCanvasVfxMetrics af_canvas_renderer_vfx_metrics(const AfCanvasRenderer *renderer);

#endif
