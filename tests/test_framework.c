#include "af/animation/animation.h"
#include "af/core/allocator.h"
#include "af/ui/actions.h"
#include "af/ui/draw.h"
#include "af/ui/input.h"

#include <math.h>
#include <stdio.h>

#define CHECK(condition)                                                                           \
    do {                                                                                           \
        if (!(condition)) {                                                                        \
            fprintf(stderr, "framework check failed at line %d: %s\n", __LINE__, #condition);      \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)

int main(void) {
    {
        AfSceneNode node = {.id = 1,
                            .parent_id = AF_INVALID_NODE_ID,
                            .type = AF_NODE_RECTANGLE,
                            .local_transform = {.scale = {1, 1}},
                            .bounds = {100, 100},
                            .opacity = 1,
                            .visible = true};
        AfStyle style = af_style_default(1, 1, (AfColor){1, 0, 0, 1});
        style.fill.type = AF_PAINT_LINEAR_GRADIENT;
        style.fill.start = (AfVec2){0, 0};
        style.fill.end = (AfVec2){100, 0};
        style.fill.stop_count = 2;
        style.fill.stops[0] = (AfGradientStop){0, {1, 0, 0, 1}};
        style.fill.stops[1] = (AfGradientStop){1, {0, 0, 1, 1}};
        style.stroke_paint = style.fill;
        AfStyleData styles = {.styles = &style, .count = 1, .capacity = 1};
        AfScene animated = {.nodes = &node, .count = 1, .capacity = 1, .styles = &styles};
        AfAnimationTrack tracks[] = {{.id = 1,
                                      .node_id = 1,
                                      .property = AF_PROPERTY_GRADIENT_STOP_COLOR,
                                      .keyframe_count = 2},
                                     {.id = 2,
                                      .node_id = 1,
                                      .property = AF_PROPERTY_GRADIENT_STOP_COLOR,
                                      .element_index = AF_ANIMATION_OUTLINE_ELEMENT + 1,
                                      .keyframe_start = 2,
                                      .keyframe_count = 2},
                                     {.id = 3,
                                      .node_id = 1,
                                      .property = AF_PROPERTY_GRADIENT_END,
                                      .element_index = AF_ANIMATION_OUTLINE_ELEMENT,
                                      .keyframe_start = 4,
                                      .keyframe_count = 2}};
        AfKeyframe keys[] = {
            {.time = 0, .value = {1, 0, 0, 1}, .interpolation = AF_INTERPOLATION_LINEAR},
            {.time = 2, .value = {0, 1, 0, 1}},
            {.time = 0, .value = {0, 0, 1, 1}, .interpolation = AF_INTERPOLATION_LINEAR},
            {.time = 2, .value = {1, 1, 1, 1}},
            {.time = 0, .value = {100, 0}, .interpolation = AF_INTERPOLATION_LINEAR},
            {.time = 2, .value = {200, 100}}};
        AfAnimationClip clip = {.id = 1, .duration = 2, .track_count = 3};
        AfAnimationData animation = {.clips = &clip,
                                     .clip_count = 1,
                                     .tracks = tracks,
                                     .track_count = 3,
                                     .keyframes = keys,
                                     .keyframe_count = 6};
        CHECK(af_animation_validate(&animation, &animated) == AF_RESULT_SUCCESS);
        CHECK(af_animation_apply(&animation, &clip, 1, &animated) == AF_RESULT_SUCCESS);
        CHECK(fabsf(style.fill.stops[0].color.r - .5F) < .001F &&
              fabsf(style.fill.stops[0].color.g - .5F) < .001F);
        CHECK(fabsf(style.stroke_paint.stops[1].color.r - .5F) < .001F);
        CHECK(style.stroke_paint.end.x == 150 && style.stroke_paint.end.y == 50 &&
              style.fill.end.x == 100);
        CHECK(af_style_data_validate(&styles) == AF_RESULT_SUCCESS);
        tracks[0].element_index = AF_MAX_GRADIENT_STOPS * 2U;
        CHECK(af_animation_validate(&animation, &animated) == AF_RESULT_INVALID_ARGUMENT);
    }
    AfSceneNode nodes[8] = {0};
    AfScene scene;
    AfPath paths[8] = {0};
    AfPathCommand commands[64] = {0};
    AfPathData path_data = {
        .paths = paths, .path_capacity = 8U, .commands = commands, .command_capacity = 64U};
    AfUiContext ui;
    af_scene_init(&scene, nodes, 8U);
    CHECK(af_ui_init(&ui, &scene, &path_data) == AF_RESULT_SUCCESS);
    af_ui_begin_frame(&ui, 1.0F / 60.0F, (AfVec2){30.0F, 30.0F}, false);
    ui.reduced_motion = true;
    af_ui_add_button_rect(&ui, 10.0F, 10.0F, 80.0F, 30.0F, (AfColor){0.1F, 0.2F, 0.3F, 1.0F}, 10);
    CHECK(scene.count == 2U && !ui.overflowed);
    CHECK(nodes[0].id != nodes[1].id && nodes[1].world_visible);
    CHECK(nodes[1].fill.r > 0.1F);
    ui.interaction_blocked_below_layer = 20;
    af_ui_add_button_rect(&ui, 10.0F, 10.0F, 80.0F, 30.0F, (AfColor){0.1F, 0.2F, 0.3F, 1.0F}, 10);
    CHECK(fabsf(nodes[3].fill.r - 0.1F) < 0.0001F);
    af_ui_add_line(&ui, (AfVec2){100.0F, 10.0F}, (AfVec2){160.0F, 10.0F}, 2.0F,
                   (AfColor){1.0F, 1.0F, 1.0F, 1.0F}, 30);
    CHECK(af_scene_validate(&scene) == AF_RESULT_SUCCESS);
    CHECK(af_scene_evaluate_world(&scene) == AF_RESULT_SUCCESS);
    CHECK(nodes[4].resolved_bounds.x == 60.0F);
    while (scene.count < scene.capacity)
        af_ui_add_rect(&ui, 0.0F, 0.0F, 1.0F, 1.0F, (AfColor){1.0F, 1.0F, 1.0F, 1.0F}, 0);
    af_ui_add_rect(&ui, 0.0F, 0.0F, 1.0F, 1.0F, (AfColor){1.0F, 1.0F, 1.0F, 1.0F}, 0);
    CHECK(ui.overflowed && scene.count == 8U);
    af_ui_begin_frame(&ui, NAN, (AfVec2){0}, false);
    CHECK(ui.delta_seconds == 0.0F && !ui.overflowed);
    AfColor color;
    float number;
    CHECK(af_ui_parse_hex_color("#12345678", &color));
    CHECK(fabsf(color.a - 120.0F / 255.0F) < 0.0001F);
    CHECK(af_ui_parse_number("12.5", &number) && number == 12.5F);
    CHECK(!af_ui_parse_number("nan", &number));
    AfUiAction actions[2];
    AfUiActionRegistry registry;
    CHECK(af_ui_action_registry_init(&registry, actions, 2U) == AF_RESULT_SUCCESS);
    const uint32_t custom_context = UINT32_C(1) << 20U;
    const AfUiActionDefinition definition = {.id = 7U,
                                             .label = "Custom host action",
                                             .tooltip = "Application-owned action",
                                             .keywords = "host custom",
                                             .contexts = custom_context};
    CHECK(af_ui_action_register(&registry, &definition) == AF_RESULT_SUCCESS);
    AfUiCommandPalette palette;
    CHECK(af_ui_command_palette_init(&palette, &registry, 4U) == AF_RESULT_SUCCESS);
    CHECK(af_ui_command_palette_open(&palette, custom_context) == AF_RESULT_SUCCESS);
    CHECK(af_ui_command_palette_set_query(&palette, "custom") == AF_RESULT_SUCCESS);
    CHECK(palette.result_count == 1U && af_ui_command_palette_selected(&palette)->id == 7U);
    puts("Independent framework UI and engine checks passed");
    return 0;
}
