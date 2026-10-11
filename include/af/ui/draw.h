#ifndef AF_UI_DRAW_H
#define AF_UI_DRAW_H

#include "af/scene/scene.h"
#include "af/text/text.h"

typedef struct AfUiHoverFeedback {
    uint64_t key;
    uint32_t frame;
    float amount;
} AfUiHoverFeedback;

/* Caller-owned buffers let UI and application geometry share one render scene.
 * The framework never owns the document, selection, menus, or editing history. */
typedef struct AfUiContext {
    AfScene *scene;
    AfPathData *paths;
    AfTextEngine *text_engine;
    AfGlyphPlacement *glyphs;
    AfTextLine *lines;
    uint32_t glyph_capacity;
    uint32_t line_capacity;
    uint64_t font_id;
    uint64_t node_id_base;
    AfVec2 pointer;
    AfColor contrast_border;
    AfUiHoverFeedback hover_feedback[256];
    uint32_t frame;
    float delta_seconds;
    /* Widgets below this application-defined layer cannot receive hover feedback. */
    int32_t interaction_blocked_below_layer;
    bool pointer_down;
    bool reduced_motion;
    bool high_contrast;
    bool overflowed;
} AfUiContext;

AfResult af_ui_init(AfUiContext *ui, AfScene *scene, AfPathData *paths);
void af_ui_begin_frame(AfUiContext *ui, float delta_seconds, AfVec2 pointer, bool pointer_down);
bool af_ui_hovered_rect(const AfUiContext *ui, float x, float y, float width, float height);
bool af_ui_point_in_rect(AfVec2 point, float x, float y, float width, float height);
void af_ui_add_shape(AfUiContext *ui, AfNodeType type, AfAffine2 transform, AfVec2 bounds,
                     AfColor color, float radius, int32_t layer);
void af_ui_add_rect(AfUiContext *ui, float x, float y, float width, float height, AfColor color,
                    int32_t layer);
void af_ui_add_surface(AfUiContext *ui, float x, float y, float width, float height, AfColor color,
                       float radius, int32_t layer);
void af_ui_add_button_rect(AfUiContext *ui, float x, float y, float width, float height,
                           AfColor color, int32_t layer);
void af_ui_add_button_rounded(AfUiContext *ui, float x, float y, float width, float height,
                              AfColor color, float radius, int32_t layer);
void af_ui_add_line(AfUiContext *ui, AfVec2 start, AfVec2 end, float thickness, AfColor color,
                    int32_t layer);
void af_ui_add_bitmap_text(AfUiContext *ui, float x, float y, const char *text, float scale,
                           float maximum_width, AfColor color, int32_t layer);
void af_ui_add_text(AfUiContext *ui, float x, float y, const char *text, float scale,
                    float maximum_width, AfColor color, int32_t layer);
/* Commit outlines appended by a host's text editor to the shared command buffer. */
void af_ui_finish_text_path(AfUiContext *ui, uint32_t command_start, AfVec2 origin, AfColor color,
                            int32_t layer);

#endif
