#include "af/ui/actions.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

static bool af_ui_action_string_valid(const char *text, size_t capacity, bool allow_empty) {
    const char *end = text != NULL ? memchr(text, '\0', capacity) : NULL;
    return end != NULL && (allow_empty || end != text);
}

static AfResult af_ui_action_copy(char *output, size_t capacity, const char *text) {
    const char *end = NULL;
    size_t length = 0U;
    if (output == NULL || capacity == 0U || text == NULL) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    end = memchr(text, '\0', capacity);
    if (end == NULL) {
        return AF_RESULT_LIMIT_EXCEEDED;
    }
    length = (size_t)(end - text);
    memcpy(output, text, length + 1U);
    return AF_RESULT_SUCCESS;
}

static bool af_ui_action_contexts_valid(uint32_t contexts) { return contexts != 0U; }

static bool af_ui_action_contexts_overlap(uint32_t left, uint32_t right) {
    return (left & AF_UI_ACTION_CONTEXT_GLOBAL) != 0U ||
           (right & AF_UI_ACTION_CONTEXT_GLOBAL) != 0U || (left & right) != 0U;
}

static bool af_ui_action_context_matches(uint32_t action_contexts, uint32_t active_contexts) {
    return (action_contexts & AF_UI_ACTION_CONTEXT_GLOBAL) != 0U ||
           (action_contexts & active_contexts) != 0U;
}

static AfUiAction *af_ui_action_find_mutable(AfUiActionRegistry *registry, AfUiActionId action_id) {
    if (registry == NULL || registry->actions == NULL || action_id == 0U) {
        return NULL;
    }
    for (uint32_t index = 0U; index < registry->count; ++index) {
        if (registry->actions[index].id == action_id) {
            return &registry->actions[index];
        }
    }
    return NULL;
}

AfUiShortcut af_ui_shortcut_normalize(AfUiShortcut shortcut) {
    if (shortcut.key >= (uint32_t)'a' && shortcut.key <= (uint32_t)'z') {
        shortcut.key -= (uint32_t)'a' - (uint32_t)'A';
    }
    shortcut.modifiers &= AF_UI_SHORTCUT_MODIFIER_MASK;
    if (shortcut.key == 0U) {
        shortcut.modifiers = 0U;
    }
    return shortcut;
}

bool af_ui_shortcut_equal(AfUiShortcut left, AfUiShortcut right) {
    left = af_ui_shortcut_normalize(left);
    right = af_ui_shortcut_normalize(right);
    return left.key == right.key && left.modifiers == right.modifiers;
}

bool af_ui_shortcut_is_bound(AfUiShortcut shortcut) {
    return af_ui_shortcut_normalize(shortcut).key != 0U;
}

static bool af_ui_shortcut_valid(AfUiShortcut shortcut) {
    if ((shortcut.modifiers & ~AF_UI_SHORTCUT_MODIFIER_MASK) != 0U) {
        return false;
    }
    if (shortcut.key == 0U) {
        return shortcut.modifiers == 0U;
    }
    return (shortcut.key >= 32U && shortcut.key <= 126U) ||
           (shortcut.key >= AF_UI_KEY_ESCAPE && shortcut.key <= AF_UI_KEY_DOWN) ||
           (shortcut.key >= AF_UI_KEY_F1 && shortcut.key <= AF_UI_KEY_F12);
}

AfResult af_ui_action_registry_init(AfUiActionRegistry *registry, AfUiAction *actions,
                                    uint32_t capacity) {
    if (registry == NULL || capacity > AF_UI_MAX_ACTIONS || (capacity != 0U && actions == NULL)) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    *registry = (AfUiActionRegistry){.actions = actions, .capacity = capacity};
    return AF_RESULT_SUCCESS;
}

const AfUiAction *af_ui_action_find(const AfUiActionRegistry *registry, AfUiActionId action_id) {
    if (registry == NULL || registry->actions == NULL || action_id == 0U) {
        return NULL;
    }
    for (uint32_t index = 0U; index < registry->count; ++index) {
        if (registry->actions[index].id == action_id) {
            return &registry->actions[index];
        }
    }
    return NULL;
}

