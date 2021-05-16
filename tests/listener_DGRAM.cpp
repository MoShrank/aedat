/*
** listener.c -- a datagram sockets "server" demo
*/

#include <stdio.h>
#include <stdlib.h>
#include <fstream>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#define MYPORT "4950"    // the port users will be connecting to

// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

int main(int argc, char *argv[])
{
    std::fstream fileOutput;
    int sockfd;
    struct addrinfo hints, *servinfo, *p;
    int rv;
    int numbytes;
    struct sockaddr_storage their_addr;
    socklen_t addr_len;
    char s[INET6_ADDRSTRLEN];

    u_int16_t int_buf[6];
    bool timestamp_flag, polarity; 
    uint16_t x_coord, y_coord;
    uint64_t timestamp; 

    if (argc >= 2){
	    fileOutput.open(argv[1], std::fstream::app);
	}

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET6; // set to AF_INET to use IPv4
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_flags = AI_PASSIVE; // use my IP
    
    // Get adrress-info 
    if ((rv = getaddrinfo(NULL, MYPORT, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }

    // loop through all the results and bind to the first we can
    for(p = servinfo; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype,
                p->ai_protocol)) == -1) {
            perror("listener: socket");
            continue;
        }

        if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            perror("listener: bind");
            continue;
        }

        break;
    }


    if (p == NULL) {
        fprintf(stderr, "listener: failed to bind socket\n");
        return 2;
    }

    freeaddrinfo(servinfo);

    printf("listener: waiting to recvfrom...\n");

    addr_len = sizeof their_addr;
    while (true) {
        if ((numbytes = recvfrom(sockfd, int_buf, sizeof(int_buf) , 0, (struct sockaddr *)&their_addr, &addr_len)) == -1) {
            perror("recvfrom");
            exit(1);
        }

        //printf("listener: got packet from %s\n", inet_ntop(their_addr.ss_family, get_in_addr((struct sockaddr *)&their_addr), s, sizeof s));
        printf("listener: packet is %d bytes long\n", numbytes);

        // Decoding according to protocol
        for(int i = 0; i<=numbytes/2; i++){
            int_buf[i] = ntohs(int_buf[i]);
            if (i == 0){
                if(int_buf[i] & 0x8000){
                    timestamp_flag = 1;
                }
                else{
                    timestamp_flag = 0;
                    
                }

                x_coord = int_buf[i] & 0x7FFF;
            }

            if (i == 1){
                if(int_buf[i] & 0x8000){
                    polarity = 1;
                }
                else{
                    polarity = 0;
                }

                y_coord = int_buf[i] & 0x7FFF;
            }


        }

        //printf("timestamp flag: %d\n", timestamp_flag);
        //printf("x: %d\n", x_coord);
        //printf("y: %d\n", y_coord);
        //printf("polarity: %d\n", polarity);
        //printf("======================\n");

        if (argc >= 2){
            fileOutput << "DVS " << " " << x_coord << " " << y_coord << " " << polarity << std::endl;
        }
    }

    close(sockfd);

    return 0;
}


