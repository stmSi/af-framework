#include "af/canvas.h"

#include <math.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

_Static_assert(sizeof(AfPathCommand) == 7U * sizeof(float), "Canvas path ABI");
_Static_assert(offsetof(AfPathCommand, point) == sizeof(float), "Canvas path point ABI");
_Static_assert(sizeof(AfAffine2) == 6U * sizeof(float), "Canvas transform ABI");

extern int af_canvas_js_init(void);
extern void af_canvas_js_begin(void);
extern void af_canvas_js_destroy(void);
extern void af_canvas_js_shape(int kind, const AfAffine2 *matrix, float width, float height,
                               float radius, const AfPathCommand *commands, uint32_t count,
                               int evenodd, const float *fill, const float *stroke, float opacity,
                               float fill_opacity, float stroke_opacity, const float *geometry);
extern void af_canvas_js_image(const uint8_t *pixels, uint32_t source_width, uint32_t source_height,
                               const AfAffine2 *matrix, float width, float height, float opacity,
                               const AfImageBorders *borders, uint32_t generation,
                               uint32_t hash_low, uint32_t hash_high, int linear);
extern void af_canvas_js_capture(const char *path);
extern void af_canvas_js_particles(const AfAffine2 *matrix, const float *vertices, uint32_t count,
                                   const float *clip, int clipping);

typedef struct AfCanvasParticle {
    AfParticleSpawnSample spawn;
    AfVec2 position;
    AfVec2 velocity;
    float age;
} AfCanvasParticle;

typedef struct AfCanvasEmitter {
    uint64_t id;
    uint32_t capacity;
    uint32_t offset;
    uint32_t serial;
    uint32_t cursor;
    uint32_t event_sequence;
    float accumulated;
    bool started;
} AfCanvasEmitter;

typedef struct AfCanvasItem {
    const AfSceneNode *node;
    uint32_t index;
} AfCanvasItem;

struct AfCanvasRenderer {
    AfPlatformWindow *window;
    AfCanvasItem items[AF_MAX_NODES];
    AfCanvasFrameMetrics metrics;
    float time;
    float particle_accumulator;
    AfCanvasVfxMetrics vfx;
    AfCanvasEmitter emitters[AF_MAX_PARTICLE_EMITTERS];
    uint32_t emitter_count;
    AfCanvasParticle *particles;
    float *particle_vertices;
    char capture[1024];
};

static int af_canvas_compare(const void *a, const void *b) {
    const AfCanvasItem *left = a;
    const AfCanvasItem *right = b;
    if (left->node->layer_order != right->node->layer_order)
        return left->node->layer_order < right->node->layer_order ? -1 : 1;
    return left->index < right->index ? -1 : left->index != right->index;
}

static void af_canvas_paint(const AfPaint *paint, float *packet) {
    packet[0] = (float)paint->type;
    memcpy(packet + 1, &paint->solid, sizeof(AfColor));
    memcpy(packet + 5, &paint->start, sizeof(AfVec2));
    memcpy(packet + 7, &paint->end, sizeof(AfVec2));
    packet[9] = paint->start_radius;
    packet[10] = paint->end_radius;
    packet[11] = paint->angle;
    memcpy(packet + 12, &paint->transform, sizeof(AfAffine2));
    packet[18] = (float)paint->stop_count;
    for (uint32_t i = 0; i < paint->stop_count; ++i) {
        packet[19 + i * 5] = paint->stops[i].offset;
        memcpy(packet + 20 + i * 5, &paint->stops[i].color, sizeof(AfColor));
    }
}

AfResult af_canvas_renderer_create(AfPlatformWindow *window, AfCanvasRenderer **out_renderer) {
    if (window == NULL || window->window == NULL || out_renderer == NULL)
        return AF_RESULT_INVALID_ARGUMENT;
    *out_renderer = NULL;
    AfCanvasRenderer *renderer = calloc(1, sizeof(*renderer));
    if (renderer == NULL)
        return AF_RESULT_OUT_OF_MEMORY;
    if (!af_canvas_js_init()) {
        free(renderer);
        return AF_RESULT_UNSUPPORTED_FEATURE;
    }
    renderer->window = window;
    *out_renderer = renderer;
    return AF_RESULT_SUCCESS;
}

