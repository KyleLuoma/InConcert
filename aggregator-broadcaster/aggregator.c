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



void main() {
    fprintf(stdout, "\n ------- InConcert Aggregator Broadcaster ------- \n ");

    pthread_t udp_listener_thread, udp_broadcaster_thread, 
              tempo_calculator_thread, event_manager_thread, 
              screen_display_thread;

    int udp_listener_t_ret, udp_broadcaster_t_ret,
        tempo_calculator_t_ret, event_manager_t_ret,
        screen_display_t_ret;

    int *receive_buffer = malloc(BUFFER_SIZE);
    int *send_buffer = malloc(BUFFER_SIZE);

    struct udp_listener_t_arg listener_arg;
    listener_arg.rcv_buffer = receive_buffer;

    udp_listener_t_ret = pthread_create( &udp_listener_thread, NULL, &udp_listener, &listener_arg);

    while(1) {};
}


static void * udp_listener(void *arg) {

    struct udp_listener_t_arg *listener_arg = arg;

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
        fprintf(stdout, "error binding to socket \n");
    }

    buf = malloc(BUFFER_SIZE);

    while(1) {

        msg_type = -1;

        memset(buf, 0, BUFFER_SIZE);
        n = recvfrom(sockfd, listener_arg->rcv_buffer, BUFFER_SIZE, 0, (struct sockaddr *)&clientaddr, &clientlen);
        if(n < 0) {
            fprintf(stdout, "\nrecvfrom error \n");
        } else {
            fprintf(stdout, "\nReceived UDP packet \n");
        }
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
            //htonl(*(listener_arg->rcv_buffer + i))

            msg_type = htonl(*(listener_arg->rcv_buffer));

            fprintf(stdout, "\nMessage type: %i\n", msg_type);
        }

        //handle message types

        if(msg_type == TEMPO) {
            fprintf(stdout, "Received tempo message!\n");
        } else if(msg_type == EVENT) {
            fprintf(stdout, "Received event message!\n");
        } else if(msg_type == TIME) {
            fprintf(stdout, "Received time message!\n");
        }

        n = sendto(sockfd, listener_arg->rcv_buffer, n, 0, (struct sockaddr*)&clientaddr, clientlen);
        if(n < 0){
            //do error stuff
        }
    }
}

// udp_listener() Adapted from:
// Example of a simple UDP server:
// https://gist.github.com/miekg/a61d55a8ec6560ad6c4a2747b21e6128


// pthread usage
// https://www.cs.cmu.edu/afs/cs/academic/class/15492-f07/www/pthreads.html

