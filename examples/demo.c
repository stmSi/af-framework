#include "af/ui/draw.h"
#include "af/vulkan.h"

#include <SDL3/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argument_count, char **arguments) {
    uint32_t maximum_frames = 0U;
    const char *capture = NULL;
    const char *shader_directory = NULL;
    for (int index = 1; index < argument_count; ++index) {
        if (strcmp(arguments[index], "--frames") == 0 && index + 1 < argument_count)
            maximum_frames = (uint32_t)strtoul(arguments[++index], NULL, 10);
        else if (strcmp(arguments[index], "--output") == 0 && index + 1 < argument_count)
            capture = arguments[++index];
        else if (strcmp(arguments[index], "--shaders") == 0 && index + 1 < argument_count)
            shader_directory = arguments[++index];
    }
    AfPlatformWindow window = {0};
    AfVkRenderer *renderer = NULL;
    AfSceneNode nodes[512] = {0};
    AfScene scene;
    AfPathData paths = {0};
    AfUiContext ui;
    AfResult result = af_platform_window_create("Independent AF Framework Demo", 960, 640, &window);
    if (result == AF_RESULT_SUCCESS)
        result = af_vk_renderer_create_with_info(
            &(AfVkRendererCreateInfo){.window = &window, .shader_directory = shader_directory},
            &renderer);
    af_scene_init(&scene, nodes, 512U);
    if (result == AF_RESULT_SUCCESS)
        result = af_ui_init(&ui, &scene, &paths);
    uint32_t frame = 0U;
    bool running = result == AF_RESULT_SUCCESS;
    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT)
                running = false;
            else if (event.type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED)
                af_vk_renderer_request_resize(renderer);
        }
        if (!running)
            break;
        float pointer_x, pointer_y;
        const SDL_MouseButtonFlags buttons = SDL_GetMouseState(&pointer_x, &pointer_y);
        scene.count = 0U;
        af_ui_begin_frame(&ui, 1.0F / 60.0F, (AfVec2){pointer_x, pointer_y},
                          (buttons & SDL_BUTTON_LMASK) != 0U);
        af_ui_add_rect(&ui, 0.0F, 0.0F, 960.0F, 640.0F, (AfColor){0.08F, 0.09F, 0.12F, 1.0F}, 0);
        af_ui_add_surface(&ui, 40.0F, 40.0F, 880.0F, 560.0F, (AfColor){0.13F, 0.15F, 0.2F, 1.0F},
                          14.0F, 1);
        af_ui_add_bitmap_text(&ui, 72.0F, 76.0F, "REUSABLE UI + ENGINE + VULKAN", 3.0F, 816.0F,
                              (AfColor){0.75F, 0.7F, 1.0F, 1.0F}, 2);
        af_ui_add_button_rect(&ui, 72.0F, 150.0F, 230.0F, 54.0F, (AfColor){0.25F, 0.2F, 0.5F, 1.0F},
                              2);
        af_ui_add_bitmap_text(&ui, 92.0F, 170.0F, "HOVER THIS BUTTON", 1.5F, 194.0F,
                              (AfColor){1.0F, 1.0F, 1.0F, 1.0F}, 4);
        af_ui_add_shape(&ui, AF_NODE_ELLIPSE,
                        (AfAffine2){.m00 = 1.0F, .m11 = 1.0F, .tx = 400.0F, .ty = 280.0F},
                        (AfVec2){160.0F, 160.0F}, (AfColor){0.3F, 0.8F, 0.65F, 1.0F}, 0.0F, 2);
        if (capture != NULL && (maximum_frames == 0U || frame + 1U == maximum_frames))
            result = af_vk_renderer_capture_next(renderer, capture);
        if (result == AF_RESULT_SUCCESS)
            result = af_vk_renderer_draw(renderer, &scene);
        if (result != AF_RESULT_SUCCESS)
            break;
        ++frame;
        if (maximum_frames != 0U && frame >= maximum_frames)
            break;
    }
    if (af_vk_renderer_has_validation_errors(renderer))
        result = AF_RESULT_VULKAN_ERROR;
    af_vk_renderer_destroy(renderer);
    af_platform_window_destroy(&window);
    if (result != AF_RESULT_SUCCESS)
        fprintf(stderr, "Framework demo: %s\n", af_result_string(result));
    return result == AF_RESULT_SUCCESS ? 0 : 1;
}
