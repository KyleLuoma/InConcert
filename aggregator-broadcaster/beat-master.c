#include "beat-master.h"
#include <time.h>

uint32_t bs_L, bs_R, ms_per_beat;

void keep_rhythm(void *args) {
    struct global_t_args * global_t_arg = args;
    struct timeval last_beat, next_beat;

    global_t_arg->measure = 0;
    global_t_arg->beat = 0;
    bs_L = global_t_arg->beat_signature_L;
    bs_R = global_t_arg->beat_signature_R;

    suseconds_t last_beat_ms, next_beat_ms, current_time;

    gettimeofday(next_beat, NULL);
    next_beat_ms = next_beat.tv_usec / 1000;

    while(1) {
        //Update ms per beat
        gettimeofday(current_time, NULL);
        if(global_t_arg->current_tempo > 0) {
        ms_per_beat = 60000 / global_t_arg->current_tempo;
        } else {
            ms_per_beat = 60000 / 100;
        }
        if(current_time > next_beat_ms){
            next_beat_ms = current_time + ms_per_beat;
            global_t_arg->beat++;
        }
        if(global_t_arg->beat >= bs_R) {
            global_t_arg->beat = 1;
            global_t_arg->measure++;
        }
    }
}