AfResult af_ui_action_shortcut_conflict(const AfUiActionRegistry *registry, AfUiActionId action_id,
                                        AfUiShortcut shortcut,
                                        AfUiActionId *out_conflicting_action_id) {
    const AfUiAction *target = af_ui_action_find(registry, action_id);
    if (out_conflicting_action_id != NULL) {
        *out_conflicting_action_id = 0U;
    }
    if (registry == NULL || target == NULL || !af_ui_shortcut_valid(shortcut)) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    shortcut = af_ui_shortcut_normalize(shortcut);
    if (!af_ui_shortcut_is_bound(shortcut)) {
        return AF_RESULT_NOT_FOUND;
    }
    for (uint32_t index = 0U; index < registry->count; ++index) {
        const AfUiAction *other = &registry->actions[index];
        if (other->id != action_id &&
            af_ui_action_contexts_overlap(target->contexts, other->contexts) &&
            af_ui_shortcut_equal(shortcut, other->shortcut)) {
            if (out_conflicting_action_id != NULL) {
                *out_conflicting_action_id = other->id;
            }
            return AF_RESULT_SUCCESS;
        }
    }
    return AF_RESULT_NOT_FOUND;
}

AfResult af_ui_action_register(AfUiActionRegistry *registry,
                               const AfUiActionDefinition *definition) {
    AfUiAction action = {0};
    if (registry == NULL || definition == NULL || definition->id == 0U ||
        af_ui_action_find(registry, definition->id) != NULL ||
        !af_ui_action_contexts_valid(definition->contexts) ||
        !af_ui_shortcut_valid(definition->default_shortcut) ||
        !af_ui_action_string_valid(definition->label, AF_UI_ACTION_LABEL_CAPACITY, false) ||
        !af_ui_action_string_valid(definition->tooltip, AF_UI_ACTION_TOOLTIP_CAPACITY, true) ||
        !af_ui_action_string_valid(definition->keywords, AF_UI_ACTION_KEYWORDS_CAPACITY, true)) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    if (registry->count >= registry->capacity) {
        return AF_RESULT_LIMIT_EXCEEDED;
    }
    action =
        (AfUiAction){.id = definition->id,
                     .default_shortcut = af_ui_shortcut_normalize(definition->default_shortcut),
                     .shortcut = af_ui_shortcut_normalize(definition->default_shortcut),
                     .contexts = definition->contexts,
                     .execute = definition->execute,
                     .user = definition->user,
                     .enabled = true,
                     .visible = true};
    (void)af_ui_action_copy(action.label, sizeof(action.label), definition->label);
    (void)af_ui_action_copy(action.tooltip, sizeof(action.tooltip), definition->tooltip);
    (void)af_ui_action_copy(action.keywords, sizeof(action.keywords), definition->keywords);
    for (uint32_t index = 0U; index < registry->count; ++index) {
        if (af_ui_shortcut_is_bound(action.shortcut) &&
            af_ui_action_contexts_overlap(action.contexts, registry->actions[index].contexts) &&
            af_ui_shortcut_equal(action.shortcut, registry->actions[index].shortcut)) {
            return AF_RESULT_INVALID_ARGUMENT;
        }
    }
    registry->actions[registry->count++] = action;
    return AF_RESULT_SUCCESS;
}

AfResult af_ui_action_set_enabled(AfUiActionRegistry *registry, AfUiActionId action_id,
                                  bool enabled) {
    AfUiAction *action = af_ui_action_find_mutable(registry, action_id);
    if (action == NULL) {
        return AF_RESULT_NOT_FOUND;
    }
    action->enabled = enabled;
    return AF_RESULT_SUCCESS;
}

AfResult af_ui_action_set_visible(AfUiActionRegistry *registry, AfUiActionId action_id,
                                  bool visible) {
    AfUiAction *action = af_ui_action_find_mutable(registry, action_id);
    if (action == NULL) {
        return AF_RESULT_NOT_FOUND;
    }
    action->visible = visible;
    return AF_RESULT_SUCCESS;
}

AfResult af_ui_action_set_shortcut(AfUiActionRegistry *registry, AfUiActionId action_id,
                                   AfUiShortcut shortcut, AfUiActionId *out_conflicting_action_id) {
    AfUiAction *action = af_ui_action_find_mutable(registry, action_id);
    AfResult conflict = AF_RESULT_SUCCESS;
    if (out_conflicting_action_id != NULL) {
        *out_conflicting_action_id = 0U;
    }
    if (action == NULL) {
        return AF_RESULT_NOT_FOUND;
    }
    if (!af_ui_shortcut_valid(shortcut)) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    shortcut = af_ui_shortcut_normalize(shortcut);
    conflict =
        af_ui_action_shortcut_conflict(registry, action_id, shortcut, out_conflicting_action_id);
    if (conflict == AF_RESULT_SUCCESS) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    if (conflict != AF_RESULT_NOT_FOUND) {
        return conflict;
    }
    action->shortcut = shortcut;
    return AF_RESULT_SUCCESS;
}

