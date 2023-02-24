#include "beat-master.h"
#include "aggregator.h"
#include <time.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>

uint32_t bs_L, bs_R;
double ms_per_beat;

void * keep_rhythm(void *args) {
    fprintf(stdout, "Initializing keep_rhythm\n");
    struct global_t_args * global_t_arg = args;
    struct timespec ts;
    clock_t beat_time, current_time;

    uint32_t loc_beat, loc_measure;
    uint64_t cur_sec, cur_ns, next_sec, next_ns, interval_ns;

    global_t_arg->measure = 0;
    global_t_arg->beat = 0;
    bs_L = global_t_arg->beat_signature_L;
    bs_R = global_t_arg->beat_signature_R;

    fprintf(stdout, "Calculating next beat ms\n");

    //Set to default 1 BPS until we get tempo data
    interval_ns = 1000000000U;

    clock_gettime(CLOCK_REALTIME, &ts);
    cur_sec = ts.tv_sec;
    cur_ns = ts.tv_nsec;

    if(cur_sec + interval_ns > 1000000000U){
        next_sec = cur_sec + 1;
        next_ns = (cur_sec + interval_ns) - 1000000000U;
    } else {
        next_ns = ts.tv_nsec + interval_ns;
    }
    
    next_sec = ts.tv_sec;
    
    
    fprintf(stdout, "Next beat will occur at %i ms\n", next_beat_ms);

    while(1) {
        //Update ms per beat
        clock_gettime(CLOCK_REALTIME, &ts);
        current_time = (uint64_t)ts.tv_sec * 1000000000U + (uint64_t)ts.tv_nsec;
        current_time_ms = ((double)current_time) / 1000000;

        if(global_t_arg->current_tempo > 30) {
            ms_per_beat = 60000 / (double)global_t_arg->current_tempo;
        } else {
            ms_per_beat = 60000 / 60;
        }

        if(current_time_ms > next_beat_ms){
            temp_beat_ms = next_beat_ms;
            next_beat_ms = next_beat_ms + ms_per_beat;
            last_beat_ms= temp_beat_ms;
            fprintf(
                stdout, "B: %i, clock: %f, beat_goal: %f, delta: %f, next: %f, interval: %f\n", 
                global_t_arg->beat, current_time_ms, last_beat_ms, (current_time_ms - last_beat_ms),
                next_beat_ms, ms_per_beat
            );
            global_t_arg->beat++;
        }

        if(global_t_arg->beat > bs_R) {
            global_t_arg->beat = 1;
            global_t_arg->measure++;
            fprintf(stdout, "M: %i\n", global_t_arg->measure);
        }
    }
}