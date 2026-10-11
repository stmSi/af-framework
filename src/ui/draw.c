#include "af/ui/draw.h"
#include "af/core/hash.h"

#include <limits.h>
#include <math.h>
#include <stddef.h>
#include <string.h>

#define AF_UI_TEXT_GLYPHS_PER_PATH 8U

static float af_ui_clamp(float value, float minimum, float maximum) {
    return fminf(fmaxf(value, minimum), maximum);
}

AfResult af_ui_init(AfUiContext *ui, AfScene *scene, AfPathData *paths) {
    if (ui == NULL || scene == NULL || paths == NULL || scene->nodes == NULL ||
        scene->count > scene->capacity || paths->path_count > paths->path_capacity ||
        paths->command_count > paths->command_capacity) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    *ui = (AfUiContext){.scene = scene,
                        .paths = paths,
                        .node_id_base = UINT64_C(0xF000000000000000),
                        .font_id = 1U,
                        .contrast_border = {0.62F, 0.66F, 0.75F, 1.0F},
                        .interaction_blocked_below_layer = INT32_MIN};
    return AF_RESULT_SUCCESS;
}

void af_ui_begin_frame(AfUiContext *ui, float delta_seconds, AfVec2 pointer, bool pointer_down) {
    if (ui == NULL)
        return;
    ++ui->frame;
    ui->delta_seconds = isfinite(delta_seconds) ? af_ui_clamp(delta_seconds, 0.0F, 0.05F) : 0.0F;
    ui->pointer = pointer;
    ui->pointer_down = pointer_down;
    ui->overflowed = false;
}

void af_ui_add_shape(AfUiContext *ui, AfNodeType type, AfAffine2 transform, AfVec2 bounds,
                     AfColor color, float radius, int32_t layer) {
    AfSceneNode *node = NULL;
    if (ui == NULL || ui->scene == NULL || ui->scene->nodes == NULL)
        return;
    if (ui->scene->count >= ui->scene->capacity) {
        ui->overflowed = true;
        return;
    }
    node = &ui->scene->nodes[ui->scene->count++];
    *node = (AfSceneNode){.id = ui->node_id_base + ui->scene->count,
                          .parent_id = AF_INVALID_NODE_ID,
                          .type = type,
                          .bounds = bounds,
                          .resolved_bounds = bounds,
                          .fill = color,
                          .opacity = 1.0F,
                          .corner_radius = radius,
                          .layer_order = layer,
                          .visible = true,
                          .world_transform = transform,
                          .world_opacity = 1.0F,
                          .world_visible = true};
}

void af_ui_add_rect(AfUiContext *ui, float x, float y, float width, float height, AfColor color,
                    int32_t layer) {
    af_ui_add_shape(ui, AF_NODE_RECTANGLE, (AfAffine2){.m00 = 1.0F, .m11 = 1.0F, .tx = x, .ty = y},
                    (AfVec2){width, height}, color, 0.0F, layer);
}

bool af_ui_hovered_rect(const AfUiContext *ui, float x, float y, float width, float height) {
    return ui != NULL && af_ui_point_in_rect(ui->pointer, x, y, width, height);
}

bool af_ui_point_in_rect(AfVec2 point, float x, float y, float width, float height) {
    return point.x >= x && point.x <= x + width && point.y >= y && point.y <= y + height;
}

void af_ui_add_surface(AfUiContext *ui, float x, float y, float width, float height, AfColor color,
                       float radius, int32_t layer) {
    if (width <= 0.0F || height <= 0.0F)
        return;
    af_ui_add_shape(ui, AF_NODE_ROUNDED_RECTANGLE,
                    (AfAffine2){.m00 = 1, .m11 = 1, .tx = x, .ty = y}, (AfVec2){width, height},
                    color, radius, layer);
}

