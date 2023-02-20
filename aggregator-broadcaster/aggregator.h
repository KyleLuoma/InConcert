
#include <stdint.h>

#define PORT 54523
#define RECEIVE_BUFFER_SIZE 1024 //bytes
#define SEND_BUFFER_SIZE 1024 //bytes
#define EVENT_BUFFER_SIZE 16 //Number of events
#define TEMPO_BUFFER_SIZE 16 //Number of events

#define BROADCAST_ADDRESS "172.31.22.255"
#define BROADCAST_PORT 54555


//Message types:
#define TEMPO 0
#define EVENT 1
#define TIME  2


//Message structs:
struct tempo_message {
    uint32_t message_type;
    uint32_t device_id;
    uint32_t bpm;
    uint32_t confidence;
    long long timestamp;
};

struct event_message {
    int message_type;
    int device_id;
    int event_type;
    long long timestamp;
    int num_params;
    int *params;
};

struct time {
    int message_type;
    int device_id;
    long long timestamp;
};


struct global_t_args {
    struct tempo_message        *tempo_buffer;
    struct event_message        *event_buffer;
    struct shared_buffer_stats  *shared_buffer_stats;
    int current_tempo;
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