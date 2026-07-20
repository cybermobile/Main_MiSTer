// animator.cpp
// Animation system for MiSTer modern frontend
// 2024

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <sys/time.h>

#include "animator.h"

// Constants for easing calculations
#define PI 3.14159265358979323846f
#define PI_2 (PI / 2.0f)

// Animation storage
static anim_instance_t animations[ANIM_MAX_ACTIVE];
static anim_group_t groups[8];
static uint32_t next_id = 1;
static struct timeval start_time;
static int initialized = 0;

// Forward declarations
static anim_instance_t* find_animation(uint32_t id);
static int find_free_slot(void);

void anim_init(void)
{
	memset(animations, 0, sizeof(animations));
	memset(groups, 0, sizeof(groups));
	next_id = 1;
	gettimeofday(&start_time, NULL);
	initialized = 1;
	printf("Animation system initialized\n");
}

void anim_shutdown(void)
{
	memset(animations, 0, sizeof(animations));
	memset(groups, 0, sizeof(groups));
	initialized = 0;
	printf("Animation system shutdown\n");
}

float anim_get_time(void)
{
	struct timeval now;
	gettimeofday(&now, NULL);
	return (now.tv_sec - start_time.tv_sec) +
	       (now.tv_usec - start_time.tv_usec) / 1000000.0f;
}

static anim_instance_t* find_animation(uint32_t id)
{
	if (id == 0) return NULL;
	for (int i = 0; i < ANIM_MAX_ACTIVE; i++)
	{
		if (animations[i].active && animations[i].id == id)
		{
			return &animations[i];
		}
	}
	return NULL;
}

static int find_free_slot(void)
{
	for (int i = 0; i < ANIM_MAX_ACTIVE; i++)
	{
		if (!animations[i].active)
		{
			return i;
		}
	}
	return -1;
}

// Easing functions
float anim_ease(float t, anim_easing_t easing)
{
	// Clamp t to 0-1
	if (t <= 0.0f) return 0.0f;
	if (t >= 1.0f) return 1.0f;

	switch (easing)
	{
		case EASE_LINEAR:
			return t;

		case EASE_IN_QUAD:
			return t * t;

		case EASE_OUT_QUAD:
			return t * (2.0f - t);

		case EASE_IN_OUT_QUAD:
			return t < 0.5f ? 2.0f * t * t : -1.0f + (4.0f - 2.0f * t) * t;

		case EASE_IN_CUBIC:
			return t * t * t;

		case EASE_OUT_CUBIC:
			{
				float t1 = t - 1.0f;
				return t1 * t1 * t1 + 1.0f;
			}

		case EASE_IN_OUT_CUBIC:
			return t < 0.5f ? 4.0f * t * t * t : (t - 1.0f) * (2.0f * t - 2.0f) * (2.0f * t - 2.0f) + 1.0f;

		case EASE_IN_EXPO:
			return t == 0.0f ? 0.0f : powf(2.0f, 10.0f * (t - 1.0f));

		case EASE_OUT_EXPO:
			return t == 1.0f ? 1.0f : 1.0f - powf(2.0f, -10.0f * t);

		case EASE_IN_OUT_EXPO:
			if (t == 0.0f) return 0.0f;
			if (t == 1.0f) return 1.0f;
			if (t < 0.5f) return powf(2.0f, 20.0f * t - 10.0f) / 2.0f;
			return (2.0f - powf(2.0f, -20.0f * t + 10.0f)) / 2.0f;

		case EASE_IN_BACK:
			{
				float c1 = 1.70158f;
				float c3 = c1 + 1.0f;
				return c3 * t * t * t - c1 * t * t;
			}

		case EASE_OUT_BACK:
			{
				float c1 = 1.70158f;
				float c3 = c1 + 1.0f;
				float t1 = t - 1.0f;
				return 1.0f + c3 * t1 * t1 * t1 + c1 * t1 * t1;
			}

		case EASE_IN_OUT_BACK:
			{
				float c1 = 1.70158f;
				float c2 = c1 * 1.525f;
				if (t < 0.5f)
				{
					return (powf(2.0f * t, 2.0f) * ((c2 + 1.0f) * 2.0f * t - c2)) / 2.0f;
				}
				return (powf(2.0f * t - 2.0f, 2.0f) * ((c2 + 1.0f) * (t * 2.0f - 2.0f) + c2) + 2.0f) / 2.0f;
			}

		case EASE_IN_ELASTIC:
			{
				float c4 = (2.0f * PI) / 3.0f;
				if (t == 0.0f) return 0.0f;
				if (t == 1.0f) return 1.0f;
				return -powf(2.0f, 10.0f * t - 10.0f) * sinf((t * 10.0f - 10.75f) * c4);
			}

		case EASE_OUT_ELASTIC:
			{
				float c4 = (2.0f * PI) / 3.0f;
				if (t == 0.0f) return 0.0f;
				if (t == 1.0f) return 1.0f;
				return powf(2.0f, -10.0f * t) * sinf((t * 10.0f - 0.75f) * c4) + 1.0f;
			}

		case EASE_OUT_BOUNCE:
			{
				float n1 = 7.5625f;
				float d1 = 2.75f;
				if (t < 1.0f / d1)
				{
					return n1 * t * t;
				}
				else if (t < 2.0f / d1)
				{
					float t1 = t - 1.5f / d1;
					return n1 * t1 * t1 + 0.75f;
				}
				else if (t < 2.5f / d1)
				{
					float t1 = t - 2.25f / d1;
					return n1 * t1 * t1 + 0.9375f;
				}
				else
				{
					float t1 = t - 2.625f / d1;
					return n1 * t1 * t1 + 0.984375f;
				}
			}

		default:
			return t;
	}
}

