#ifndef AF_UI_ACTIONS_H
#define AF_UI_ACTIONS_H

#include "af/result.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define AF_UI_MAX_ACTIONS 256U
#define AF_UI_ACTION_LABEL_CAPACITY 64U
#define AF_UI_ACTION_TOOLTIP_CAPACITY 192U
#define AF_UI_ACTION_KEYWORDS_CAPACITY 128U
#define AF_UI_ACTION_SEARCH_CAPACITY 128U
#define AF_UI_COMMAND_QUERY_CAPACITY 96U
#define AF_UI_SHORTCUT_TEXT_CAPACITY 64U

typedef uint32_t AfUiActionId;

/* Context bits are supplied by the host application. Bit zero is global. */
typedef uint32_t AfUiActionContext;
#define AF_UI_ACTION_CONTEXT_GLOBAL UINT32_C(1)
#define AF_UI_ACTION_CONTEXT_ALL UINT32_MAX

typedef enum AfUiShortcutModifier {
    AF_UI_SHORTCUT_MODIFIER_CTRL = 1U << 0U,
    AF_UI_SHORTCUT_MODIFIER_SHIFT = 1U << 1U,
    AF_UI_SHORTCUT_MODIFIER_ALT = 1U << 2U,
    AF_UI_SHORTCUT_MODIFIER_SUPER = 1U << 3U
} AfUiShortcutModifier;

#define AF_UI_SHORTCUT_MODIFIER_MASK UINT32_C(0x0000000f)

typedef enum AfUiSpecialKey {
    AF_UI_KEY_ESCAPE = 0x100U,
    AF_UI_KEY_ENTER,
    AF_UI_KEY_TAB,
    AF_UI_KEY_SPACE,
    AF_UI_KEY_BACKSPACE,
    AF_UI_KEY_DELETE,
    AF_UI_KEY_HOME,
    AF_UI_KEY_END,
    AF_UI_KEY_PAGE_UP,
    AF_UI_KEY_PAGE_DOWN,
    AF_UI_KEY_LEFT,
    AF_UI_KEY_RIGHT,
    AF_UI_KEY_UP,
    AF_UI_KEY_DOWN,
    AF_UI_KEY_F1 = 0x120U,
    AF_UI_KEY_F2,
    AF_UI_KEY_F3,
    AF_UI_KEY_F4,
    AF_UI_KEY_F5,
    AF_UI_KEY_F6,
    AF_UI_KEY_F7,
    AF_UI_KEY_F8,
    AF_UI_KEY_F9,
    AF_UI_KEY_F10,
    AF_UI_KEY_F11,
    AF_UI_KEY_F12
} AfUiSpecialKey;

typedef struct AfUiShortcut {
    uint32_t key;
    uint32_t modifiers;
} AfUiShortcut;

typedef AfResult (*AfUiActionExecuteFn)(void *user, AfUiActionId action_id);

typedef struct AfUiActionDefinition {
    AfUiActionId id;
    const char *label;
    const char *tooltip;
    const char *keywords;
    AfUiShortcut default_shortcut;
    uint32_t contexts;
    AfUiActionExecuteFn execute;
    void *user;
} AfUiActionDefinition;

typedef struct AfUiAction {
    AfUiActionId id;
    char label[AF_UI_ACTION_LABEL_CAPACITY];
    char tooltip[AF_UI_ACTION_TOOLTIP_CAPACITY];
    char keywords[AF_UI_ACTION_KEYWORDS_CAPACITY];
    AfUiShortcut default_shortcut;
    AfUiShortcut shortcut;
    uint32_t contexts;
    AfUiActionExecuteFn execute;
    void *user;
    bool enabled;
    bool visible;
} AfUiAction;

typedef struct AfUiActionRegistry {
    AfUiAction *actions;
    uint32_t count;
    uint32_t capacity;
} AfUiActionRegistry;

