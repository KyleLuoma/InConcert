
#include <stdint.h>
#include "inconcert-communication.h"

#define PORT 54523
#define RECEIVE_BUFFER_SIZE 1024 //bytes
#define SEND_BUFFER_SIZE 1024 //bytes
#define EVENT_BUFFER_SIZE 16 //Number of events
#define TEMPO_BUFFER_SIZE 16 //Number of events

#define BROADCAST_ADDRESS "172.31.22.255"
#define BROADCAST_PORT 54555


struct global_t_args {
    struct tempo_message        *tempo_buffer;
    struct event_message        *event_buffer;
    struct shared_buffer_stats  *shared_buffer_stats;
    uint32_t current_tempo;
    uint32_t beat_signature_L;
    uint32_t beat_signature_R;
    uint32_t measure;
    uint32_t beat;
};

struct shared_buffer_stats {
    int tempo_buffer_last_write_ix;
    int tempo_buffer_last_read_ix;
    int tempo_buffer_locked;
    int tempo_buffer_rollovers;

    int event_buffer_last_write_ix;
    int event_buffer_last_read_ix;
    int event_buffer_locked;
    int event_buffer_rollovers;
};


void main();
void init_buffer_stats(void *buffer_stats);
int write_to_tempo_buffer(
    void *tempo_buffer,
    struct tempo_message message,
    void *buffer_stats
    );

static void * udp_listener(void *listener_arg);
static void * udp_broadcaster(void *arg);
void * buffer_watcher(void *arg);

void kill_time(int time_ms);