static float af_ui_button_feedback(AfUiContext *ui, float x, float y, float width, float height,
                                   int32_t layer) {
    const float bounds[] = {x, y, width, height, (float)layer};
    const uint64_t key = af_hash_bytes(bounds, sizeof(bounds));
    uint32_t slot = (uint32_t)(key % 256U);
    for (uint32_t probe = 0; probe < 8; ++probe) {
        const AfUiHoverFeedback *entry = &ui->hover_feedback[slot];
        if (entry->key == key || entry->key == 0 || ui->frame - entry->frame > 1U)
            break;
        slot = (slot + 1U) % 256U;
    }
    AfUiHoverFeedback *feedback = &ui->hover_feedback[slot];
    const bool covered = layer < ui->interaction_blocked_below_layer;
    const float target = !covered && af_ui_hovered_rect(ui, x, y, width, height) ? 1 : 0;
    if (feedback->key != key || ui->frame - feedback->frame > 1U) {
        *feedback = (AfUiHoverFeedback){.key = key, .amount = 0};
    }
    const float blend = ui->reduced_motion ? 1.0F : 1.0F - expf(-22.0F * ui->delta_seconds);
    feedback->amount += (target - feedback->amount) * blend;
    feedback->frame = ui->frame;
    return feedback->amount;
}

void af_ui_add_button_rounded(AfUiContext *ui, float x, float y, float width, float height,
                              AfColor color, float radius, int32_t layer) {
    const float feedback = af_ui_button_feedback(ui, x, y, width, height, layer);
    const bool pressed =
        feedback > 0.1F && af_ui_hovered_rect(ui, x, y, width, height) && ui->pointer_down;
    const float lift = feedback * (pressed ? -0.025F : 0.055F);
    color.r = af_ui_clamp(color.r + lift, 0, 1);
    color.g = af_ui_clamp(color.g + lift, 0, 1);
    color.b = af_ui_clamp(color.b + lift, 0, 1);
    AfColor border = {color.r + 0.045F + feedback * 0.07F, color.g + 0.045F + feedback * 0.055F,
                      color.b + 0.06F + feedback * 0.10F, color.a};
    border.r = af_ui_clamp(border.r, 0, 1);
    border.g = af_ui_clamp(border.g, 0, 1);
    border.b = af_ui_clamp(border.b, 0, 1);
    if (ui->high_contrast)
        border = ui->contrast_border;
    af_ui_add_surface(ui, x, y, width, height, border, radius, layer);
    af_ui_add_surface(ui, x + 1, y + 1, width - 2, height - 2, color, fmaxf(0, radius - 1),
                      layer + 1);
}

void af_ui_add_button_rect(AfUiContext *ui, float x, float y, float width, float height,
                           AfColor color, int32_t layer) {
    af_ui_add_button_rounded(ui, x, y, width, height, color, fminf(6.0F, height * 0.25F), layer);
}

void af_ui_add_line(AfUiContext *ui, AfVec2 start, AfVec2 end, float thickness, AfColor color,
                    int32_t layer) {
    const float x = end.x - start.x;
    const float y = end.y - start.y;
    const float length = sqrtf(x * x + y * y);
    const float cosine = length > 0.0F ? x / length : 1.0F;
    const float sine = length > 0.0F ? y / length : 0.0F;
    af_ui_add_shape(ui, AF_NODE_RECTANGLE,
                    (AfAffine2){.m00 = cosine,
                                .m01 = -sine,
                                .m10 = sine,
                                .m11 = cosine,
                                .tx = start.x + sine * thickness * 0.5F,
                                .ty = start.y - cosine * thickness * 0.5F},
                    (AfVec2){length, thickness}, color, 0.0F, layer);
}

