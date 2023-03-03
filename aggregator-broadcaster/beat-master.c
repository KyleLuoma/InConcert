#include "beat-master.h"
#include "aggregator.h"
#include <time.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>

// #define VERBOSE

uint32_t bs_L, bs_R;
double ms_per_beat;

void * keep_rhythm(void *args) {
    fprintf(stdout, "Initializing keep_rhythm\n");
    struct global_t_args * global_t_arg = args;
    struct timespec ts;
    clock_t beat_time, current_time;

    uint32_t loc_beat, loc_measure;
    uint64_t cur_sec, cur_ns, next_sec, next_ns, interval_ns, interval_ns_backup;

    global_t_arg->measure = 0;
    global_t_arg->beat = 0;
    bs_L = global_t_arg->beat_signature_L;
    bs_R = global_t_arg->beat_signature_R;

    fprintf(stdout, "Calculating next beat ms\n");

    //Set to default 1 BPS until we get tempo data
    int default_bpm = DEFAULT_BPM;
    interval_ns = 60000000000U / default_bpm;
    global_t_arg->beat_interval = interval_ns / 100000;

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
    
    uint64_t last_sec_debug = 0;

    while(1) {
        //Update ms per beat
        while(cur_sec < next_sec || cur_ns < next_ns) {
            clock_gettime(CLOCK_REALTIME, &ts);
#ifdef VERBOSE
            if(last_sec_debug != ts.tv_sec) {
                // fprintf(stdout, "%u, %llu, %u, %llu, %llu\n", cur_sec, next_sec, cur_ns, next_ns, interval_ns);
                last_sec_debug = ts.tv_sec;
            }
#endif
            cur_sec = ts.tv_sec;
            cur_ns = ts.tv_nsec;
        }

        if(next_ns + interval_ns < 1000000000U) {
            next_sec = cur_sec;
            next_ns += interval_ns;
        } else {
            next_sec = cur_sec;
            while(next_ns + interval_ns > 1000000000U){
                next_sec++;
                next_ns -= 1000000000U;
            } 
            next_ns = next_ns + interval_ns;
        }
         

        if(global_t_arg->beat >= bs_R) {
            global_t_arg->beat = 1;
            global_t_arg->measure++;
#ifdef VERBOSE
            fprintf(stdout, "M: %i\n", global_t_arg->measure);
#endif
        } else {
            global_t_arg->beat++;
        }
#ifdef VERBOSE
        fprintf(stdout, "B: %i\n", global_t_arg->beat);
#endif
        
        interval_ns_backup = interval_ns;
        if(global_t_arg->current_tempo > 10) {
            interval_ns = 60000000000U / global_t_arg->current_tempo;
        } else {
            interval_ns = 60000000000U / default_bpm;
        }
        if(interval_ns > 60000000000U) { //Checking for error and recover to saved value
            interval_ns = interval_ns_backup;
        }
        global_t_arg->beat_interval = interval_ns / 1000000;

    }
}