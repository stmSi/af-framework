#ifndef AF_ANIMATION_H
#define AF_ANIMATION_H

#include "af/result.h"
#include "af/scene/scene.h"

#include <stdbool.h>
#include <stdint.h>

#define AF_MAX_CLIPS 256U
#define AF_MAX_TRACKS 8192U
#define AF_MAX_KEYFRAMES 65536U

typedef enum AfPropertyType {
    AF_PROPERTY_POSITION = 0,
    AF_PROPERTY_ROTATION,
    AF_PROPERTY_SCALE,
    AF_PROPERTY_OPACITY,
    AF_PROPERTY_FILL_COLOR,
    AF_PROPERTY_CORNER_RADIUS,
    AF_PROPERTY_SKEW,
    AF_PROPERTY_PATH_POINT,
    AF_PROPERTY_PATH_CONTROL_IN,
    AF_PROPERTY_PATH_CONTROL_OUT,
    AF_PROPERTY_OUTLINE_COLOR,
    AF_PROPERTY_GRADIENT_START,
    AF_PROPERTY_GRADIENT_END,
    AF_PROPERTY_GRADIENT_RADII,
    AF_PROPERTY_GRADIENT_ANGLE,
    AF_PROPERTY_GRADIENT_STOP_COLOR,
    AF_PROPERTY_GRADIENT_STOP_OFFSET,
    AF_PROPERTY_LAST = AF_PROPERTY_GRADIENT_STOP_OFFSET
} AfPropertyType;

/* Gradient elements 0..15 target fill stops; 16..31 target outline stops.
 * Geometry tracks use element 0 for fill and 16 for outline. */
#define AF_ANIMATION_OUTLINE_ELEMENT AF_MAX_GRADIENT_STOPS

static inline bool af_property_is_path(AfPropertyType property) {
    return property >= AF_PROPERTY_PATH_POINT && property <= AF_PROPERTY_PATH_CONTROL_OUT;
}
static inline bool af_property_is_gradient(AfPropertyType property) {
    return property >= AF_PROPERTY_GRADIENT_START && property <= AF_PROPERTY_GRADIENT_STOP_OFFSET;
}

bool af_animation_target_valid(const AfScene *scene, uint64_t node_id, AfPropertyType property,
                               uint32_t element_index);
AfResult af_animation_apply_value(AfScene *scene, uint64_t node_id, AfPropertyType property,
                                  uint32_t element_index, const float value[4]);

typedef enum AfInterpolation {
    AF_INTERPOLATION_STEP = 0,
    AF_INTERPOLATION_LINEAR,
    AF_INTERPOLATION_CUBIC_BEZIER
} AfInterpolation;

typedef struct AfKeyframe {
    float time;
    float value[4];
    AfInterpolation interpolation;
    float easing[4];
} AfKeyframe;

typedef struct AfAnimationTrack {
    uint64_t id;
    uint64_t node_id;
    AfPropertyType property;
    uint32_t keyframe_start;
    uint32_t keyframe_count;
    uint32_t element_index;
} AfAnimationTrack;

typedef struct AfAnimationClip {
    uint64_t id;
    float duration;
    uint32_t track_start;
    uint32_t track_count;
    bool loop;
} AfAnimationClip;

typedef struct AfAnimationData {
    AfAnimationClip *clips;
    uint32_t clip_count;
    AfAnimationTrack *tracks;
    uint32_t track_count;
    AfKeyframe *keyframes;
    uint32_t keyframe_count;
} AfAnimationData;

typedef struct AfPlayback {
    float time;
    float speed;
    int direction;
    bool playing;
} AfPlayback;

AfResult af_animation_validate(const AfAnimationData *animation, const AfScene *scene);
AfResult af_animation_sample_track(const AfAnimationData *animation, const AfAnimationTrack *track,
                                   float time, float out_value[4]);
AfResult af_animation_apply(const AfAnimationData *animation, const AfAnimationClip *clip,
                            float time, AfScene *scene);
void af_playback_play(AfPlayback *playback);
void af_playback_pause(AfPlayback *playback);
void af_playback_stop(AfPlayback *playback);
void af_playback_seek(AfPlayback *playback, const AfAnimationClip *clip, float time);
void af_playback_update(AfPlayback *playback, const AfAnimationClip *clip, float delta_time);

#endif
