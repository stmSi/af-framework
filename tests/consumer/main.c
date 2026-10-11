#include "af/engine.h"
#include "af/ui.h"

#if defined(AF_CONSUMER_HAS_VULKAN)
#include "af/vulkan.h"
#endif

#include <stdio.h>

int main(void) {
    AfSceneNode nodes[4] = {0};
    AfScene scene;
    AfPathData paths = {0};
    AfUiContext ui;
    af_scene_init(&scene, nodes, 4U);
    if (af_ui_init(&ui, &scene, &paths) != AF_RESULT_SUCCESS)
        return 1;
    af_ui_begin_frame(&ui, 1.0F / 60.0F, (AfVec2){20.0F, 20.0F}, false);
    af_ui_add_button_rect(&ui, 10.0F, 10.0F, 80.0F, 30.0F, (AfColor){0.2F, 0.3F, 0.4F, 1.0F}, 1);
    if (scene.count != 2U || af_scene_validate(&scene) != AF_RESULT_SUCCESS)
        return 1;
#if defined(AF_CONSUMER_HAS_VULKAN)
    const AfVkDeviceContext empty = af_vk_renderer_device_context(NULL);
    if (empty.device != VK_NULL_HANDLE)
        return 1;
#endif
    puts("Standalone installed framework consumer passed");
    return 0;
}