void af_canvas_renderer_destroy(AfCanvasRenderer *renderer) {
    if (renderer != NULL) {
        af_canvas_js_destroy();
        free(renderer->particles);
        free(renderer->particle_vertices);
        free(renderer);
    }
}

static AfResult af_canvas_prepare_particles(AfCanvasRenderer *renderer, const AfScene *scene,
                                            float delta_time) {
    const AfParticleData *data = scene->particles;
    renderer->vfx = (AfCanvasVfxMetrics){0};
    if (data == NULL || data->count == 0) {
        renderer->emitter_count = 0;
        renderer->particle_accumulator = 0;
        return AF_RESULT_SUCCESS;
    }
    if (af_particle_data_validate(data, scene) != AF_RESULT_SUCCESS)
        return AF_RESULT_INVALID_ARGUMENT;
    if (renderer->particles == NULL) {
        renderer->particles = calloc(AF_MAX_PARTICLES, sizeof(*renderer->particles));
        renderer->particle_vertices = calloc(AF_MAX_PARTICLES * 8U, sizeof(float));
        if (renderer->particles == NULL || renderer->particle_vertices == NULL) {
            free(renderer->particles);
            free(renderer->particle_vertices);
            renderer->particles = NULL;
            renderer->particle_vertices = NULL;
            return AF_RESULT_OUT_OF_MEMORY;
        }
    }
    uint32_t offset = 0;
    bool reset = renderer->emitter_count != data->count;
    const uint32_t budget = data->lod.maximum_particles == 0 ? AF_MAX_PARTICLES
                            : data->lod.maximum_particles < AF_MAX_PARTICLES
                                ? data->lod.maximum_particles
                                : AF_MAX_PARTICLES;
    for (uint32_t i = 0; i < data->count; ++i) {
        const AfParticleEmitter *emitter = &data->emitters[i];
        const uint32_t capacity =
            emitter->capacity < budget - offset ? emitter->capacity : budget - offset;
        AfCanvasEmitter *state = &renderer->emitters[i];
        if (state->id != emitter->id || state->capacity != capacity || state->offset != offset)
            reset = true;
        state->id = emitter->id;
        state->capacity = capacity;
        state->offset = offset;
        offset += capacity;
    }
    if (reset) {
        memset(renderer->particles, 0, AF_MAX_PARTICLES * sizeof(*renderer->particles));
        for (uint32_t i = 0; i < data->count; ++i) {
            renderer->emitters[i].started = false;
            renderer->emitters[i].serial = 0;
            renderer->emitters[i].cursor = 0;
            renderer->emitters[i].accumulated = 0;
        }
        renderer->emitter_count = data->count;
        renderer->particle_accumulator = 0;
    }
    renderer->vfx.effective_capacity = offset;
    renderer->vfx.lod_factor = af_vfx_lod_factor(&data->lod);
    const float step =
        AF_PARTICLE_FIXED_STEP /
        (data->lod.maximum_particles == 0 ? 1 : data->lod.simulation_frequency_scale);
    const float spawn_scale = data->lod.maximum_particles == 0 ? 1 : data->lod.spawn_rate_scale;
    renderer->particle_accumulator += fminf(delta_time, 0.1F);
    while (renderer->particle_accumulator >= step &&
           renderer->vfx.simulated_steps < AF_PARTICLE_MAX_STEPS_PER_FRAME) {
        const float dt = step;
        renderer->particle_accumulator -= dt;
        ++renderer->vfx.simulated_steps;
        for (uint32_t i = 0; i < data->count; ++i) {
            const AfParticleEmitter *emitter = &data->emitters[i];
            AfCanvasEmitter *state = &renderer->emitters[i];
            for (uint32_t p = 0; p < state->capacity; ++p) {
                AfCanvasParticle *particle = &renderer->particles[state->offset + p];
                if (particle->spawn.lifetime <= 0)
                    continue;
                particle->age += dt;
                if (particle->age >= particle->spawn.lifetime) {
                    particle->spawn.lifetime = 0;
                    continue;
                }
                const AfVec2 force =
                    af_particle_force_sample(emitter, particle->position, renderer->time);
                particle->velocity.x =
                    (particle->velocity.x + force.x * dt) * expf(-emitter->drag * dt);
                particle->velocity.y =
                    (particle->velocity.y + force.y * dt) * expf(-emitter->drag * dt);
                const AfVec2 previous = particle->position;
                particle->position.x += particle->velocity.x * dt;
                particle->position.y += particle->velocity.y * dt;
                float depth = 0, depth_velocity = 0;
                (void)af_particle_collide_sample(emitter, previous, &particle->position,
                                                 &particle->velocity, &depth, &depth_velocity);
            }
            if (!emitter->enabled || state->capacity == 0 || renderer->vfx.lod_factor <= 0)
                continue;
            state->accumulated += emitter->spawn_rate * renderer->vfx.lod_factor * spawn_scale * dt;
            uint32_t spawn = (uint32_t)fmaxf(floorf(state->accumulated), 0);
            state->accumulated -= (float)spawn;
            if (!state->started) {
                spawn += emitter->burst_count;
                state->started = true;
            }
            if (state->event_sequence != emitter->event_sequence)
                spawn += emitter->event_burst_count;
            state->event_sequence = emitter->event_sequence;
            if (spawn > state->capacity)
                spawn = state->capacity;
            for (uint32_t n = 0; n < spawn; ++n) {
                uint32_t checked = 0;
                while (checked < state->capacity &&
                       renderer->particles[state->offset + state->cursor].spawn.lifetime > 0) {
                    state->cursor = (state->cursor + 1) % state->capacity;
                    ++checked;
                }
                if (checked == state->capacity)
                    break;
                AfCanvasParticle *particle = &renderer->particles[state->offset + state->cursor];
                if (af_particle_spawn_sample(emitter, state->serial++, &particle->spawn) ==
                    AF_RESULT_SUCCESS) {
                    particle->position = particle->spawn.position;
                    particle->velocity = particle->spawn.velocity;
                    particle->age = 0;
                }
                state->cursor = (state->cursor + 1) % state->capacity;
            }
        }
    }
    return AF_RESULT_SUCCESS;
}

