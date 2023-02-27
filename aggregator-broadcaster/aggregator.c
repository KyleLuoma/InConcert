
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <time.h>
#include <sched.h>

#include "tempo_calculator.h"
#include "beat-master.h"
#include "aggregator.h"


void main() {
    fprintf(stdout, "\n ------- InConcert Aggregator Broadcaster ------- \n ");

    pthread_t udp_listener_thread, udp_broadcaster_thread, 
              tempo_calculator_thread, event_manager_thread, 
              screen_display_thread, buffer_watcher_thread, 
              keep_rhythm_thread;

    int udp_listener_t_ret, udp_broadcaster_t_ret,
        tempo_calculator_t_ret, event_manager_t_ret,
        screen_display_t_ret, buffer_watcher_t_ret,
        keep_rhythm_t_ret;

    
    int *send_buffer = malloc(SEND_BUFFER_SIZE);
    struct tempo_message *tempo_buffer = malloc(TEMPO_BUFFER_SIZE * sizeof(struct tempo_message));
    struct event_message *event_buffer = malloc(EVENT_BUFFER_SIZE * sizeof(struct event_message));

    struct shared_buffer_stats buffer_stats;
    
    init_buffer_stats(&buffer_stats);

    struct global_t_args global_t_arg;
    global_t_arg.tempo_buffer = tempo_buffer;
    global_t_arg.event_buffer = event_buffer;
    global_t_arg.shared_buffer_stats = &buffer_stats;
    global_t_arg.measure = 0;
    global_t_arg.beat = 0;
    global_t_arg.beat_signature_L = 4;
    global_t_arg.beat_signature_R = 4;

    udp_listener_t_ret = pthread_create(&udp_listener_thread, NULL, &udp_listener, &global_t_arg);
    buffer_watcher_t_ret = pthread_create(&buffer_watcher_thread, NULL, &buffer_watcher, &global_t_arg);
    tempo_calculator_t_ret = pthread_create(&tempo_calculator_thread, NULL, &tempo_calculator, &global_t_arg);
    udp_broadcaster_t_ret = pthread_create(&udp_broadcaster_thread, NULL, &udp_broadcaster, &global_t_arg);

    pthread_attr_t keep_rhythm_t_attr;
    struct sched_param kr_param;
    keep_rhythm_t_ret = pthread_attr_init(&keep_rhythm_t_attr);
    keep_rhythm_t_ret = pthread_attr_getschedparam(&keep_rhythm_t_attr, &kr_param);
    kr_param.sched_priority = sched_get_priority_max(SCHED_FIFO);
    keep_rhythm_t_ret = pthread_attr_setschedparam (&keep_rhythm_t_attr, &kr_param);
    keep_rhythm_t_ret = pthread_create(&keep_rhythm_thread, NULL, &keep_rhythm, &global_t_arg);

    while(1) {};

    free(send_buffer);
    free(tempo_buffer);
    free(event_buffer);
}

void init_buffer_stats(void *buffer_stats) {
    struct shared_buffer_stats *stats = buffer_stats; 
    stats->tempo_buffer_last_write_ix = 0;
    stats->tempo_buffer_last_read_ix = 0;
    stats->tempo_buffer_locked = 0;
    stats->tempo_buffer_rollovers = 0;
    stats->event_buffer_last_write_ix = 0;
    stats->event_buffer_last_read_ix = 0;
    stats->event_buffer_locked = 0;
    stats->event_buffer_rollovers = 0;
}

