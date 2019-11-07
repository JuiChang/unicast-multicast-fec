/*
Run this server before run the client:
	./<exe> <host> <port> <filename>
*/

#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h> // for memset
#include <unistd.h> // for close

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
#define FEC_K 230

#define ERR_EXIT(m) \
    do { \
        perror(m); \
        exit(EXIT_FAILURE); \
    } while (0)

using fecpp::byte;

// send element (which is sent to client)
struct Sendele {
    size_t block_no;
    byte share[SHARESIZE];
    size_t len;
};

void error(const char *msg);
const char *get_filename_ext(const char *filename);
void encode_function(size_t block_no, size_t, const byte share[], size_t len);


int sd;
struct sockaddr_in groupSock;
struct sockaddr_in peeraddr;
socklen_t peerlen;


int main (int argc, char *argv[ ]) {
    int portno;
    struct sockaddr_in serv_addr;
    char recvbuf[1024] = {0};  // receiving the first call from client

    char *databuf;

    //////// create socket and connect with client

    sd = socket(PF_INET, SOCK_DGRAM, 0);
    if (sd < 0) error("ERROR opening socket");
    
    memset(&serv_addr, 0, sizeof(serv_addr));
    portno = atoi(argv[2]);
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_addr.sin_port = htons(portno);
    if (bind(sd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0)
        error("ERROR on binding");
    
    //////// write & read

    // recv call from client
    peerlen = sizeof(peeraddr);
    memset(recvbuf, 0, sizeof(recvbuf));
    int n = recvfrom(sd, recvbuf, sizeof(recvbuf), 0, (struct sockaddr *)&peeraddr, &peerlen);
    if (n == -1 && errno != EINTR) ERR_EXIT("recvfrom error");
    printf("received\n");

    int fec_n = FEC_N;
    int fec_k = FEC_K;
    if(fec_k >= fec_n) {
        printf("fec_k >= fec_n\n");
        exit(1);
    }
    printf("k = %d, n = %d\n", fec_k, fec_n);
    printf("sizeof(struct Sendele) = %lu\n", sizeof(struct Sendele));

    char file_ext[10];
    int k_offset = 0;

    FILE *file;
    file = fopen(argv[3], "r");
    if (!file) error("ERROR : opening file.\n");
    memset(file_ext, 0, sizeof(file_ext));
    strcpy(file_ext, get_filename_ext(argv[3]));
    // determine file length
    size_t pos = ftell(file);    // Current position
    fseek(file, 0, SEEK_END);    // Go to end
    size_t file_length = ftell(file); // read the position which is the size
    fseek(file, pos, SEEK_SET);  // restore original position
    printf("file length = %zu\n", file_length);

    // make the data size be the multiple of k
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
    // copy the arguments into the struct Sendele and sendto client
    struct Sendele ele;
    ele.block_no = block_no;
    memset(ele.share, 0, SHARESIZE);
    for(int i = 0; i < len; ++i) 
        ele.share[i] = share[i];
    ele.len = len;
    // printf("share len = %zu\n", len);
    int n = sendto(sd, &ele, sizeof(struct Sendele), 0, (struct sockaddr *)&peeraddr, peerlen);
    if (n < 0) error("ERROR : sendto()"); 
}
