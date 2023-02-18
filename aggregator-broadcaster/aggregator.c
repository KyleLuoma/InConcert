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

    udp_listener();
}


void loop() {
    printf("\n Starting UDP listener");
    udp_listener();
}


void udp_listener() {
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
        memset(buf, 0, BUFFER_SIZE);
        n = recvfrom(sockfd, buf, BUFFER_SIZE, 0, (struct sockaddr *)&clientaddr, &clientlen);
        if(n < 0) {
            fprintf(stdout, "recvfrom error \n");
        } else {
            fprintf(stdout, "Received UDP packet \n");
        }
        hostp = gethostbyaddr(
            (const char *)&clientaddr.sin_addr.s_addr, 
            sizeof(clientaddr.sin_addr.s_addr), 
            AF_INET
        );
        if(hostaddrp == NULL) {
            fprintf(stdout, "Error: null host address\n");
        }
        hostaddrp = inet_ntoa(clientaddr.sin_addr);
        if(hostaddrp == NULL) {
            //do error stuff
        } else {
            fprintf(stdout, "%i \n", clientaddr.sin_addr);
            fprintf(stdout, hostaddrp);
            fprintf(stdout, "\n");
        }
        n = sendto(sockfd, buf, n, 0, (struct sockaddr*)&clientaddr, clientlen);
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

