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
    int default_bpm = 120;
    interval_ns = 60000000000U / default_bpm;

    clock_gettime(CLOCK_REALTIME, &ts);
    cur_sec = ts.tv_sec;
    cur_ns = ts.tv_nsec;

    if(cur_ns + interval_ns > 1000000000U){
        next_sec = cur_sec + 1;
        next_ns = (cur_ns + interval_ns) - 1000000000U;
    } else {
        next_sec = cur_sec;
        next_ns = ts.tv_nsec + interval_ns;
    }  
    
    fprintf(stdout, "Next beat will occur at %i ns\n", next_ns);

    while(1) {
        //Update ms per beat
        while(cur_sec < next_sec || cur_ns < next_ns) {
            clock_gettime(CLOCK_REALTIME, &ts);
            cur_sec = ts.tv_sec;
            cur_ns = ts.tv_nsec;
        }

        if(cur_ns + interval_ns > 1000000000U){
            next_sec = cur_sec + 1;
            next_ns = (next_ns + interval_ns) - 1000000000U;
        } else {
            next_sec = cur_sec;
            next_ns = next_ns + interval_ns;
        }  

        if(global_t_arg->beat >= bs_R) {
            global_t_arg->beat = 1;
            global_t_arg->measure++;
            fprintf(stdout, "M: %i\n", global_t_arg->measure);
        } else {
            global_t_arg->beat++;
        }
        fprintf(stdout, "B: %i\n", global_t_arg->beat);
        
        if(global_t_arg->current_tempo > 30) {
            interval_ns = 60000000000U / global_t_arg->current_tempo;
        } else {
            interval_ns = 60000000000U / default_bpm;
        }
    }
}