//Takes in the same argument as the udp_listener and monitors stats to determine
//if an update occurred to buffers
void * buffer_watcher(void *arg) {

    struct global_t_args *args = arg;
    int tb_last_write = args->shared_buffer_stats->tempo_buffer_last_write_ix;
    int tb_last_rollover = args->shared_buffer_stats->tempo_buffer_rollovers;

    while(1) {
        if(tb_last_write < args->shared_buffer_stats->tempo_buffer_last_write_ix) {
            fprintf(stdout, "buffer_watcher detected a tempo buffer update with payload:\n");
            struct tempo_message msg = args->tempo_buffer[args->shared_buffer_stats->tempo_buffer_last_write_ix];
            fprintf(stdout, "     message_type: %i, device_id: %i bpm: %i, confidence: %i, measure: %lld\n", 
                msg.message_type, msg.device_id, msg.bpm, msg.confidence, msg.measure
            );
            tb_last_write = args->shared_buffer_stats->tempo_buffer_last_write_ix;
        }
        if(tb_last_rollover < args->shared_buffer_stats->tempo_buffer_rollovers) {
            tb_last_rollover = args->shared_buffer_stats->tempo_buffer_rollovers;
            tb_last_write = args->shared_buffer_stats->tempo_buffer_last_write_ix;
        }
    }
}