static void af_canvas_draw_particles(AfCanvasRenderer *renderer, const AfScene *scene,
                                     const AfCanvasRenderOptions *options) {
    const AfParticleData *data = scene->particles;
    if (data == NULL || renderer->particles == NULL || renderer->vfx.lod_factor <= 0)
        return;
    float clip[4] = {0};
    if (options != NULL) {
        clip[0] = (float)options->particle_clip_x;
        clip[1] = (float)options->particle_clip_y;
        clip[2] = (float)options->particle_clip_width;
        clip[3] = (float)options->particle_clip_height;
    }
    for (uint32_t i = 0; i < data->count; ++i) {
        const AfParticleEmitter *emitter = &data->emitters[i];
        const AfSceneNode *node = af_scene_find_const(scene, emitter->node_id);
        if (node == NULL || !node->world_visible ||
            emitter->output >= AF_PARTICLE_OUTPUT_HEAT_DISTORTION)
            continue;
        const AfCanvasEmitter *state = &renderer->emitters[i];
        uint32_t count = 0;
        for (uint32_t p = 0; p < state->capacity; ++p) {
            const AfCanvasParticle *particle = &renderer->particles[state->offset + p];
            if (particle->spawn.lifetime <= 0)
                continue;
            const float t = fminf(particle->age / particle->spawn.lifetime, 1);
            float *vertex = renderer->particle_vertices + count++ * 8U;
            vertex[0] = particle->position.x;
            vertex[1] = particle->position.y;
            vertex[2] = particle->spawn.size_start +
                        (particle->spawn.size_end - particle->spawn.size_start) * t;
            vertex[3] = particle->spawn.rotation + particle->spawn.angular_velocity * particle->age;
            vertex[4] =
                emitter->color_start.r + (emitter->color_end.r - emitter->color_start.r) * t;
            vertex[5] =
                emitter->color_start.g + (emitter->color_end.g - emitter->color_start.g) * t;
            vertex[6] =
                emitter->color_start.b + (emitter->color_end.b - emitter->color_start.b) * t;
            vertex[7] =
                (emitter->color_start.a + (emitter->color_end.a - emitter->color_start.a) * t) *
                node->world_opacity;
        }
        AfAffine2 transform =
            options != NULL ? af_affine_multiply(options->particle_transform, node->world_transform)
                            : node->world_transform;
        af_canvas_js_particles(&transform, renderer->particle_vertices, count, clip,
                               options != NULL && options->particle_clip_enabled);
        ++renderer->metrics.draw_batches;
    }
}

