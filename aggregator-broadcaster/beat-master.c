#include "beat-master.h"
#include "aggregator.h"
#include <time.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>

uint32_t bs_L, bs_R, ms_per_beat;

void * keep_rhythm(void *args) {
    fprintf(stdout, "Initializing keep_rhythm\n");
    struct global_t_args * global_t_arg = args;
    clock_t beat_time, current_time;
    double next_beat_ms, current_time_ms;
    uint32_t loc_beat, loc_measure; 

    global_t_arg->measure = 0;
    global_t_arg->beat = 0;
    bs_L = global_t_arg->beat_signature_L;
    bs_R = global_t_arg->beat_signature_R;

    fprintf(stdout, "Calculating next beat ms\n");
    beat_time = clock();
    next_beat_ms = (double)beat_time / CLOCKS_PER_SEC * 1000;
    fprintf(stdout, "Next beat will occur at %i ms\n", next_beat_ms);

    while(1) {
        //Update ms per beat
        current_time = clock();
        current_time_ms = (double)current_time / CLOCKS_PER_SEC * 1000;

        if(global_t_arg->current_tempo > 30) {
            ms_per_beat = 60000 / global_t_arg->current_tempo;
        } else {
            ms_per_beat = 60000 / 50;
        }
        if(current_time_ms > next_beat_ms){
            next_beat_ms = current_time_ms + ms_per_beat;
            // fprintf(stdout, "B: %i\n", global_t_arg->beat);
            global_t_arg->beat++;
        }

        if(global_t_arg->beat > bs_R) {
            global_t_arg->beat = 1;
            global_t_arg->measure++;
            // fprintf(stdout, "M: %i\n", global_t_arg->measure);
        }
    }
}