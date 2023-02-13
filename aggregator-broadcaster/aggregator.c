#include <aggregator.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>



void main() {

    loop();

}


void loop() {
    udp_listener();
}


void udp_listener() {
    int sockfd;
    int portno;
    int clientlen;
    struct sockaddr_in serveraddr;
    struct sockaddr_in clientaddr;
    struct hostent *hostp;
    char *buf;
    char *hostaddrp;
    int n;

    sockfd = socket(AF_INET, SOCK_DGRAM, 0);

    if(sockfd < 0) {
        //do error stuff
    }

    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (const void *), sizeof(int));

    bzero((char *)&serveraddr, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
    serveraddr.sin_port = htons(PORT);

    if(bind(sockfd,(struct sockaddr *)&serveraddr, sizeof(serveraddr)) < 0) {
        //do error stuff
    }

    buf = malloc(BUFFER_SIZE);

    while(1) {
        memset(buf, 0, BUFFER_SIZE);
        n = recvfrom(sockfd, buf, BUFFER_SIZE, 0, (struc sockaddr *)&clientaddr, &clientlen);
        if(n < ) {
            //do error stuff
        }
        hostp = gethostbyaddr(
            (const char *)&clientaddr.sin_addr.s_addr, 
            sizeof(clientaddr.sin_addr.s_addr), 
            AF_INET
        );
        if(hostaddrp == NULL) {
            //do error stuff
        }
        hostaddrp = inet_ntoa(clientaddr.sin_addr);
        if(hostaddrp == NULL) {
            //do error stuff
        } else {
            printf(hostaddrp);
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