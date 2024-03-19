/*
 * MIT License
 *
 * Copyright (c) 2024 <#author_name#>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <stdlib.h>
#include <string.h>
#include "hpt_lapsplit_chrono_face.h"
#include <stdio.h>
#include <inttypes.h>

// frequency rate of underlying timer (high precision timer)
#define LCF_SUBSECOND_RATE 1024

#define LCF_DISPLAY_UPDATE_RATE 16

static void render(hpt_lapsplit_chrono_state_t *context, bool lowEnergyUpdate)
{
    // show CR in the DOW index
    // DAY numerals show hours duration
    // show LAP if in lap mode, not if in split mode
    // rest is pretty obvious

    uint64_t runningTime;
    if (context->running == LCF_RUN_RUNNING)
    {
        // use "fast" get here because we don't need a truly accurate timestamp while the timer is running.
        runningTime = movement_hpt_get_fast() - context->startTs;
    }
    else
    {
        runningTime = context->pausedTs;
    }
    uint64_t showTime = context->display == LCF_DISPLAY_SPLIT ? context->splitTs : runningTime;

    uint8_t time_hundreths = (((uint16_t)(showTime % LCF_SUBSECOND_RATE)) * 100) / LCF_SUBSECOND_RATE;

    uint32_t d = showTime / LCF_SUBSECOND_RATE;
    uint8_t time_seconds = d % 60;
    d /= 60;
    uint8_t time_minutes = d % 60;
    d /= 60;
    uint8_t time_hours = d > 40 ? 40 : d;

    char buf[7];

    if (!lowEnergyUpdate)
    {
        sprintf(buf, "%02d%02d%02d", time_minutes, time_seconds, time_hundreths);
    }
    else
    {
        // since we only update once a minute in LE mode, only display the minutes
        sprintf(buf, "%02d--LE", time_minutes);
    }
    watch_display_string(buf, 4);

    // always show the colon if we're paused
    if (context->running == LCF_RUN_STOPPED)
    {
        watch_set_colon();
    }
    else
    {
        // otherwise, blink the colon once every second
        if ((runningTime % LCF_SUBSECOND_RATE) < (LCF_SUBSECOND_RATE / 2))
        {
            watch_set_colon();
        }
        else
        {
            watch_clear_colon();
        }
    }

    if (context->mode == LCF_MODE_LAP)
    {
        // display lap count in lap mode
        watch_set_indicator(WATCH_INDICATOR_LAP);
        sprintf(buf, "%2d", context->laps);
        watch_display_string(buf, 2);
    }
    else
    {
        // display hour count in date digits for as long as possible

        watch_clear_indicator(WATCH_INDICATOR_LAP);
        if (time_hours == 0)
        {
            watch_display_string("  ", 2);
        }
        else if (time_hours > 39)
        {
            // keep timing, but I guess show an error up here.
            watch_display_string(" E", 2);
        }
        else
        {
            sprintf(buf, "%2d", time_hours);
            watch_display_string(buf, 2);
        }
    }
}

static void splitButton(hpt_lapsplit_chrono_state_t *state)
{
    if (state->display == LCF_DISPLAY_SPLIT)
    {
        // if the split duration is being displayed, clear it when you press "light" again, but don't change anything else
        state->display = LCF_DISPLAY_TIME;
        return;
    }

    if (state->running == LCF_RUN_STOPPED)
    {
        // If the timer is paused, but it is showing a non-zero time, reset the time back to zero
        if (state->pausedTs != 0 || state->laps != 0)
        {
            state->pausedTs = 0;
            state->laps = 0;
        }
        else
        {
            // if already reset to zero, toggle lap/split mode
            state->mode = state->mode == LCF_MODE_LAP ? LCF_MODE_SPLIT : LCF_MODE_LAP;
        }
    }
    else
    {
        // record split duration
        uint64_t now = movement_hpt_get();
        state->splitTs = now - state->startTs;

        // display split instead of current time
        state->display = LCF_DISPLAY_SPLIT;

        if (state->mode == LCF_MODE_LAP)
        {
            // reset start time to current timestamp to start a new lap
            state->startTs = now;
            if (state->laps == 39)
            {
                state->laps = 0;
            }
            else
            {
                state->laps = state->laps + 1;
            }
        }
    }
}

static void startStopButton(hpt_lapsplit_chrono_state_t *state)
{
    if (state->running == LCF_RUN_RUNNING)
    {
        // if running, stop the timer and record its duration
        uint64_t now = movement_hpt_get();
        state->running = LCF_RUN_STOPPED;
        state->pausedTs = now - state->startTs;
        movement_hpt_release();

        // slow display back down because the time is paused
        movement_request_tick_frequency(1);
    }
    else
    {
        // restart the timer
        movement_hpt_request();
        uint64_t now = movement_hpt_get();
        state->running = LCF_RUN_RUNNING;
        state->startTs = now - state->pausedTs;

        // increase display rate so it looks like the timer is running
        movement_request_tick_frequency(LCF_DISPLAY_UPDATE_RATE);
    }
}

void hpt_lapsplit_chrono_face_setup(movement_settings_t *settings, uint8_t watch_face_index, void **context_ptr)
{
    (void)settings;
    (void)watch_face_index;
    if (*context_ptr == NULL)
    {
        *context_ptr = malloc(sizeof(hpt_lapsplit_chrono_state_t));
        memset(*context_ptr, 0, sizeof(hpt_lapsplit_chrono_state_t));
    }
}

void hpt_lapsplit_chrono_face_activate(movement_settings_t *settings, void *context)
{
    (void)settings;
    hpt_lapsplit_chrono_state_t *state = (hpt_lapsplit_chrono_state_t *)context;
    // always show the running time when switching to this face
    state->display = LCF_DISPLAY_TIME;

    // if the timer is running, show a higher update rate
    if(state->running == LCF_RUN_RUNNING) {
        movement_request_tick_frequency(LCF_DISPLAY_UPDATE_RATE);
    } else {
        movement_request_tick_frequency(1);
    }

    watch_display_string("CH", 0);
}

bool hpt_lapsplit_chrono_face_loop(movement_event_t event, movement_settings_t *settings, void *context)
{
    hpt_lapsplit_chrono_state_t *state = (hpt_lapsplit_chrono_state_t *)context;

    switch (event.event_type)
    {
    case EVENT_LIGHT_BUTTON_DOWN:
        splitButton(state);
        render(state, false);
        break;
    // swallow the long press to avoid toggling light settings here in a confusing way
    case EVENT_LIGHT_LONG_PRESS:
        break;
    case EVENT_ALARM_BUTTON_DOWN:
        startStopButton(state);
        render(state, false);
        break;
    case EVENT_LOW_ENERGY_UPDATE:
        render(state, true);
        break;
    case EVENT_ACTIVATE:
    case EVENT_TICK:
        render(state, false);
        break;
    case EVENT_TIMEOUT:
        // only timeout if the chrono is not running
        if (state->running == LCF_RUN_STOPPED)
        {
            movement_move_to_face(0);
        }
        break;
    default:
        // Movement's default loop handler will step in for any cases you don't handle above:
        // * EVENT_LIGHT_BUTTON_DOWN lights the LED
        // * EVENT_MODE_BUTTON_UP moves to the next watch face in the list
        // * EVENT_MODE_LONG_PRESS returns to the first watch face (or skips to the secondary watch face, if configured)
        // You can override any of these behaviors by adding a case for these events to this switch statement.
        return movement_default_loop_handler(event, settings);
    }

    // return true if the watch can enter standby mode. Generally speaking, you should always return true.
    // Exceptions:
    //  * If you are displaying a color using the low-level watch_set_led_color function, you should return false.
    //  * If you are sounding the buzzer using the low-level watch_set_buzzer_on function, you should return false.
    // Note that if you are driving the LED or buzzer using Movement functions like movement_illuminate_led or
    // movement_play_alarm, you can still return true. This guidance only applies to the low-level watch_ functions.
    return true;
}

void hpt_lapsplit_chrono_face_resign(movement_settings_t *settings, void *context)
{
    (void)settings;
    (void)context;    

    // handle any cleanup before your watch face goes off-screen.
    // reset tick frequency
    movement_request_tick_frequency(1);
}