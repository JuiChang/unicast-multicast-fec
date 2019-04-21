/*
Send Multicast Datagram code example.
Run:
	./muticast_server <filename>
*/

#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h> // for memset

#include <inttypes.h>
#include <math.h>
#include <errno.h>
#include <sys/ioctl.h>

#include "fecpp.h"
#include <stdio.h>
#include <string.h>
#include <iostream>

#define SHARESIZE 9000
#define FEC_N 255
#define FEC_K 250

using fecpp::byte;

struct Sendele {
    size_t block_no;
    // const byte share[10000];
    byte share[SHARESIZE];
    size_t len;
};

void error(const char *msg);
const char *get_filename_ext(const char *filename);
// void udp_subchunk_size(int file_length, int num_chunk, int *subchunk_size, int *num_subchunk);
// int send_file(int sockfd, struct sockaddr_in groupSock, char *file_name);
void encode_function(size_t block_no, size_t, const byte share[], size_t len);

void deter_km(int file_length, int *k, int *m);


int sd;
struct sockaddr_in groupSock;
int num_send = 0;


int main (int argc, char *argv[ ]) {
    struct in_addr localInterface;
    // struct sockaddr_in groupSock;
    // int sd;
    // char databuf[1024] = "Multicast test message.";
    char *databuf;
    // int datalen = sizeof(databuf);

    /* Create a datagram socket on which to send. */
    sd = socket(AF_INET, SOCK_DGRAM, 0);
    if(sd < 0) {
        perror("Opening datagram socket error");
        exit(1);
    } else
        printf("Opening the datagram socket...OK.\n");


    /* Initialize the group sockaddr structure with a */
    /* group address of 225.1.1.1 and port 5555. */
    memset((char *) &groupSock, 0, sizeof(groupSock));
    groupSock.sin_family = AF_INET;
    groupSock.sin_addr.s_addr = inet_addr("226.1.1.1");
    groupSock.sin_port = htons(4321);


    /* Set local interface for outbound multicast datagrams. */
    /* The IP address specified must be associated with a local, */
    /* multicast capable interface. */
    //localInterface.s_addr = inet_addr("192.168.32.143");
    localInterface.s_addr = inet_addr("192.168.1.103");
    if(setsockopt(sd, IPPROTO_IP, IP_MULTICAST_IF, (char *)&localInterface, sizeof(localInterface)) < 0) {
        perror("Setting local interface error");
        exit(1);
    } else
        printf("Setting the local interface...OK\n");


    // send_file(sd, groupSock, argv[1]);
    /* Send a message to the multicast group specified by the*/
    /* groupSock sockaddr structure. */
    /*int datalen = 1024;*/

    int fec_n = FEC_N;
    int fec_k = FEC_K;

    if(fec_k >= fec_n) {
        printf("fec_k >= fec_n\n");
        exit(1);
    }
    printf("%d %d\n", fec_k, fec_n);
    printf("sizeof(struct Sendele) = %lu\n", sizeof(struct Sendele));

    char file_ext[10];
    int k_offset = 0;

    FILE *file;
    file = fopen(argv[1], "r");
    if (!file) error("ERROR : opening file.\n");

    memset(file_ext, 0, sizeof(file_ext));
    strcpy(file_ext, get_filename_ext(argv[1]));

    // determine file length
    size_t pos = ftell(file);    // Current position
    fseek(file, 0, SEEK_END);    // Go to end
    size_t file_length = ftell(file); // read the position which is the size
    fseek(file, pos, SEEK_SET);  // restore original position
    printf("file length = %zu\n", file_length);

    int datalen = file_length;
    if(file_length % fec_k) {
        datalen = file_length + (fec_k - file_length % fec_k);
        k_offset = fec_k - file_length % fec_k;
    }
    databuf = (char *)malloc(datalen);
    memset(databuf, 0, datalen);
    fread(databuf, 1, file_length, file);
    
    

    fecpp::fec_code fec(fec_k, fec_n);
    fec.encode((byte *)databuf, datalen, encode_function); // save content to share_len and shares


    // close(sd);
    return 0;
}

const char *get_filename_ext(const char *filename) {
    const char *dot = strrchr(filename, '.');
    if(!dot || dot == filename) return "";
    return dot + 1;
}

void error(const char *msg) {
    perror(msg);
    exit(1);
}

void encode_function(size_t block_no, size_t, const byte share[], size_t len) {
    if(len > SHARESIZE) {
        printf("len > %d\n", SHARESIZE);
        exit(1);
    }
    struct Sendele ele;
    ele.block_no = block_no;
    memset(ele.share, 0, SHARESIZE);
    for(int i = 0; i < len; ++i) 
        ele.share[i] = share[i];
    ele.len = len;

    int n = sendto(sd, &ele, sizeof(struct Sendele), 0, (struct sockaddr*)&groupSock, sizeof(groupSock));
    if (n < 0) error("ERROR : sendto()"); 
}
