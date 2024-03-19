#ifndef WATCH_HPT_H__
#define WATCH_HPT_H__

#include <inttypes.h>
#include <stdbool.h>

/**
 * watch_hpt provides low-level access to the high-precision timer.
 * 
 * These methods are not intended to be used by watch faces. See the 
 * "movement_hpt_*" faces in movement.h instead.
*/

/**
 * Describes the reasons the HPT callback is being invoked. More than one flag may be set.
*/
typedef struct {

    /**
     * The callback is being invoked because the count is greater than or equal to to the scheduled timestamp
     */
    bool compare_match :1;

    /**
     * The callback is being invoked because the counter overflowed and reset to zero.
     */
    bool overflow :1;

    // not used
    uint8_t _padding :6;
} HPT_CALLBACK_CAUSE;



/**
 * Performs one-time setup of the peripherals used by the high-precision timer.
 * 
 * Does not start the timer.
 * 
 * @param callback_function an interrupt handler that will be invoked when the timer hits a scheduled timestamp or overflows.
*/
void watch_hpt_init(void (*callback_function)(HPT_CALLBACK_CAUSE cause));

/**
 * Enables and starts the high-precision timer. The timestamp *may* be reset to zero if the timer was not already running.
 */
void watch_hpt_enable(void);

/**
 * Stops the high-precision timer and powers it down.
 */
void watch_hpt_disable(void);

/**
 * Returns the current counter value of the high-precision timer.
 */
uint32_t watch_hpt_get(void);

/**
 * Returns the current timestamp of the high-precision timer, without synchronization.
 * 
 * The timestamp returned by this method is not suitable for scheduling purposes or other complex logic, but it may be good enough for non-critical purposes, such as showing the current time of a running stopwatch.
*/
uint32_t watch_hpt_get_fast(void);

/**
 * Sets the timestamp at which the previously registered callback should be invoked. Note that this will be called every time the counter value reaches this value, including after an overflow occurs. 
*/
void watch_hpt_schedule_callback(uint32_t timestamp);

/**
 * Disables any previously scheduled callback.
*/
void watch_hpt_disable_scheduled_callback(void);

// TC2 Interrupt Handler (internal)
void TC2_Handler(void);

#endif