typedef struct AfUiActionSearchResult {
    AfUiActionId action_id;
    uint32_t score;
} AfUiActionSearchResult;

typedef enum AfUiCommandPaletteKey {
    AF_UI_COMMAND_PALETTE_UP = 0,
    AF_UI_COMMAND_PALETTE_DOWN,
    AF_UI_COMMAND_PALETTE_HOME,
    AF_UI_COMMAND_PALETTE_END,
    AF_UI_COMMAND_PALETTE_PAGE_UP,
    AF_UI_COMMAND_PALETTE_PAGE_DOWN,
    AF_UI_COMMAND_PALETTE_ACTIVATE,
    AF_UI_COMMAND_PALETTE_CANCEL
} AfUiCommandPaletteKey;

typedef struct AfUiCommandPalette {
    const AfUiActionRegistry *registry;
    AfUiActionSearchResult results[AF_UI_ACTION_SEARCH_CAPACITY];
    char query[AF_UI_COMMAND_QUERY_CAPACITY];
    uint32_t result_count;
    uint32_t selected_index;
    uint32_t page_size;
    uint32_t contexts;
    bool open;
} AfUiCommandPalette;

AfResult af_ui_action_registry_init(AfUiActionRegistry *registry, AfUiAction *actions,
                                    uint32_t capacity);
AfResult af_ui_action_register(AfUiActionRegistry *registry,
                               const AfUiActionDefinition *definition);
const AfUiAction *af_ui_action_find(const AfUiActionRegistry *registry, AfUiActionId action_id);
AfResult af_ui_action_set_enabled(AfUiActionRegistry *registry, AfUiActionId action_id,
                                  bool enabled);
AfResult af_ui_action_set_visible(AfUiActionRegistry *registry, AfUiActionId action_id,
                                  bool visible);
AfResult af_ui_action_set_shortcut(AfUiActionRegistry *registry, AfUiActionId action_id,
                                   AfUiShortcut shortcut, AfUiActionId *out_conflicting_action_id);
AfResult af_ui_action_reset_shortcut(AfUiActionRegistry *registry, AfUiActionId action_id,
                                     AfUiActionId *out_conflicting_action_id);
AfResult af_ui_action_shortcut_conflict(const AfUiActionRegistry *registry, AfUiActionId action_id,
                                        AfUiShortcut shortcut,
                                        AfUiActionId *out_conflicting_action_id);
AfResult af_ui_action_resolve_shortcut(const AfUiActionRegistry *registry, AfUiShortcut shortcut,
                                       uint32_t contexts, AfUiActionId *out_action_id);
AfResult af_ui_action_execute(const AfUiActionRegistry *registry, AfUiActionId action_id);
AfResult af_ui_action_search(const AfUiActionRegistry *registry, const char *query,
                             uint32_t contexts, AfUiActionSearchResult *results,
                             uint32_t result_capacity, uint32_t *out_result_count);
AfUiShortcut af_ui_shortcut_normalize(AfUiShortcut shortcut);
bool af_ui_shortcut_equal(AfUiShortcut left, AfUiShortcut right);
bool af_ui_shortcut_is_bound(AfUiShortcut shortcut);
AfResult af_ui_shortcut_format(AfUiShortcut shortcut, char *output, size_t output_capacity);

AfResult af_ui_command_palette_init(AfUiCommandPalette *palette, const AfUiActionRegistry *registry,
                                    uint32_t page_size);
AfResult af_ui_command_palette_open(AfUiCommandPalette *palette, uint32_t contexts);
void af_ui_command_palette_close(AfUiCommandPalette *palette);
AfResult af_ui_command_palette_set_query(AfUiCommandPalette *palette, const char *query);
AfResult af_ui_command_palette_handle_key(AfUiCommandPalette *palette, AfUiCommandPaletteKey key,
                                          AfUiActionId *out_action_id);
const AfUiAction *af_ui_command_palette_selected(const AfUiCommandPalette *palette);

#endif