static void af_canvas_image(AfCanvasRenderer *renderer, const AfScene *scene,
                            const AfSceneNode *node, float time) {
    (void)renderer;
    const AfImageBinding *binding = af_image_find_binding_const(scene->images, node->id);
    if (binding == NULL)
        return;
    uint64_t id = binding->asset_id;
    const AfImageSequence *sequence =
        af_image_find_sequence_const(scene->images, binding->sequence_id);
    if (sequence != NULL && sequence->frame_count != 0) {
        const float rate = binding->frames_per_second > 0 ? binding->frames_per_second
                                                          : sequence->frames_per_second;
        uint32_t frame = (uint32_t)fmaxf(floorf(time * rate), 0);
        frame = binding->loop                   ? frame % sequence->frame_count
                : frame < sequence->frame_count ? frame
                                                : sequence->frame_count - 1;
        id = scene->images->sequence_frames[sequence->frame_start + frame];
    }
    const AfImageAsset *asset = af_image_find_const(scene->images, id);
    if (asset == NULL || asset->missing || asset->pixels == NULL)
        return;
    af_canvas_js_image(
        asset->pixels, asset->width, asset->height, &node->world_transform, node->resolved_bounds.x,
        node->resolved_bounds.y, node->world_opacity,
        binding->display_mode == AF_IMAGE_DISPLAY_NINE_SLICE ? &binding->borders : NULL,
        (uint32_t)scene->images->generation, (uint32_t)asset->content_hash,
        (uint32_t)(asset->content_hash >> 32U), asset->color_space == AF_IMAGE_COLOR_LINEAR);
}

