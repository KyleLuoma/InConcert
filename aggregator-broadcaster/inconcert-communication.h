#include <stdint.h>

#define htons(x) ( ((x)<< 8 & 0xFF00) | \
                   ((x)>> 8 & 0x00FF) )

#define htonl(x) ( ((x)<<24 & 0xFF000000UL) | \
                   ((x)<< 8 & 0x00FF0000UL) | \
                   ((x)>> 8 & 0x0000FF00UL) | \
                   ((x)>>24 & 0x000000FFUL) )


#define SEND_PORT 54523

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
    uint32_t measure;
    uint32_t beat;
};

struct event_message {
    uint32_t message_type;
    uint32_t device_id;
    uint32_t event_type;
    uint32_t measure;
    uint32_t beat;
    uint32_t num_params;
    uint32_t *params;
};

struct time {
    uint32_t message_type;
    uint32_t device_id;
    uint32_t beat_signature_L;
    uint32_t beat_signature_R;
    uint32_t measure;
    uint32_t beat; //1 ... beat_signature_R values
};