// animator.h
// Animation system for MiSTer modern frontend
// Provides smooth transitions, easing functions, and timed animations
// 2024

#ifndef __ANIMATOR_H__
#define __ANIMATOR_H__

#include <inttypes.h>

// Maximum concurrent animations
#define ANIM_MAX_ACTIVE 32

// Animation easing types
typedef enum {
	EASE_LINEAR = 0,
	EASE_IN_QUAD,
	EASE_OUT_QUAD,
	EASE_IN_OUT_QUAD,
	EASE_IN_CUBIC,
	EASE_OUT_CUBIC,
	EASE_IN_OUT_CUBIC,
	EASE_IN_EXPO,
	EASE_OUT_EXPO,
	EASE_IN_OUT_EXPO,
	EASE_IN_BACK,
	EASE_OUT_BACK,
	EASE_IN_OUT_BACK,
	EASE_IN_ELASTIC,
	EASE_OUT_ELASTIC,
	EASE_OUT_BOUNCE,
	EASE_COUNT
} anim_easing_t;

// Animation state
typedef enum {
	ANIM_IDLE = 0,
	ANIM_RUNNING,
	ANIM_PAUSED,
	ANIM_FINISHED
} anim_state_t;

// Animation target types
typedef enum {
	ANIM_TARGET_FLOAT = 0,
	ANIM_TARGET_INT,
	ANIM_TARGET_COLOR
} anim_target_type_t;

// Single animation instance
typedef struct {
	uint32_t id;                    // Unique animation ID
	anim_state_t state;             // Current state
	anim_easing_t easing;           // Easing function
	anim_target_type_t target_type; // Type of value being animated

	float start_value;              // Starting value
	float end_value;                // Ending value
	float current_value;            // Current interpolated value

	float duration;                 // Total duration in seconds
	float elapsed;                  // Elapsed time in seconds
	float delay;                    // Delay before starting

	void *target_ptr;               // Pointer to value to update (optional)
	void (*on_complete)(uint32_t id, void *user_data);  // Completion callback
	void *user_data;                // User data for callback

	uint8_t loop;                   // Loop animation
	uint8_t ping_pong;              // Reverse on completion
	uint8_t active;                 // Slot is in use
} anim_instance_t;

// Color animation (ARGB)
typedef struct {
	uint8_t a, r, g, b;
} anim_color_t;

// Animation group (for coordinated animations)
typedef struct {
	uint32_t ids[8];
	int count;
	uint8_t active;
} anim_group_t;

//// Core Functions ////

// Initialize animation system
void anim_init(void);

// Shutdown animation system
void anim_shutdown(void);

// Update all animations (call once per frame)
// delta_time: time since last update in seconds
void anim_update(float delta_time);

// Get current time in seconds (for timing)
float anim_get_time(void);

//// Animation Creation ////

// Create a new float animation
// Returns animation ID, or 0 on failure
uint32_t anim_create(float from, float to, float duration, anim_easing_t easing);

// Create animation with target pointer (auto-updates value)
uint32_t anim_create_to(float *target, float to, float duration, anim_easing_t easing);

// Create animation from current value to target
uint32_t anim_create_from_current(float *target, float to, float duration, anim_easing_t easing);

// Create integer animation
uint32_t anim_create_int(int *target, int to, float duration, anim_easing_t easing);

// Create color animation
uint32_t anim_create_color(anim_color_t *target, anim_color_t to, float duration, anim_easing_t easing);

//// Animation Control ////

// Start an animation
void anim_start(uint32_t id);

// Pause an animation
void anim_pause(uint32_t id);

// Resume a paused animation
void anim_resume(uint32_t id);

// Stop and reset an animation
void anim_stop(uint32_t id);

// Stop and remove an animation
void anim_cancel(uint32_t id);

// Set animation delay
void anim_set_delay(uint32_t id, float delay);

// Set loop mode
void anim_set_loop(uint32_t id, int loop);

// Set ping-pong mode (reverse on complete)
void anim_set_ping_pong(uint32_t id, int ping_pong);

// Set completion callback
void anim_set_callback(uint32_t id, void (*callback)(uint32_t, void*), void *user_data);

//// Animation Queries ////

// Get current animation value
float anim_get_value(uint32_t id);

// Get animation progress (0.0 to 1.0)
float anim_get_progress(uint32_t id);

// Check if animation is running
int anim_is_running(uint32_t id);

// Check if animation is finished
int anim_is_finished(uint32_t id);

// Get animation state
anim_state_t anim_get_state(uint32_t id);

//// Animation Groups ////

// Create animation group
uint32_t anim_group_create(void);

// Add animation to group
void anim_group_add(uint32_t group_id, uint32_t anim_id);

// Start all animations in group
void anim_group_start(uint32_t group_id);

// Stop all animations in group
void anim_group_stop(uint32_t group_id);

// Check if all animations in group are finished
int anim_group_is_finished(uint32_t group_id);

//// Easing Functions ////

// Apply easing function to normalized time (0.0 to 1.0)
float anim_ease(float t, anim_easing_t easing);

// Get easing function name
const char* anim_easing_name(anim_easing_t easing);

//// Utility Functions ////

// Interpolate between two values
float anim_lerp(float a, float b, float t);

// Interpolate colors
anim_color_t anim_lerp_color(anim_color_t a, anim_color_t b, float t);

// Clamp value to range
float anim_clamp(float value, float min, float max);

// Map value from one range to another
float anim_map(float value, float in_min, float in_max, float out_min, float out_max);

// Smooth step (Hermite interpolation)
float anim_smoothstep(float edge0, float edge1, float x);

//// Preset Animations ////

// Fade in (0 to 1)
uint32_t anim_fade_in(float *opacity, float duration);

// Fade out (1 to 0)
uint32_t anim_fade_out(float *opacity, float duration);

// Pulse (scale up and down)
uint32_t anim_pulse(float *scale, float min_scale, float max_scale, float duration);

// Slide in from direction
uint32_t anim_slide_in(float *position, float from, float to, float duration);

// Bounce effect
uint32_t anim_bounce(float *value, float target, float duration);

// Shake effect (returns to original)
uint32_t anim_shake(float *value, float intensity, float duration);

//// Debug ////

// Print active animations
void anim_debug_print(void);

// Get count of active animations
int anim_get_active_count(void);

#endif // __ANIMATOR_H__