static const uint8_t *af_ui_glyph(char character) {
    static const uint8_t letters[26][5] = {
        {0x7EU, 0x11U, 0x11U, 0x11U, 0x7EU}, {0x7FU, 0x49U, 0x49U, 0x49U, 0x36U},
        {0x3EU, 0x41U, 0x41U, 0x41U, 0x22U}, {0x7FU, 0x41U, 0x41U, 0x22U, 0x1CU},
        {0x7FU, 0x49U, 0x49U, 0x49U, 0x41U}, {0x7FU, 0x09U, 0x09U, 0x09U, 0x01U},
        {0x3EU, 0x41U, 0x49U, 0x49U, 0x7AU}, {0x7FU, 0x08U, 0x08U, 0x08U, 0x7FU},
        {0x00U, 0x41U, 0x7FU, 0x41U, 0x00U}, {0x20U, 0x40U, 0x41U, 0x3FU, 0x01U},
        {0x7FU, 0x08U, 0x14U, 0x22U, 0x41U}, {0x7FU, 0x40U, 0x40U, 0x40U, 0x40U},
        {0x7FU, 0x02U, 0x0CU, 0x02U, 0x7FU}, {0x7FU, 0x04U, 0x08U, 0x10U, 0x7FU},
        {0x3EU, 0x41U, 0x41U, 0x41U, 0x3EU}, {0x7FU, 0x09U, 0x09U, 0x09U, 0x06U},
        {0x3EU, 0x41U, 0x51U, 0x21U, 0x5EU}, {0x7FU, 0x09U, 0x19U, 0x29U, 0x46U},
        {0x46U, 0x49U, 0x49U, 0x49U, 0x31U}, {0x01U, 0x01U, 0x7FU, 0x01U, 0x01U},
        {0x3FU, 0x40U, 0x40U, 0x40U, 0x3FU}, {0x1FU, 0x20U, 0x40U, 0x20U, 0x1FU},
        {0x3FU, 0x40U, 0x38U, 0x40U, 0x3FU}, {0x63U, 0x14U, 0x08U, 0x14U, 0x63U},
        {0x07U, 0x08U, 0x70U, 0x08U, 0x07U}, {0x61U, 0x51U, 0x49U, 0x45U, 0x43U}};
    static const uint8_t digits[10][5] = {
        {0x3EU, 0x51U, 0x49U, 0x45U, 0x3EU}, {0x00U, 0x42U, 0x7FU, 0x40U, 0x00U},
        {0x42U, 0x61U, 0x51U, 0x49U, 0x46U}, {0x21U, 0x41U, 0x45U, 0x4BU, 0x31U},
        {0x18U, 0x14U, 0x12U, 0x7FU, 0x10U}, {0x27U, 0x45U, 0x45U, 0x45U, 0x39U},
        {0x3CU, 0x4AU, 0x49U, 0x49U, 0x30U}, {0x01U, 0x71U, 0x09U, 0x05U, 0x03U},
        {0x36U, 0x49U, 0x49U, 0x49U, 0x36U}, {0x06U, 0x49U, 0x49U, 0x29U, 0x1EU}};
    static const uint8_t unknown[5] = {0x02U, 0x01U, 0x51U, 0x09U, 0x06U};
    static const uint8_t dash[5] = {0x08U, 0x08U, 0x08U, 0x08U, 0x08U};
    static const uint8_t dot[5] = {0x00U, 0x60U, 0x60U, 0x00U, 0x00U};
    static const uint8_t colon[5] = {0x00U, 0x36U, 0x36U, 0x00U, 0x00U};
    static const uint8_t slash[5] = {0x20U, 0x10U, 0x08U, 0x04U, 0x02U};
    static const uint8_t plus[5] = {0x08U, 0x08U, 0x3EU, 0x08U, 0x08U};
    static const uint8_t percent[5] = {0x63U, 0x13U, 0x08U, 0x64U, 0x63U};
    static const uint8_t equals[5] = {0x14U, 0x14U, 0x14U, 0x14U, 0x14U};
    static const uint8_t underscore[5] = {0x40U, 0x40U, 0x40U, 0x40U, 0x40U};
    if (character >= 'a' && character <= 'z') {
        character = (char)(character - 'a' + 'A');
    }
    if (character >= 'A' && character <= 'Z') {
        return letters[(uint32_t)(character - 'A')];
    }
    if (character >= '0' && character <= '9') {
        return digits[(uint32_t)(character - '0')];
    }
    switch (character) {
    case ' ':
        return NULL;
    case '-':
        return dash;
    case '.':
    case ',':
        return dot;
    case ':':
        return colon;
    case '/':
        return slash;
    case '+':
        return plus;
    case '%':
        return percent;
    case '=':
        return equals;
    case '_':
        return underscore;
    default:
        return unknown;
    }
}

