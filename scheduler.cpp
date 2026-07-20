#include "scheduler.h"
#include <stdio.h>
#include <sys/time.h>
#include "libco.h"
#include "menu.h"
#include "user_io.h"
#include "input.h"
#include "frame_timer.h"
#include "fpga_io.h"
#include "osd.h"
#include "profiling.h"
#include "video.h"
#include "gfx_menu.h"
#include "animator.h"

static cothread_t co_scheduler = nullptr;
static cothread_t co_poll = nullptr;
static cothread_t co_ui = nullptr;
static cothread_t co_last = nullptr;

static void scheduler_wait_fpga_ready(void)
{
	while (!is_fpga_ready(1))
	{
		fpga_wait_to_reset();
	}
}

static void scheduler_co_poll(void)
{
	for (;;)
	{
		scheduler_wait_fpga_ready();

		{
			SPIKE_SCOPE("co_poll", 1000);
			user_io_poll();
			frame_timer();
			input_poll(0);
			video_poll();
		}

		scheduler_yield();
	}
}

static void scheduler_co_ui(void)
{
	// Frame timing for animations (gfx menu + animator)
	struct timeval last_frame_time, current_frame_time;
	gettimeofday(&last_frame_time, NULL);

	for (;;)
	{
		{
			SPIKE_SCOPE("co_ui", 1000);

			// Delta time for animations
			gettimeofday(&current_frame_time, NULL);
			float delta_time = (current_frame_time.tv_sec - last_frame_time.tv_sec) +
			                   (current_frame_time.tv_usec - last_frame_time.tv_usec) / 1000000.0f;
			last_frame_time = current_frame_time;

			anim_update(delta_time);
			gfx_menu_update_animations(delta_time);

			HandleUI();

			// Render graphical menu if enabled (scheduler build path)
			if (gfx_menu_is_enabled())
			{
				extern void gfx_menu_sync_from_classic(void);
				gfx_menu_sync_from_classic();
				gfx_menu_render();
			}

			OsdUpdate();
		}

		scheduler_yield();
	}
}

static void scheduler_schedule(void)
{
	if (co_last == co_poll)
	{
		co_last = co_ui;
		co_switch(co_ui);
	}
	else
	{
		co_last = co_poll;
		co_switch(co_poll);
	}
}

void scheduler_init(void)
{
	const unsigned int co_stack_size = 262144 * sizeof(void*);

	co_poll = co_create(co_stack_size, scheduler_co_poll);
	co_ui = co_create(co_stack_size, scheduler_co_ui);
}

void scheduler_run(void)
{
	co_scheduler = co_active();

	for (;;)
	{
		scheduler_schedule();
	}

	co_delete(co_ui);
	co_delete(co_poll);
	co_delete(co_scheduler);
}

void scheduler_yield(void)
{
	co_switch(co_scheduler);
}