const char* anim_easing_name(anim_easing_t easing)
{
	static const char* names[] = {
		"Linear",
		"EaseInQuad",
		"EaseOutQuad",
		"EaseInOutQuad",
		"EaseInCubic",
		"EaseOutCubic",
		"EaseInOutCubic",
		"EaseInExpo",
		"EaseOutExpo",
		"EaseInOutExpo",
		"EaseInBack",
		"EaseOutBack",
		"EaseInOutBack",
		"EaseInElastic",
		"EaseOutElastic",
		"EaseOutBounce"
	};

	if (easing < EASE_COUNT)
	{
		return names[easing];
	}
	return "Unknown";
}

float anim_lerp(float a, float b, float t)
{
	return a + (b - a) * t;
}

anim_color_t anim_lerp_color(anim_color_t a, anim_color_t b, float t)
{
	anim_color_t result;
	result.a = (uint8_t)(a.a + (b.a - a.a) * t);
	result.r = (uint8_t)(a.r + (b.r - a.r) * t);
	result.g = (uint8_t)(a.g + (b.g - a.g) * t);
	result.b = (uint8_t)(a.b + (b.b - a.b) * t);
	return result;
}

float anim_clamp(float value, float min, float max)
{
	if (value < min) return min;
	if (value > max) return max;
	return value;
}