void af_ui_add_bitmap_text(AfUiContext *ui, float x, float y, const char *text, float scale,
                           float maximum_width, AfColor color, int32_t layer) {
    float cursor = x;
    if (text == NULL || scale <= 0.0F || maximum_width <= 0.0F) {
        return;
    }
    for (size_t index = 0U; text[index] != '\0' && cursor + 5.0F * scale <= x + maximum_width;
         ++index) {
        const uint8_t *columns = af_ui_glyph(text[index]);
        if (columns != NULL) {
            uint8_t rows[7] = {0};
            for (uint32_t row = 0U; row < 7U; ++row) {
                for (uint32_t column = 0U; column < 5U; ++column) {
                    if ((columns[column] & (1U << row)) != 0U) {
                        rows[row] |= (uint8_t)(1U << column);
                    }
                }
            }
            for (;;) {
                uint32_t best_area = 0U;
                uint32_t best_row = 0U;
                uint32_t best_column = 0U;
                uint32_t best_width = 0U;
                uint32_t best_height = 0U;
                for (uint32_t row = 0U; row < 7U; ++row) {
                    for (uint32_t first = 0U; first < 5U; ++first) {
                        uint8_t mask = 0U;
                        if ((rows[row] & (1U << first)) == 0U) {
                            continue;
                        }
                        for (uint32_t last = first; last < 5U; ++last) {
                            if ((rows[row] & (1U << last)) == 0U) {
                                break;
                            }
                            mask |= (uint8_t)(1U << last);
                            for (uint32_t bottom = row; bottom < 7U; ++bottom) {
                                const uint32_t area = (last - first + 1U) * (bottom - row + 1U);
                                if ((rows[bottom] & mask) != mask) {
                                    break;
                                }
                                if (area > best_area) {
                                    best_area = area;
                                    best_row = row;
                                    best_column = first;
                                    best_width = last - first + 1U;
                                    best_height = bottom - row + 1U;
                                }
                            }
                        }
                    }
                }
                if (best_area == 0U) {
                    break;
                }
                af_ui_add_rect(ui, cursor + (float)best_column * scale, y + (float)best_row * scale,
                               (float)best_width * scale, (float)best_height * scale, color, layer);
                const uint8_t clear_mask = (uint8_t)(((1U << best_width) - 1U) << best_column);
                for (uint32_t row = best_row; row < best_row + best_height; ++row) {
                    rows[row] &= (uint8_t)~clear_mask;
                }
            }
        }
        cursor += 6.0F * scale;
    }
}

void af_ui_finish_text_path(AfUiContext *ui, uint32_t command_start, AfVec2 origin, AfColor color,
                            int32_t layer) {
    const uint32_t command_count = ui->paths->command_count - command_start;
    AfSceneNode *node = NULL;
    AfPath *path = NULL;
    uint64_t generation = 0U;
    uint64_t path_id = 0U;
    if (command_count == 0U || command_count > AF_MAX_PATH_COMMANDS_PER_PATH ||
        ui->scene->count >= ui->scene->capacity ||
        ui->paths->path_count >= ui->paths->path_capacity) {
        ui->paths->command_count = command_start;
        return;
    }
    node = &ui->scene->nodes[ui->scene->count++];
    *node =
        (AfSceneNode){.id = ui->node_id_base + ui->scene->count,
                      .parent_id = AF_INVALID_NODE_ID,
                      .type = AF_NODE_PATH,
                      .bounds = {1.0F, 1.0F},
                      .resolved_bounds = {1.0F, 1.0F},
                      .fill = color,
                      .opacity = 1.0F,
                      .layer_order = layer,
                      .visible = true,
                      .world_transform = {.m00 = 1.0F, .m11 = 1.0F, .tx = origin.x, .ty = origin.y},
                      .world_opacity = 1.0F,
                      .world_visible = true};
    generation = af_hash_bytes(&ui->paths->commands[command_start],
                               (size_t)command_count * sizeof(AfPathCommand));
    /* Screen position and preceding UI nodes must not invalidate tessellated text.
     * Probe duplicate outlines so each instance still has a unique path record. */
    path_id = ui->node_id_base | (generation & UINT64_C(0x0FFFFFFFFFFFFFFF));
    while (af_path_find_const(ui->paths, path_id) != NULL) {
        path_id = ui->node_id_base | ((path_id + 1U) & UINT64_C(0x0FFFFFFFFFFFFFFF));
    }
    path = &ui->paths->paths[ui->paths->path_count++];
    *path = (AfPath){.id = path_id,
                     .node_id = node->id,
                     .command_start = command_start,
                     .command_count = command_count,
                     .fill_rule = AF_PATH_FILL_NON_ZERO,
                     .geometry_generation = generation == 0U ? 1U : generation};
}

