#include <stdio.h>

#include "aggregator.h"
#include "tempo_calculator.h"

void * tempo_calculator(void *arg) {
    struct udp_listener_t_arg * listener_arg = arg;
    struct tempo_message * tempo_buffer = listener_arg->tempo_buffer;
    struct shared_buffer_stats *stats = listener_arg->shared_buffer_stats;
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

        fprintf(stdout, "Updating tempo...\n");
        for(int i = 0; i <= stop_read; i++) {
            temp_msg = tempo_buffer[i];
            tempo_sum += temp_msg.bpm;
        }
        aggregate_tempo = tempo_sum / stop_read;
        tempo_sum = 0;
        fprintf(stdout, "Updated average tempo to: %i\n", aggregate_tempo);

        kill_time(5000000);
    }
}