static void * udp_broadcaster(void *arg) {

    struct global_t_args *global_t_arg = arg;
    struct shared_buffer_stats *buffer_stats = global_t_arg->shared_buffer_stats;
    
    int last_tempo_change = 0;
    int last_event_rollover = 0;
    int sockfd, optval, n, last_event_read, last_event_write, num_events;

    struct sockaddr_in serveraddr;
    struct sockaddr_in broadcastaddr;

    struct tempo_message *tempo_send_buffer = malloc(1);
    struct tempo_message tempo_broadcast_message;

    struct time_message *time_send_buffer = malloc(1);
    struct time_message time_broadcast_message;

    struct event_message *event_send_buffer = malloc(1);
    struct event_message event_broadcast_message;

    time_broadcast_message.message_type = TIME;
    time_broadcast_message.device_id = 0;
    time_broadcast_message.beat_signature_L = 4;
    time_broadcast_message.beat_signature_R = 4;

    broadcastaddr.sin_family = AF_INET;
    broadcastaddr.sin_port = htons((unsigned short)BROADCAST_PORT);
    // broadcastaddr.sin_addr.s_addr = INADDR_BROADCAST;
    broadcastaddr.sin_addr.s_addr = inet_addr(BROADCAST_IP);
    // inet_aton(BROADCAST_ADDRESS, &broadcastaddr.sin_addr);
    
    fprintf(stdout, "Broadcast address set to: ");
    fprintf(stdout, inet_ntoa(broadcastaddr.sin_addr));
    fprintf(stdout, "\n");

    sockfd = socket(AF_INET, SOCK_DGRAM, 0);

    if(sockfd < 0) {
        fprintf(stdout, "Broadcaster socket init failure.\n");
    }

    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (const void *)&optval, sizeof(int));
    int broadcastEnable=1;
    setsockopt(sockfd, SOL_SOCKET, SO_BROADCAST, &broadcastEnable, sizeof(broadcastEnable));


    bzero((char *)&serveraddr, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
    serveraddr.sin_port = htons((unsigned short)BROADCAST_PORT);

    if(bind(sockfd, (struct sockaddr *)&serveraddr, sizeof(serveraddr)) < 0) {
        fprintf(stdout, "Broadcaster: error binding to socket \n");
    }

    uint32_t last_beat = 0;
    uint32_t last_measure = 0;
    
    while(1) {

        //handle broadcasts based on state changes

        //First check for a new beat and broadcast (highest priority)
        if(last_beat != global_t_arg->beat){

            time_broadcast_message.measure = global_t_arg->measure;
            time_broadcast_message.beat = global_t_arg->beat;
            time_broadcast_message.beat_interval = global_t_arg->beat_interval;

            time_send_buffer[0] = time_broadcast_message;
            //Send over broadcast channel:
            n = sendto(
                sockfd, time_send_buffer, sizeof(struct time_message), 0, 
                (struct sockaddr *)&broadcastaddr, sizeof(struct sockaddr_in)
            );
            if(n > 0){
                // fprintf(stdout, 
                //         "Time broadcast complete for measure %i, beat %i, chars sent: %i\n", 
                //         time_broadcast_message.measure, time_broadcast_message.beat, n
                //         );
            } else {
                fprintf(stdout, "Unable to send time message %i %i, failure with code %i\n",
                        time_broadcast_message.measure, time_broadcast_message.beat, n
                );
            }
            //Send to clients in client list:
            // for(int i = 0; i < global_t_arg->num_clients; i++){
            //     broadcastaddr.sin_addr.s_addr = global_t_arg->clients[i];
            //     n = sendto(
            //         sockfd, time_send_buffer, sizeof(struct time_message), 0,
            //         (struct sockaddr *)&broadcastaddr, sizeof(struct sockaddr_in)
            //     );
            // }
            //Set broadcast address back to network broadcast IP:
            broadcastaddr.sin_addr.s_addr = inet_addr(BROADCAST_IP);
            last_beat = global_t_arg->beat;
        }
        //Next check for events in queue
        else if(global_t_arg->shared_buffer_stats->event_buffer_last_write_ix 
                != global_t_arg->shared_buffer_stats->event_buffer_last_read_ix) {

            last_event_read = global_t_arg->shared_buffer_stats->event_buffer_last_read_ix;
            last_event_write = global_t_arg->shared_buffer_stats->event_buffer_last_write_ix;

            if(global_t_arg->shared_buffer_stats->event_buffer_rollovers > last_event_rollover){
                num_events = (EVENT_BUFFER_SIZE - last_event_read) + last_event_write;
            } else {
                num_events = last_event_write - last_event_read;
            }
            int event_ix = last_event_read + 1;
            int event_buffer_loc;
            for(int i = 0; i < num_events; i++){
                if(event_ix + i < EVENT_BUFFER_SIZE) {
                    event_buffer_loc = event_ix;
                } else {
                    event_buffer_loc = (event_ix + i) - EVENT_BUFFER_SIZE;
                }
                event_broadcast_message = global_t_arg->event_buffer[event_buffer_loc];
                event_send_buffer[0] = event_broadcast_message;
                n = sendto(
                sockfd, event_send_buffer, sizeof(struct event_message), 0, 
                (struct sockaddr *)&broadcastaddr, sizeof(struct sockaddr_in)
                );
                if(n > 0){
                    fprintf(stdout, 
                            "Event broadcast of size %i forwarded from device %i\n", 
                            n, event_broadcast_message.device_id
                            );
                } else {
                    fprintf(stdout, "Unable to send event message from device %i, failure with code %i\n",
                           event_broadcast_message.device_id, n
                    );
                }
            }
            global_t_arg->shared_buffer_stats->event_buffer_last_read_ix = last_event_write;
            last_event_rollover = global_t_arg->shared_buffer_stats->event_buffer_rollovers;
        }  
        //Then check for tempo change (should be lower priority)       
        else if(last_tempo_change != global_t_arg->current_tempo) {

            tempo_broadcast_message.message_type = TEMPO;
            tempo_broadcast_message.device_id = 0;
            tempo_broadcast_message.bpm = global_t_arg->current_tempo;
            tempo_broadcast_message.confidence = 100;
            tempo_broadcast_message.measure = global_t_arg->measure;
            tempo_broadcast_message.beat = global_t_arg->beat;

            fprintf(stdout, "Broadcasting updated tempo %i\n", tempo_broadcast_message.bpm);

            tempo_send_buffer[0] = tempo_broadcast_message;
            
            n = sendto(
                sockfd, tempo_send_buffer, sizeof(struct tempo_message), 0, 
                (struct sockaddr *)&broadcastaddr, sizeof(struct sockaddr_in)
            );

            if(n < 0) {
                fprintf(stdout, "Tempo broadcast failed with code %i\n", n);
            }

            last_tempo_change = global_t_arg->current_tempo;
        }
    }
}