AfResult af_ui_action_reset_shortcut(AfUiActionRegistry *registry, AfUiActionId action_id,
                                     AfUiActionId *out_conflicting_action_id) {
    const AfUiAction *action = af_ui_action_find(registry, action_id);
    return action != NULL ? af_ui_action_set_shortcut(registry, action_id, action->default_shortcut,
                                                      out_conflicting_action_id)
                          : AF_RESULT_NOT_FOUND;
}

AfResult af_ui_action_resolve_shortcut(const AfUiActionRegistry *registry, AfUiShortcut shortcut,
                                       uint32_t contexts, AfUiActionId *out_action_id) {
    if (registry == NULL || out_action_id == NULL || !af_ui_action_contexts_valid(contexts) ||
        !af_ui_shortcut_valid(shortcut) || !af_ui_shortcut_is_bound(shortcut)) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    *out_action_id = 0U;
    shortcut = af_ui_shortcut_normalize(shortcut);
    for (uint32_t index = 0U; index < registry->count; ++index) {
        const AfUiAction *action = &registry->actions[index];
        if (action->enabled && action->visible &&
            af_ui_action_context_matches(action->contexts, contexts) &&
            af_ui_shortcut_equal(action->shortcut, shortcut)) {
            *out_action_id = action->id;
            return AF_RESULT_SUCCESS;
        }
    }
    return AF_RESULT_NOT_FOUND;
}

AfResult af_ui_action_execute(const AfUiActionRegistry *registry, AfUiActionId action_id) {
    const AfUiAction *action = af_ui_action_find(registry, action_id);
    if (action == NULL) {
        return AF_RESULT_NOT_FOUND;
    }
    if (!action->enabled) {
        return AF_RESULT_UNSUPPORTED_FEATURE;
    }
    return action->execute != NULL ? action->execute(action->user, action->id) : AF_RESULT_SUCCESS;
}

static int af_ui_ascii_lower(int character) {
    return character >= 'A' && character <= 'Z' ? character + ('a' - 'A') : character;
}

static const char *af_ui_ascii_find(const char *text, const char *needle, size_t needle_length) {
    if (text == NULL || needle == NULL || needle_length == 0U) {
        return text;
    }
    for (; *text != '\0'; ++text) {
        size_t index = 0U;
        while (index < needle_length && text[index] != '\0' &&
               af_ui_ascii_lower((unsigned char)text[index]) ==
                   af_ui_ascii_lower((unsigned char)needle[index])) {
            ++index;
        }
        if (index == needle_length) {
            return text;
        }
    }
    return NULL;
}

static uint32_t af_ui_action_token_score(const AfUiAction *action, const char *token,
                                         size_t length) {
    const char *match = af_ui_ascii_find(action->label, token, length);
    if (match != NULL) {
        const size_t position = (size_t)(match - action->label);
        return position == 0U ? 4000U : 3500U - (uint32_t)(position < 400U ? position : 400U);
    }
    match = af_ui_ascii_find(action->keywords, token, length);
    if (match != NULL) {
        const size_t position = (size_t)(match - action->keywords);
        return 2500U - (uint32_t)(position < 400U ? position : 400U);
    }
    match = af_ui_ascii_find(action->tooltip, token, length);
    if (match != NULL) {
        const size_t position = (size_t)(match - action->tooltip);
        return 1500U - (uint32_t)(position < 400U ? position : 400U);
    }
    return 0U;
}

static uint32_t af_ui_action_match_score(const AfUiAction *action, const char *query) {
    uint32_t score = 0U;
    uint32_t token_count = 0U;
    for (size_t index = 0U; query[index] != '\0';) {
        size_t start = 0U;
        size_t length = 0U;
        uint32_t token_score = 0U;
        while (query[index] != '\0' && isspace((unsigned char)query[index]) != 0) {
            ++index;
        }
        start = index;
        while (query[index] != '\0' && isspace((unsigned char)query[index]) == 0) {
            ++index;
        }
        length = index - start;
        if (length == 0U) {
            continue;
        }
        token_score = af_ui_action_token_score(action, query + start, length);
        if (token_score == 0U) {
            return 0U;
        }
        score += token_score;
        ++token_count;
    }
    return token_count == 0U ? 1U : score;
}