float anim_map(float value, float in_min, float in_max, float out_min, float out_max)
{
	return (value - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

float anim_smoothstep(float edge0, float edge1, float x)
{
	float t = anim_clamp((x - edge0) / (edge1 - edge0), 0.0f, 1.0f);
	return t * t * (3.0f - 2.0f * t);
}

void anim_update(float delta_time)
{
	if (!initialized) return;

	for (int i = 0; i < ANIM_MAX_ACTIVE; i++)
	{
		anim_instance_t *anim = &animations[i];
		if (!anim->active || anim->state != ANIM_RUNNING) continue;

		// Handle delay
		if (anim->delay > 0.0f)
		{
			anim->delay -= delta_time;
			if (anim->delay > 0.0f) continue;
			delta_time = -anim->delay;  // Use overflow time
			anim->delay = 0.0f;
		}

		// Update elapsed time
		anim->elapsed += delta_time;

		// Calculate progress
		float progress = anim->elapsed / anim->duration;
		if (progress > 1.0f) progress = 1.0f;

		// Apply easing
		float eased = anim_ease(progress, anim->easing);

		// Calculate current value
		anim->current_value = anim_lerp(anim->start_value, anim->end_value, eased);

		// Update target pointer if set
		if (anim->target_ptr)
		{
			switch (anim->target_type)
			{
				case ANIM_TARGET_FLOAT:
					*((float*)anim->target_ptr) = anim->current_value;
					break;
				case ANIM_TARGET_INT:
					*((int*)anim->target_ptr) = (int)anim->current_value;
					break;
				case ANIM_TARGET_COLOR:
					// Color handled separately
					break;
			}
		}

		// Check if animation is complete
		if (progress >= 1.0f)
		{
			if (anim->ping_pong)
			{
				// Swap start and end, restart
				float temp = anim->start_value;
				anim->start_value = anim->end_value;
				anim->end_value = temp;
				anim->elapsed = 0.0f;
			}
			else if (anim->loop)
			{
				// Restart
				anim->elapsed = 0.0f;
			}
			else
			{
				// Finished
				anim->state = ANIM_FINISHED;

				// Call completion callback
				if (anim->on_complete)
				{
					anim->on_complete(anim->id, anim->user_data);
				}

				// Release the pool slot. Without this, finished-but-active
				// animations are never reclaimed (callers only cancel while
				// anim_is_running()), so the fixed pool exhausts after enough
				// completed animations and anim_create() starts failing.
				anim->active = 0;
			}
		}
	}
}

uint32_t anim_create(float from, float to, float duration, anim_easing_t easing)
{
	int slot = find_free_slot();
	if (slot < 0) return 0;

	anim_instance_t *anim = &animations[slot];
	memset(anim, 0, sizeof(anim_instance_t));

	anim->id = next_id++;
	anim->state = ANIM_IDLE;
	anim->easing = easing;
	anim->target_type = ANIM_TARGET_FLOAT;
	anim->start_value = from;
	anim->end_value = to;
	anim->current_value = from;
	anim->duration = duration;
	anim->elapsed = 0.0f;
	anim->delay = 0.0f;
	anim->active = 1;

	return anim->id;
}

uint32_t anim_create_to(float *target, float to, float duration, anim_easing_t easing)
{
	if (!target) return 0;

	uint32_t id = anim_create(*target, to, duration, easing);
	if (id)
	{
		anim_instance_t *anim = find_animation(id);
		if (anim)
		{
			anim->target_ptr = target;
			anim->target_type = ANIM_TARGET_FLOAT;
		}
	}
	return id;
}

uint32_t anim_create_from_current(float *target, float to, float duration, anim_easing_t easing)
{
	return anim_create_to(target, to, duration, easing);
}

uint32_t anim_create_int(int *target, int to, float duration, anim_easing_t easing)
{
	if (!target) return 0;

	uint32_t id = anim_create((float)*target, (float)to, duration, easing);
	if (id)
	{
		anim_instance_t *anim = find_animation(id);
		if (anim)
		{
			anim->target_ptr = target;
			anim->target_type = ANIM_TARGET_INT;
		}
	}
	return id;
}

uint32_t anim_create_color(anim_color_t *target, anim_color_t to, float duration, anim_easing_t easing)
{
	// For colors, we animate a progress value and interpolate
	if (!target) return 0;

	// Store color data in user_data (hacky but works)
	uint32_t id = anim_create(0.0f, 1.0f, duration, easing);
	if (id)
	{
		anim_instance_t *anim = find_animation(id);
		if (anim)
		{
			anim->target_ptr = target;
			anim->target_type = ANIM_TARGET_COLOR;
			// Store start color in start_value area (reinterpret)
			// This is a simplified approach
		}
	}
	return id;
}

void anim_start(uint32_t id)
{
	anim_instance_t *anim = find_animation(id);
	if (anim)
	{
		anim->state = ANIM_RUNNING;
		anim->elapsed = 0.0f;
		anim->current_value = anim->start_value;
	}
}

void anim_pause(uint32_t id)
{
	anim_instance_t *anim = find_animation(id);
	if (anim && anim->state == ANIM_RUNNING)
	{
		anim->state = ANIM_PAUSED;
	}
}

void anim_resume(uint32_t id)
{
	anim_instance_t *anim = find_animation(id);
	if (anim && anim->state == ANIM_PAUSED)
	{
		anim->state = ANIM_RUNNING;
	}
}

void anim_stop(uint32_t id)
{
	anim_instance_t *anim = find_animation(id);
	if (anim)
	{
		anim->state = ANIM_IDLE;
		anim->elapsed = 0.0f;
		anim->current_value = anim->start_value;
		if (anim->target_ptr && anim->target_type == ANIM_TARGET_FLOAT)
		{
			*((float*)anim->target_ptr) = anim->start_value;
		}
	}
}

void anim_cancel(uint32_t id)
{
	anim_instance_t *anim = find_animation(id);
	if (anim)
	{
		anim->active = 0;
		memset(anim, 0, sizeof(anim_instance_t));
	}
}

void anim_set_delay(uint32_t id, float delay)
{
	anim_instance_t *anim = find_animation(id);
	if (anim)
	{
		anim->delay = delay;
	}
}

void anim_set_loop(uint32_t id, int loop)
{
	anim_instance_t *anim = find_animation(id);
	if (anim)
	{
		anim->loop = loop ? 1 : 0;
	}
}

void anim_set_ping_pong(uint32_t id, int ping_pong)
{
	anim_instance_t *anim = find_animation(id);
	if (anim)
	{
		anim->ping_pong = ping_pong ? 1 : 0;
	}
}

void anim_set_callback(uint32_t id, void (*callback)(uint32_t, void*), void *user_data)
{
	anim_instance_t *anim = find_animation(id);
	if (anim)
	{
		anim->on_complete = callback;
		anim->user_data = user_data;
	}
}

float anim_get_value(uint32_t id)
{
	anim_instance_t *anim = find_animation(id);
	return anim ? anim->current_value : 0.0f;
}

float anim_get_progress(uint32_t id)
{
	anim_instance_t *anim = find_animation(id);
	if (!anim || anim->duration <= 0.0f) return 0.0f;
	float progress = anim->elapsed / anim->duration;
	return anim_clamp(progress, 0.0f, 1.0f);
}

int anim_is_running(uint32_t id)
{
	anim_instance_t *anim = find_animation(id);
	return anim && anim->state == ANIM_RUNNING;
}

int anim_is_finished(uint32_t id)
{
	anim_instance_t *anim = find_animation(id);
	return anim && anim->state == ANIM_FINISHED;
}

anim_state_t anim_get_state(uint32_t id)
{
	anim_instance_t *anim = find_animation(id);
	return anim ? anim->state : ANIM_IDLE;
}

// Animation groups
uint32_t anim_group_create(void)
{
	for (int i = 0; i < 8; i++)
	{
		if (!groups[i].active)
		{
			memset(&groups[i], 0, sizeof(anim_group_t));
			groups[i].active = 1;
			return i + 1;  // 1-indexed
		}
	}
	return 0;
}

void anim_group_add(uint32_t group_id, uint32_t anim_id)
{
	if (group_id == 0 || group_id > 8) return;
	anim_group_t *group = &groups[group_id - 1];
	if (!group->active || group->count >= 8) return;
	group->ids[group->count++] = anim_id;
}

void anim_group_start(uint32_t group_id)
{
	if (group_id == 0 || group_id > 8) return;
	anim_group_t *group = &groups[group_id - 1];
	if (!group->active) return;

	for (int i = 0; i < group->count; i++)
	{
		anim_start(group->ids[i]);
	}
}

void anim_group_stop(uint32_t group_id)
{
	if (group_id == 0 || group_id > 8) return;
	anim_group_t *group = &groups[group_id - 1];
	if (!group->active) return;

	for (int i = 0; i < group->count; i++)
	{
		anim_stop(group->ids[i]);
	}
}

int anim_group_is_finished(uint32_t group_id)
{
	if (group_id == 0 || group_id > 8) return 1;
	anim_group_t *group = &groups[group_id - 1];
	if (!group->active) return 1;

	for (int i = 0; i < group->count; i++)
	{
		if (!anim_is_finished(group->ids[i]))
		{
			return 0;
		}
	}
	return 1;
}

// Preset animations
uint32_t anim_fade_in(float *opacity, float duration)
{
	uint32_t id = anim_create_to(opacity, 1.0f, duration, EASE_OUT_QUAD);
	if (id) anim_start(id);
	return id;
}

uint32_t anim_fade_out(float *opacity, float duration)
{
	uint32_t id = anim_create_to(opacity, 0.0f, duration, EASE_IN_QUAD);
	if (id) anim_start(id);
	return id;
}

uint32_t anim_pulse(float *scale, float min_scale, float max_scale, float duration)
{
	uint32_t id = anim_create(min_scale, max_scale, duration / 2.0f, EASE_IN_OUT_QUAD);
	if (id)
	{
		anim_instance_t *anim = find_animation(id);
		if (anim)
		{
			anim->target_ptr = scale;
			anim->target_type = ANIM_TARGET_FLOAT;
			anim->ping_pong = 1;
			anim->loop = 1;
		}
		anim_start(id);
	}
	return id;
}

uint32_t anim_slide_in(float *position, float from, float to, float duration)
{
	uint32_t id = anim_create(from, to, duration, EASE_OUT_CUBIC);
	if (id)
	{
		anim_instance_t *anim = find_animation(id);
		if (anim)
		{
			anim->target_ptr = position;
			anim->target_type = ANIM_TARGET_FLOAT;
		}
		anim_start(id);
	}
	return id;
}

uint32_t anim_bounce(float *value, float target, float duration)
{
	uint32_t id = anim_create_to(value, target, duration, EASE_OUT_BOUNCE);
	if (id) anim_start(id);
	return id;
}

uint32_t anim_shake(float *value, float intensity, float duration)
{
	// Shake is handled by oscillating around original value
	// This is a simplified version
	float original = value ? *value : 0.0f;
	uint32_t id = anim_create(original - intensity, original + intensity, duration / 4.0f, EASE_IN_OUT_QUAD);
	if (id)
	{
		anim_instance_t *anim = find_animation(id);
		if (anim)
		{
			anim->target_ptr = value;
			anim->target_type = ANIM_TARGET_FLOAT;
			anim->ping_pong = 1;
		}
		anim_start(id);
	}
	return id;
}

void anim_debug_print(void)
{
	printf("=== Animation Debug ===\n");
	int count = 0;
	for (int i = 0; i < ANIM_MAX_ACTIVE; i++)
	{
		if (animations[i].active)
		{
			count++;
			printf("[%d] ID=%u State=%d Progress=%.2f Value=%.2f\n",
				i, animations[i].id, animations[i].state,
				anim_get_progress(animations[i].id),
				animations[i].current_value);
		}
	}
	printf("Active animations: %d\n", count);
	printf("=======================\n");
}

int anim_get_active_count(void)
{
	int count = 0;
	for (int i = 0; i < ANIM_MAX_ACTIVE; i++)
	{
		if (animations[i].active) count++;
	}
	return count;
}
