#include <stdio.h>

#include "aggregator.h"
#include "tempo_calculator.h"

#define VERBOSE

void * tempo_calculator(void *arg) {
    struct global_t_args * global_t_arg = arg;
    struct tempo_message * tempo_buffer = global_t_arg->tempo_buffer;
    struct shared_buffer_stats *stats = global_t_arg->shared_buffer_stats;
    struct tempo_message temp_msg;

    int aggregate_tempo = DEFAULT_TEMPO_START;
    int tempo_sum = 0;
    int last_write = stats->tempo_buffer_last_write_ix;
    int current_write, stop_read, last_rollover;

    while(1) {
        //check if more writes since last write

        if(last_rollover == 0) {
            stop_read = stats->tempo_buffer_last_write_ix;
        } else {
            stop_read = TEMPO_BUFFER_SIZE;
        }
#ifdef VERBOSE
        fprintf(stdout, "Updating tempo...\n");
#endif
        for(int i = 0; i <= stop_read; i++) {
            temp_msg = tempo_buffer[i];
            tempo_sum += temp_msg.bpm;
        }
        aggregate_tempo = tempo_sum / (stop_read + 1);
        tempo_sum = 0;
#ifdef VERBOSE
        fprintf(stdout, "Updated average tempo to: %i\n", aggregate_tempo);
#endif
        global_t_arg->current_tempo = aggregate_tempo;
        kill_time(5000000);
    }
}