void af_ui_add_text(AfUiContext *ui, float x, float y, const char *text, float scale,
                    float maximum_width, AfColor color, int32_t layer) {
    AfTextLayout layout = {.glyphs = ui->glyphs,
                           .glyph_capacity = ui->glyph_capacity,
                           .lines = ui->lines,
                           .line_capacity = ui->line_capacity};
    const size_t byte_count = text == NULL ? 0U : strlen(text);
    float font_size = ceilf(9.0F + scale * 4.0F);
    AfTextRequest request = {.utf8 = text,
                             .byte_count = (uint32_t)byte_count,
                             .font_ids = {ui->font_id},
                             .font_count = 1U,
                             .font_size = font_size,
                             .maximum_width = maximum_width,
                             .line_spacing = 1.0F,
                             .maximum_lines = 1U,
                             .direction = AF_TEXT_DIRECTION_LTR,
                             .alignment = AF_TEXT_ALIGN_START,
                             .wrap = AF_TEXT_WRAP_NONE,
                             .kerning = true,
                             .ligatures = true,
                             .truncate = true};
    uint32_t command_start = ui->paths->command_count;
    uint32_t batch_glyph_count = 0U;
    if (ui->text_engine == NULL || ui->glyphs == NULL || ui->lines == NULL || byte_count == 0U ||
        byte_count > UINT32_MAX || scale <= 0.0F || maximum_width <= 0.0F) {
        af_ui_add_bitmap_text(ui, x, y, text, scale, maximum_width, color, layer);
        return;
    }
    for (;;) {
        if (af_text_shape(ui->text_engine, &request, &layout) != AF_RESULT_SUCCESS) {
            af_ui_add_bitmap_text(ui, x, y, text, scale, maximum_width, color, layer);
            return;
        }
        if (layout.bounds.x <= maximum_width || font_size <= 10.0F) {
            break;
        }
        {
            float fitted_size = floorf(font_size * maximum_width / layout.bounds.x);
            if (fitted_size >= font_size) {
                fitted_size = font_size - 1.0F;
            }
            font_size = fmaxf(10.0F, fitted_size);
            request.font_size = font_size;
        }
    }
    for (uint32_t index = 0U; index < layout.glyph_count; ++index) {
        const AfGlyphPlacement *glyph = &layout.glyphs[index];
        const float glyph_width =
            fmaxf(fabsf(glyph->advance.x), glyph->bearing.x + glyph->bitmap_size.x);
        uint32_t written = 0U;
        uint32_t batch_commands = ui->paths->command_count - command_start;
        uint32_t available = AF_MAX_PATH_COMMANDS_PER_PATH - batch_commands;
        AfResult result = AF_RESULT_SUCCESS;
        if (glyph->position.x < 0.0F || glyph->position.x + glyph_width > maximum_width + 0.25F) {
            break;
        }
        if (batch_glyph_count >= AF_UI_TEXT_GLYPHS_PER_PATH) {
            af_ui_finish_text_path(ui, command_start, (AfVec2){x, y}, color, layer);
            command_start = ui->paths->command_count;
            batch_glyph_count = 0U;
            available = AF_MAX_PATH_COMMANDS_PER_PATH;
        }
        if (available > ui->paths->command_capacity - ui->paths->command_count) {
            available = ui->paths->command_capacity - ui->paths->command_count;
        }
        if (available == 0U) {
            break;
        }
        result = af_text_glyph_outline(
            ui->text_engine, glyph->font_id, glyph->glyph_id, font_size, glyph->position,
            &ui->paths->commands[ui->paths->command_count], available, &written);
        if (result == AF_RESULT_LIMIT_EXCEEDED && batch_commands != 0U) {
            af_ui_finish_text_path(ui, command_start, (AfVec2){x, y}, color, layer);
            command_start = ui->paths->command_count;
            batch_glyph_count = 0U;
            available = ui->paths->command_capacity - ui->paths->command_count;
            if (available > AF_MAX_PATH_COMMANDS_PER_PATH) {
                available = AF_MAX_PATH_COMMANDS_PER_PATH;
            }
            result = available == 0U
                         ? AF_RESULT_LIMIT_EXCEEDED
                         : af_text_glyph_outline(ui->text_engine, glyph->font_id, glyph->glyph_id,
                                                 font_size, glyph->position,
                                                 &ui->paths->commands[ui->paths->command_count],
                                                 available, &written);
        }
        if (result != AF_RESULT_SUCCESS) {
            break;
        }
        ui->paths->command_count += written;
        if (written != 0U) {
            ++batch_glyph_count;
        }
    }
    af_ui_finish_text_path(ui, command_start, (AfVec2){x, y}, color, layer);
}
