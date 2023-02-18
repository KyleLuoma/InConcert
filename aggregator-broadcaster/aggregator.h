
#define PORT 54523
#define BUFFER_SIZE 1024

//Message types:
#define TEMPO 0
#define EVENT 1
#define TIME  2

//Message structs:
struct tempo_message {
    int message_type;
    int bpm;
    int confidence;
    long long timestamp;
};

struct event_message {
    int message_type;
    int event_type;
    long long timestamp;
    int num_params;
    int *params;
};

struct time {
    int message_type;
    long long timestamp;
};

void main();

static void * udp_listener(void *listener_arg);

struct udp_listener_t_arg {
    int *rcv_buffer;
    struct tempo_message *tempo_buffer;
    struct event_message *event_buffer;
};