static void * udp_listener(void *arg) {

    int *receive_buffer = malloc(RECEIVE_BUFFER_SIZE);

    struct global_t_args *global_t_arg = arg;
    struct shared_buffer_stats *buffer_stats = global_t_arg->shared_buffer_stats;

    fprintf(stdout, "\n UDP listener running \n");
    int sockfd;
    int portno;
    int clientlen;
    struct sockaddr_in serveraddr;
    struct sockaddr_in clientaddr;
    struct hostent *hostp;
    char *buf;
    char *hostaddrp;
    int optval;
    int n, i;

    int msg_type;
    int client_address;

    struct tempo_message t_msg;
    struct event_message e_msg;
    

    sockfd = socket(AF_INET, SOCK_DGRAM, 0);

    if(sockfd < 0) {
        fprintf(stdout, "sockfd initialization error \n");
    }

    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (const void *)&optval, sizeof(int));

    bzero((char *)&serveraddr, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
    serveraddr.sin_port = htons((unsigned short)PORT);

    if(bind(sockfd, (struct sockaddr *)&serveraddr, sizeof(serveraddr)) < 0) {
        fprintf(stdout, "UDP Listener: error binding to socket \n");
    }

    while(1) {

        msg_type = -1;

        n = recvfrom(sockfd, receive_buffer, RECEIVE_BUFFER_SIZE, 0, (struct sockaddr *)&clientaddr, &clientlen);
        if(n < 0) {
            fprintf(stdout, "\nrecvfrom error \n");
        } else {
            fprintf(stdout, "\nReceived UDP packet \n");
        }
        client_address = clientaddr.sin_addr.s_addr;
        hostp = gethostbyaddr(
            (const char *)&clientaddr.sin_addr.s_addr, 
            sizeof(clientaddr.sin_addr.s_addr), 
            AF_INET
        );
        if(hostaddrp == NULL) {
            fprintf(stdout, "\nError: null host address\n");
        }
        hostaddrp = inet_ntoa(clientaddr.sin_addr);
        if(hostaddrp == NULL) {
            //do error stuff
        } else {
            fprintf(stdout, hostaddrp);

            //How to read from buffer:
            //htonl(*(global_t_arg->rcv_buffer + i))

            msg_type = htonl(*(receive_buffer));

            fprintf(stdout, "\nMessage type: %i\n", msg_type);
        }

        //handle message receipts by type:

        if(msg_type == REGISTER_CLIENT) {
            int address_in_client_list = 0;
            struct sockaddr_in temp_addr;
            fprintf(stdout, "Received registration message! Current clients:\n");

            for(i = 0; i < global_t_arg->num_clients; i++){

                temp_addr.sin_addr.s_addr = global_t_arg->clients[i];

                fprintf(stdout, inet_ntoa(temp_addr.sin_addr));
                fprintf(stdout, "\n");

                if(clientaddr.sin_addr.s_addr == global_t_arg->clients[i]){
                    address_in_client_list = 1;
                    temp_addr.sin_addr.s_addr = clientaddr.sin_addr.s_addr;
                    fprintf(stdout, "Client already registered: ");
                    fprintf(stdout, inet_ntoa(temp_addr.sin_addr));
                    fprintf(stdout, "\n");
                }
            }
            if(address_in_client_list == 0 && global_t_arg->num_clients < MAX_CLIENTS){
                global_t_arg->clients[global_t_arg->num_clients] = clientaddr.sin_addr.s_addr;
                global_t_arg->num_clients++;
                fprintf(stdout, "Added client: ");
                fprintf(stdout, inet_ntoa(clientaddr.sin_addr));
                fprintf(stdout, "\n");
            } else {
                fprintf(stdout, "Did not add client!\n");
            }
            address_in_client_list = 0;
        }

        if(msg_type == TEMPO) {
            fprintf(stdout, "Received tempo message!\n");
            t_msg.message_type  = msg_type;
            t_msg.device_id     = ntohl(*(receive_buffer + 1));
            t_msg.bpm           = ntohl(*(receive_buffer + 2));
            t_msg.confidence    = ntohl(*(receive_buffer + 3));
            t_msg.measure       = ntohl(*(receive_buffer + 4));
            t_msg.beat          = ntohl(*(receive_buffer + 5));

            write_to_tempo_buffer(global_t_arg->tempo_buffer, t_msg, buffer_stats);
        } else if(msg_type == EVENT) {
            fprintf(stdout, "Received event message!\n");
            e_msg.message_type      = msg_type;
            e_msg.device_id         = ntohl(*(receive_buffer + 1));
            e_msg.event_type        = ntohl(*(receive_buffer + 2));
            e_msg.measure           = ntohl(*(receive_buffer + 3));
            e_msg.beat              = ntohl(*(receive_buffer + 4));
            e_msg.num_used_params   = ntohl(*(receive_buffer + 5));
            for(i = 0; i < e_msg.num_used_params; i++) {
                if(i < e_msg.num_used_params) {
                    e_msg.params[i] = ntohl(*(receive_buffer + (6 + i)));
                } else {
                    e_msg.params[i] = 0;
                }
            }

            write_to_event_buffer(global_t_arg->event_buffer, e_msg, buffer_stats);
        } else if(msg_type == TIME) {
            fprintf(stdout, "Received time message!\n");
        }

        n = sendto(sockfd, receive_buffer, n, 0, (struct sockaddr*)&clientaddr, clientlen);

        if(n < 0){
            //do error stuff
        }
    }
}

