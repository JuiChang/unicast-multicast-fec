/*
Run:
    must run the unicast_server first
	./unicast_client 
*/
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h> // for close
#include <string.h> // for memset

#include <netinet/in.h>
#include <netdb.h>

#include <inttypes.h>
#include <math.h>
#include <errno.h>
#include <sys/ioctl.h>

#include <time.h> // time() 
#include <sys/time.h>

#include "fecpp.h"
#include <stdio.h>
#include <string.h>
#include <iostream>

#include <sys/select.h>

#define SHARESIZE 9000
#define FEC_N 255
#define FEC_K 230

#define ERR_EXIT(m) \
    do { \
        perror(m); \
        exit(EXIT_FAILURE); \
    } while (0)

using fecpp::byte;

struct sendele {
    size_t block_no;
    byte share[SHARESIZE];
    size_t len;
};

void error(const char *msg);

void decode_function(size_t block, size_t /*max_blocks*/, const byte buf[], size_t size);
int check_socket_avail(int sd, int sec);

char *result[FEC_K];
int result_size[FEC_K];

int main(int argc, char *argv[]) {
    int sd;

    char sendbuf[1024] = {0};
    int portno;
    struct sockaddr_in serv_addr;
    struct hostent *server;

    portno = atoi(argv[2]);
    sd = socket(PF_INET, SOCK_DGRAM, 0);
    if (sd < 0) error("ERROR opening socket");

    server = gethostbyname(argv[1]);
    if (server == NULL) {
        fprintf(stderr,"ERROR, no such host\n");
        exit(0);
    }

    struct sockaddr_in servaddr;
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(portno);
    bcopy((char *)server->h_addr, 
         (char *)&servaddr.sin_addr.s_addr,
         server->h_length);

    // call the server
    int n = sendto(sd, sendbuf, strlen(sendbuf), 0, (struct sockaddr *)&servaddr, sizeof(servaddr));
    if (n < 0) error("ERROR : sendto()");   
    printf("called\n");



    /* Read from the socket. */
    
    int fec_k = FEC_K;
    int fec_n = FEC_N;
    fecpp::fec_code fec(fec_k, fec_n);

    size_t tri1;
    std::map<size_t, const byte*> tri2;
    size_t& share_len = tri1;
    std::map<size_t, const byte*>& m = tri2;

    for(int i = 0; i < 1000; ++i)
        result_size[i] = 0;

    int sboard[FEC_N]; // share board, recording if each share is loss
    int sloss = 0; // share loss count
    for(int i = 0; i < FEC_N; ++i)
        sboard[i] = 0;
    struct sendele ele;
    do {
        int n = recvfrom(sd, &ele, sizeof(struct sendele), 0, NULL, NULL);
        if (n == -1 && errno != EINTR) ERR_EXIT("recvfrom");
        if(!n) {
            printf("n = 0\n");
        }
        
        share_len = ele.len;
        // Contents of share[] are only valid in this scope, must copy
        byte* share_copy = new byte[share_len];
        memcpy(share_copy, ele.share, share_len);
        m[ele.block_no] = share_copy; 
        sboard[ele.block_no] = 1;
    } while(check_socket_avail(sd, 1));

    for(int i = 0; i < FEC_N; ++i) { // print the IDs of lost shares, and compute ratio
        if(sboard[i] == 0) {
            printf("loss: share id = %d\n", i);
            ++sloss;
        }
    }
    printf("share loss ratio: %f\n", (float)sloss / FEC_N);

    fec.decode(m, share_len, decode_function); // get content from share_len and shares

    char file_name[50];
    // determine the file name and open
    FILE *file;
    strcpy(file_name, "udp_receiver");
	struct timeval t1;
	gettimeofday(&t1, NULL);
	srand(t1.tv_usec * t1.tv_sec);
	int fn = rand() % 1000000;
	char strfn[12];
	memset(strfn, 0, sizeof(strfn));
	sprintf(strfn, "%d", fn);
	strcat(file_name, strfn);
	strcat(file_name, ".");
    // strcat(file_name, file_ext);
    strcat(file_name, "txt");
    file = fopen(file_name,"w");
    if (file == NULL) {
        printf("Unable to create file.\n");
        exit(EXIT_FAILURE);
    }

    int bboard[FEC_K]; // block board, recording if each share is loss after decoding
    int bloss = 0; // block loss count
    for(int i = 0; i < FEC_K; ++i)
        bboard[i] = 0;
    for(int i = 0; i < FEC_K; ++i) {
        if(result_size[i]) {  // fwrite the blocks
            fwrite(result[i], 1, result_size[i], file);
        } else {   // print the IDs of lost blocks, and compute ratio
            printf("loss: block id = %d\n", i);
            ++bloss;
        }
    }
    printf("block loss ratio: %f\n", (float)bloss / FEC_K);

    return 0;
}

void decode_function(size_t block, size_t /*max_blocks*/, const byte buf[], size_t size) {
    // copy the arguments to the global variable: result, result_size
    result[block] = (char *)malloc(size);
    memset(result[block], '\0', size);
    strcpy(result[block], (char *)buf);
    result_size[block] = size;
}

int check_socket_avail(int sd, int sec) {
    int ret = 0;
    
    fd_set rfds;
	struct timeval tv = {0,0};
	FD_ZERO(&rfds);
    FD_SET(sd, &rfds);
    ret = select(sd + 1, &rfds, NULL, NULL, &tv);
    if (ret == -1) {
        perror("select()");
        exit(1);
    } else if(!ret) {
        tv = {sec,0};
        FD_ZERO(&rfds);
        FD_SET(sd, &rfds);
        ret = select(sd + 1, &rfds, NULL, NULL, &tv);
        if (ret == -1) {
            perror("select()");
            exit(1);
        }
    }
    return ret;
}

void error(const char *msg) {
    perror(msg);
    exit(1);
}