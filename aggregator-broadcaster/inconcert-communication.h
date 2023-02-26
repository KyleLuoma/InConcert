#include <stdint.h>

// #define INCLUDE_HTONX

#ifdef INCLUDE_HTONX
#define htons(x) ( ((x)<< 8 & 0xFF00) | \
                   ((x)>> 8 & 0x00FF) )

#define htonl(x) ( ((x)<<24 & 0xFF000000UL) | \
                   ((x)<< 8 & 0x00FF0000UL) | \
                   ((x)>> 8 & 0x0000FF00UL) | \
                   ((x)>>24 & 0x000000FFUL) )
#endif


#define SEND_PORT 54523
#define BROADCAST_PORT 54555
#define BROADCAST_IP "10.42.0.255"
// #define BROADCAST_IP "255.255.255.255"

//Message types:
#define TEMPO           0
#define EVENT           1
#define TIME            2
#define REGISTER_CLIENT 3

//Message structs:

struct tempo_message {
    uint32_t message_type;
    uint32_t device_id;
    uint32_t bpm;
    uint32_t confidence;
    uint32_t measure;
    uint32_t beat;
};

struct event_message {
    uint32_t message_type;
    uint32_t device_id;
    uint32_t event_type;
    uint32_t measure;
    uint32_t beat;
    uint32_t num_used_params; //1-10
    uint32_t params[10];
};

struct time_message {
    uint32_t message_type;
    uint32_t device_id;
    uint32_t beat_signature_L;
    uint32_t beat_signature_R;
    uint32_t measure;
    uint32_t beat; //1 ... beat_signature_R values
    uint32_t beat_interval; //ms between beats
};

struct register_client_message {
    uint32_t message_type;
    uint32_t device_id;
};