int write_to_event_buffer(void *event_buffer, struct event_message message, void *buffer_stats) {

    struct shared_buffer_stats *stats = buffer_stats;
    struct event_message *event_buff = event_buffer;

    if(stats->event_buffer_locked == 1) {
        fprintf(stdout, "event buffer locked!");
        return -1;
    }

    stats->event_buffer_locked = 1;

    //check if we need to rollover to 0
    if(stats->event_buffer_last_write_ix >= EVENT_BUFFER_SIZE) {
        stats->event_buffer_last_write_ix = 0;
        stats->event_buffer_rollovers++;
    }

    fprintf(stdout, "Writing event to buffer\n");
    fprintf(stdout, "Message type: %i, Device ID: %i, Event Type: %i, measure: %i, beat: %i, num_params: %i\n", 
        message.message_type,
        message.device_id,
        message.event_type,
        message.measure,
        message.beat,
        message.num_used_params
    );

    stats->event_buffer_last_write_ix++;
    event_buff[stats->event_buffer_last_write_ix] = message;
    fprintf(stdout, "Wrote message to event buffer position %i\n", stats->event_buffer_last_write_ix);
    stats->event_buffer_locked = 0;
}

//Write data in message to tempo buffer
//Use parameters in buffer stats to write to correct location or reject request if locked
//Return -1 if failure, else return write index
int write_to_tempo_buffer(void *tempo_buffer, struct tempo_message message, void *buffer_stats) {

    struct shared_buffer_stats *stats = buffer_stats;
    struct tempo_message *tempo_buff = tempo_buffer;

    if(stats->tempo_buffer_locked == 1) {
        fprintf(stdout, "tempo buffer locked!");
        return -1;
    } 

    stats->tempo_buffer_locked = 1;

    //Check if we need to rollover to 0
    if(stats->tempo_buffer_last_write_ix >= TEMPO_BUFFER_SIZE) {
        stats->tempo_buffer_last_write_ix = 0;
        stats->tempo_buffer_rollovers++;
    }

    fprintf(stdout, "Writing tempo to buffer\n");
    fprintf(stdout, "Message type: %i, Device ID: %i, BPM: %i, Confidence: %i, measure: %i, beat: %i\n", 
        message.message_type,
        message.device_id,
        message.bpm,
        message.confidence,
        message.measure,
        message.beat
    );
    
    stats->tempo_buffer_last_write_ix++;
    tempo_buff[stats->tempo_buffer_last_write_ix] = message;
    fprintf(stdout, "Wrote message to buffer position %i\n", stats->tempo_buffer_last_write_ix);

    stats->tempo_buffer_locked = 0;
}

void kill_time(int time_ms) {
    clock_t start = clock();
    while(clock() < start + time_ms) {}
}

// udp_listener() Adapted from:
// Example of a simple UDP server:
// https://gist.github.com/miekg/a61d55a8ec6560ad6c4a2747b21e6128


// pthread usage
// https://www.cs.cmu.edu/afs/cs/academic/class/15492-f07/www/pthreads.html

