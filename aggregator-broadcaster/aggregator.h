
#include <stdint.h>
#include "inconcert-communication.h"

#define PORT 54523
#define RECEIVE_BUFFER_SIZE 1024 //bytes
#define SEND_BUFFER_SIZE 1024 //bytes
#define EVENT_BUFFER_SIZE 16 //Number of events
#define TEMPO_BUFFER_SIZE 16 //Number of events

#define BROADCAST_PORT 54555

#define MAX_CLIENTS 20


struct global_t_args {
    struct tempo_message        *tempo_buffer;
    struct event_message        *event_buffer;
    struct shared_buffer_stats  *shared_buffer_stats;
    uint32_t current_tempo;
    uint32_t beat_signature_L;
    uint32_t beat_signature_R;
    uint32_t measure;
    uint32_t beat;
    uint32_t beat_interval;
    uint32_t clients[MAX_CLIENTS];
    int num_clients;
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

//Initializes buffer stats to 0 values
//Args: void *buffer_stats - pointer to struct shared_buffer_stats
void init_buffer_stats(void *buffer_stats);

// Writes a tempo message to the tempo buffer and updates
// buffer stats in global_t_args
int write_to_tempo_buffer(
    void *tempo_buffer,
    struct tempo_message message,
    void *buffer_stats
    );

// Writes an event message to the event buffer and updates
// buffer stats in global_t_args
int write_to_event_buffer(
    void *event_buffer,
    struct event_message message,
    void *buffer_stats
);

// Monitors inbound UDP socket and calls buffer
// write functions to store messages
static void * udp_listener(void *listener_arg);

// Monitors global_t_args and broadcasts event, 
// tempo, and time messages based on state changes
static void * udp_broadcaster(void *arg);

// Debug function to observe changes to buffer
// states and print observations to console
void * buffer_watcher(void *arg);

// Simple delay function that uses clock()
void kill_time(int time_ms);