AfResult af_ui_action_search(const AfUiActionRegistry *registry, const char *query,
                             uint32_t contexts, AfUiActionSearchResult *results,
                             uint32_t result_capacity, uint32_t *out_result_count) {
    uint32_t count = 0U;
    if (registry == NULL || query == NULL || out_result_count == NULL ||
        !af_ui_action_contexts_valid(contexts) || (result_capacity != 0U && results == NULL)) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    *out_result_count = 0U;
    if (memchr(query, '\0', AF_UI_COMMAND_QUERY_CAPACITY) == NULL) {
        return AF_RESULT_LIMIT_EXCEEDED;
    }
    for (uint32_t index = 0U; index < registry->count; ++index) {
        const AfUiAction *action = &registry->actions[index];
        const uint32_t score = af_ui_action_match_score(action, query);
        uint32_t insertion = count;
        if (!action->enabled || !action->visible ||
            !af_ui_action_context_matches(action->contexts, contexts) || score == 0U) {
            continue;
        }
        while (insertion > 0U && (results[insertion - 1U].score < score ||
                                  (results[insertion - 1U].score == score &&
                                   results[insertion - 1U].action_id > action->id))) {
            --insertion;
        }
        if (insertion >= result_capacity) {
            continue;
        }
        if (count < result_capacity) {
            ++count;
        }
        if (insertion + 1U < count) {
            memmove(&results[insertion + 1U], &results[insertion],
                    (size_t)(count - insertion - 1U) * sizeof(results[0]));
        }
        results[insertion] = (AfUiActionSearchResult){.action_id = action->id, .score = score};
    }
    *out_result_count = count;
    return AF_RESULT_SUCCESS;
}

static const char *af_ui_special_key_name(uint32_t key) {
    static const char *const navigation[] = {"ESC",    "ENTER", "TAB", "SPACE", "BACKSPACE",
                                             "DELETE", "HOME",  "END", "PGUP",  "PGDN",
                                             "LEFT",   "RIGHT", "UP",  "DOWN"};
    static const char *const functions[] = {"F1", "F2", "F3", "F4",  "F5",  "F6",
                                            "F7", "F8", "F9", "F10", "F11", "F12"};
    if (key >= AF_UI_KEY_ESCAPE && key <= AF_UI_KEY_DOWN) {
        return navigation[key - AF_UI_KEY_ESCAPE];
    }
    if (key >= AF_UI_KEY_F1 && key <= AF_UI_KEY_F12) {
        return functions[key - AF_UI_KEY_F1];
    }
    return NULL;
}

static AfResult af_ui_shortcut_append(char *output, size_t capacity, size_t *length,
                                      const char *text) {
    const size_t text_length = strlen(text);
    if (*length > capacity || text_length >= capacity - *length) {
        return AF_RESULT_LIMIT_EXCEEDED;
    }
    memcpy(output + *length, text, text_length);
    *length += text_length;
    output[*length] = '\0';
    return AF_RESULT_SUCCESS;
}

AfResult af_ui_shortcut_format(AfUiShortcut shortcut, char *output, size_t output_capacity) {
    const char *key_name = NULL;
    char printable[2] = {0};
    size_t length = 0U;
    AfResult result = AF_RESULT_SUCCESS;
    if (output == NULL || output_capacity == 0U || !af_ui_shortcut_valid(shortcut)) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    output[0] = '\0';
    shortcut = af_ui_shortcut_normalize(shortcut);
    if (!af_ui_shortcut_is_bound(shortcut)) {
        return AF_RESULT_SUCCESS;
    }
    if ((shortcut.modifiers & AF_UI_SHORTCUT_MODIFIER_CTRL) != 0U) {
        result = af_ui_shortcut_append(output, output_capacity, &length, "CTRL+");
    }
    if (result == AF_RESULT_SUCCESS && (shortcut.modifiers & AF_UI_SHORTCUT_MODIFIER_SHIFT) != 0U) {
        result = af_ui_shortcut_append(output, output_capacity, &length, "SHIFT+");
    }
    if (result == AF_RESULT_SUCCESS && (shortcut.modifiers & AF_UI_SHORTCUT_MODIFIER_ALT) != 0U) {
        result = af_ui_shortcut_append(output, output_capacity, &length, "ALT+");
    }
    if (result == AF_RESULT_SUCCESS && (shortcut.modifiers & AF_UI_SHORTCUT_MODIFIER_SUPER) != 0U) {
        result = af_ui_shortcut_append(output, output_capacity, &length, "SUPER+");
    }
    key_name = af_ui_special_key_name(shortcut.key);
    if (key_name == NULL) {
        printable[0] = (char)shortcut.key;
        key_name = printable;
    }
    return result == AF_RESULT_SUCCESS
               ? af_ui_shortcut_append(output, output_capacity, &length, key_name)
               : result;
}

