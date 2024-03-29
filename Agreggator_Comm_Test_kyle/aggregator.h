
//#define PORT 54523
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
    int message_type;
    int device_id;
    int bpm;
    int confidence;
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
