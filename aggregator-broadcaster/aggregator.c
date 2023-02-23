
#include <stdint.h>

#include "tempo_calculator.h"
#include "beat-master.h"
#include "aggregator.h"

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



void main() {
    fprintf(stdout, "\n ------- InConcert Aggregator Broadcaster ------- \n ");

    pthread_t udp_listener_thread, udp_broadcaster_thread, 
              tempo_calculator_thread, event_manager_thread, 
              screen_display_thread, buffer_watcher_thread, 
              keep_rhythm_thread;

    int udp_listener_t_ret, udp_broadcaster_t_ret,
        tempo_calculator_t_ret, event_manager_t_ret,
        screen_display_t_ret, buffer_watcher_t_ret
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

    udp_listener_t_ret = pthread_create(&udp_listener_thread, NULL, &udp_listener, &global_t_arg);
    buffer_watcher_t_ret = pthread_create(&buffer_watcher_thread, NULL, &buffer_watcher, &global_t_arg);
    tempo_calculator_t_ret = pthread_create(&tempo_calculator_thread, NULL, &tempo_calculator, &global_t_arg);
    udp_broadcaster_t_ret = pthread_create(&udp_broadcaster_thread, NULL, &udp_broadcaster, &global_t_arg);
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
            fprintf(stdout, "     message_type: %i, device_id: %i bpm: %i, confidence: %i, timestamp: %lld\n", 
                msg.message_type, msg.device_id, msg.bpm, msg.confidence, msg.timestamp
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
    int sockfd, optval, n;

    struct sockaddr_in serveraddr;
    struct sockaddr_in broadcastaddr;

    struct tempo_message *tempo_send_buffer = malloc(1);
    struct tempo_message tempo_broadcast_message;

    broadcastaddr.sin_family = AF_INET;
    broadcastaddr.sin_port = BROADCAST_PORT;
    broadcastaddr.sin_addr.s_addr = INADDR_BROADCAST;
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
    
    while(1) {

        //handle broadcasts based on state changes
        if(last_tempo_change != global_t_arg->current_tempo) {

            tempo_broadcast_message.message_type = 0;
            tempo_broadcast_message.device_id = 0;
            tempo_broadcast_message.bpm = global_t_arg->current_tempo;
            tempo_broadcast_message.confidence = 100;
            tempo_broadcast_message.timestamp = 0;

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
    int n;

    int msg_type;
    int client_address;
    

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

        if(msg_type == TEMPO) {
            fprintf(stdout, "Received tempo message!\n");
            struct tempo_message msg;

            msg.message_type = msg_type;
            msg.device_id = ntohl(*(receive_buffer + 1));
            msg.bpm = ntohl(*(receive_buffer + 2));
            msg.confidence = ntohl(*(receive_buffer + 3));
            msg.timestamp = ntohl(*(receive_buffer + 4));

            write_to_tempo_buffer(global_t_arg->tempo_buffer, msg, buffer_stats);
        } else if(msg_type == EVENT) {
            fprintf(stdout, "Received event message!\n");
        } else if(msg_type == TIME) {
            fprintf(stdout, "Received time message!\n");
        }

        n = sendto(sockfd, receive_buffer, n, 0, (struct sockaddr*)&clientaddr, clientlen);

        if(n < 0){
            //do error stuff
        }
    }
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
    fprintf(stdout, "Message type: %i, Device ID: %i, BPM: %i, Confidence: %i, timestamp: %llu\n", 
        message.message_type,
        message.device_id,
        message.bpm,
        message.confidence,
        message.timestamp
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