AfResult af_ui_command_palette_init(AfUiCommandPalette *palette, const AfUiActionRegistry *registry,
                                    uint32_t page_size) {
    if (palette == NULL || registry == NULL || page_size == 0U ||
        page_size > AF_UI_ACTION_SEARCH_CAPACITY) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    *palette = (AfUiCommandPalette){.registry = registry, .page_size = page_size};
    return AF_RESULT_SUCCESS;
}

AfResult af_ui_command_palette_open(AfUiCommandPalette *palette, uint32_t contexts) {
    if (palette == NULL || !af_ui_action_contexts_valid(contexts)) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    palette->open = true;
    palette->contexts = contexts;
    palette->query[0] = '\0';
    palette->selected_index = 0U;
    return af_ui_action_search(palette->registry, palette->query, palette->contexts,
                               palette->results, AF_UI_ACTION_SEARCH_CAPACITY,
                               &palette->result_count);
}

void af_ui_command_palette_close(AfUiCommandPalette *palette) {
    if (palette != NULL) {
        palette->open = false;
        palette->result_count = 0U;
        palette->selected_index = 0U;
    }
}

AfResult af_ui_command_palette_set_query(AfUiCommandPalette *palette, const char *query) {
    AfResult result = AF_RESULT_SUCCESS;
    if (palette == NULL || !palette->open || query == NULL) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    result = af_ui_action_copy(palette->query, sizeof(palette->query), query);
    if (result != AF_RESULT_SUCCESS) {
        return result;
    }
    palette->selected_index = 0U;
    return af_ui_action_search(palette->registry, palette->query, palette->contexts,
                               palette->results, AF_UI_ACTION_SEARCH_CAPACITY,
                               &palette->result_count);
}

AfResult af_ui_command_palette_handle_key(AfUiCommandPalette *palette, AfUiCommandPaletteKey key,
                                          AfUiActionId *out_action_id) {
    uint32_t step = 0U;
    if (out_action_id != NULL) {
        *out_action_id = 0U;
    }
    if (palette == NULL || !palette->open || key > AF_UI_COMMAND_PALETTE_CANCEL) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    if (key == AF_UI_COMMAND_PALETTE_CANCEL) {
        af_ui_command_palette_close(palette);
        return AF_RESULT_SUCCESS;
    }
    if (key == AF_UI_COMMAND_PALETTE_ACTIVATE) {
        if (palette->result_count == 0U || out_action_id == NULL) {
            return AF_RESULT_NOT_FOUND;
        }
        *out_action_id = palette->results[palette->selected_index].action_id;
        af_ui_command_palette_close(palette);
        return AF_RESULT_SUCCESS;
    }
    if (palette->result_count == 0U) {
        return AF_RESULT_NOT_FOUND;
    }
    switch (key) {
    case AF_UI_COMMAND_PALETTE_UP:
        palette->selected_index = palette->selected_index == 0U ? palette->result_count - 1U
                                                                : palette->selected_index - 1U;
        break;
    case AF_UI_COMMAND_PALETTE_DOWN:
        palette->selected_index = (palette->selected_index + 1U) % palette->result_count;
        break;
    case AF_UI_COMMAND_PALETTE_HOME:
        palette->selected_index = 0U;
        break;
    case AF_UI_COMMAND_PALETTE_END:
        palette->selected_index = palette->result_count - 1U;
        break;
    case AF_UI_COMMAND_PALETTE_PAGE_UP:
        step = palette->page_size < palette->selected_index ? palette->page_size
                                                            : palette->selected_index;
        palette->selected_index -= step;
        break;
    case AF_UI_COMMAND_PALETTE_PAGE_DOWN:
        step = palette->result_count - 1U - palette->selected_index;
        if (step > palette->page_size) {
            step = palette->page_size;
        }
        palette->selected_index += step;
        break;
    case AF_UI_COMMAND_PALETTE_ACTIVATE:
    case AF_UI_COMMAND_PALETTE_CANCEL:
        break;
    }
    return AF_RESULT_SUCCESS;
}

const AfUiAction *af_ui_command_palette_selected(const AfUiCommandPalette *palette) {
    if (palette == NULL || !palette->open || palette->selected_index >= palette->result_count) {
        return NULL;
    }
    return af_ui_action_find(palette->registry,
                             palette->results[palette->selected_index].action_id);
}