AfResult af_canvas_renderer_draw_timed_with_options(AfCanvasRenderer *renderer,
                                                    const AfScene *scene, float delta_time,
                                                    const AfCanvasRenderOptions *options) {
    if (renderer == NULL || scene == NULL || scene->count > AF_MAX_NODES ||
        (scene->count != 0 && scene->nodes == NULL) || !isfinite(delta_time) || delta_time < 0)
        return AF_RESULT_INVALID_ARGUMENT;
    const uint64_t started = SDL_GetPerformanceCounter();
    if (af_scene_validate(scene) != AF_RESULT_SUCCESS)
        return AF_RESULT_INVALID_ARGUMENT;
    renderer->time += delta_time;
    renderer->metrics = (AfCanvasFrameMetrics){0};
    for (uint32_t i = 0; i < scene->count; ++i)
        renderer->items[i] = (AfCanvasItem){&scene->nodes[i], i};
    qsort(renderer->items, scene->count, sizeof(renderer->items[0]), af_canvas_compare);
    const AfScene *particle_scene =
        options != NULL && options->particle_scene != NULL ? options->particle_scene : scene;
    AfResult result = af_canvas_prepare_particles(renderer, particle_scene, delta_time);
    if (result != AF_RESULT_SUCCESS)
        return result;
    renderer->vfx.cpu_milliseconds = (float)((double)(SDL_GetPerformanceCounter() - started) *
                                             1000.0 / (double)SDL_GetPerformanceFrequency());
    af_canvas_js_begin();
    bool particles_drawn = false;
    for (uint32_t i = 0; i < scene->count; ++i) {
        const AfSceneNode *node = renderer->items[i].node;
        if (!particles_drawn && options != NULL &&
            node->layer_order >= options->particle_layer_order) {
            af_canvas_draw_particles(renderer, particle_scene, options);
            particles_drawn = true;
        }
        if (!node->world_visible || node->world_opacity <= 0 || node->type == AF_NODE_GROUP)
            continue;
        if (node->type == AF_NODE_IMAGE) {
            af_canvas_image(renderer, scene, node,
                            options != NULL && options->image_time_override ? options->image_time
                                                                            : renderer->time);
            ++renderer->metrics.draw_batches;
            continue;
        }
        const AfPath *path = af_path_find_node_const(scene->paths, node->id);
        if (node->type == AF_NODE_PATH && path == NULL)
            continue;
        AfStyle style = af_style_default(node->id, node->id, node->fill);
        const AfStyle *source = af_style_find_node_const(scene->styles, node->id);
        if (source != NULL && af_style_resolve(scene->styles, source, &style) != AF_RESULT_SUCCESS)
            return AF_RESULT_INVALID_ARGUMENT;
        float fill[99] = {0}, stroke[99] = {0}, geometry[22] = {0};
        af_canvas_paint(&style.fill, fill);
        af_canvas_paint(&style.stroke_paint, stroke);
        geometry[0] = style.stroke.enabled ? style.stroke.width : 0;
        geometry[1] = (float)style.stroke.cap;
        geometry[2] = (float)style.stroke.join;
        geometry[3] = style.stroke.miter_limit;
        geometry[4] = style.stroke.dash_offset;
        geometry[5] = (float)style.stroke.dash_count;
        memcpy(geometry + 6, style.stroke.dashes, style.stroke.dash_count * sizeof(float));
        af_canvas_js_shape((int)node->type, &node->world_transform, node->resolved_bounds.x,
                           node->resolved_bounds.y, node->corner_radius,
                           path != NULL ? scene->paths->commands + path->command_start : NULL,
                           path != NULL ? path->command_count : 0,
                           path != NULL && path->fill_rule == AF_PATH_FILL_EVEN_ODD, fill, stroke,
                           node->world_opacity, style.fill_opacity, style.stroke_opacity, geometry);
        ++renderer->metrics.draw_batches;
    }
    if (!particles_drawn)
        af_canvas_draw_particles(renderer, particle_scene, options);
    if (renderer->capture[0] != '\0') {
        af_canvas_js_capture(renderer->capture);
        renderer->capture[0] = '\0';
    }
    renderer->metrics.upload_ms = (double)(SDL_GetPerformanceCounter() - started) * 1000.0 /
                                  (double)SDL_GetPerformanceFrequency();
    return AF_RESULT_SUCCESS;
}

AfResult af_canvas_renderer_draw_timed(AfCanvasRenderer *renderer, const AfScene *scene,
                                       float delta_time) {
    return af_canvas_renderer_draw_timed_with_options(renderer, scene, delta_time, NULL);
}

AfResult af_canvas_renderer_draw(AfCanvasRenderer *renderer, const AfScene *scene) {
    return af_canvas_renderer_draw_timed(renderer, scene, 0);
}

AfResult af_canvas_renderer_capture_next(AfCanvasRenderer *renderer, const char *path) {
    if (renderer == NULL || path == NULL || strlen(path) >= sizeof(renderer->capture))
        return AF_RESULT_INVALID_ARGUMENT;
    (void)strcpy(renderer->capture, path);
    return AF_RESULT_SUCCESS;
}

void af_canvas_renderer_request_resize(AfCanvasRenderer *renderer) { (void)renderer; }
bool af_canvas_renderer_has_errors(const AfCanvasRenderer *renderer) {
    (void)renderer;
    return false;
}
AfCanvasFrameMetrics af_canvas_renderer_frame_metrics(const AfCanvasRenderer *renderer) {
    return renderer != NULL ? renderer->metrics : (AfCanvasFrameMetrics){0};
}
AfCanvasVfxMetrics af_canvas_renderer_vfx_metrics(const AfCanvasRenderer *renderer) {
    return renderer != NULL ? renderer->vfx : (AfCanvasVfxMetrics){0};
}
