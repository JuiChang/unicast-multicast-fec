/* Receiver/client multicast Datagram example. */
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h> // for close
#include <string.h> // for memset

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

using fecpp::byte;

struct sendele {
    size_t block_no;
    byte share[SHARESIZE];
    size_t len;
};

void decode_function(size_t block, size_t /*max_blocks*/, const byte buf[], size_t size);
int check_socket_avail(int sd, int sec);

char *result[FEC_K];
int result_size[FEC_K];

int main() {
    struct sockaddr_in localSock;
    struct ip_mreq group;
    int sd;

    /* Create a datagram socket on which to receive. */
    sd = socket(AF_INET, SOCK_DGRAM, 0);
    if(sd < 0) {
        perror("Opening datagram socket error");
        exit(1);
    } else
        printf("Opening datagram socket....OK.\n");


    /* Enable SO_REUSEADDR to allow multiple instances of this */
    /* application to receive copies of the multicast datagrams. */
    //{
    int reuse = 1;
    //if(setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, (char *)&reuse, sizeof(reuse)) < 0)
    if(setsockopt(sd, SOL_SOCKET, SO_REUSEPORT, (char *)&reuse, sizeof(reuse)) < 0) {
        perror("Setting SO_REUSEADDR error");
        close(sd);
        exit(1);
    } else
        printf("Setting SO_REUSEADDR...OK.\n");
    //}


    /* Bind to the proper port number with the IP address */
    /* specified as INADDR_ANY. */
    memset((char *) &localSock, 0, sizeof(localSock));
    localSock.sin_family = AF_INET;
    localSock.sin_port = htons(4321);
    localSock.sin_addr.s_addr = INADDR_ANY;
    if(bind(sd, (struct sockaddr*)&localSock, sizeof(localSock))) {
        perror("Binding datagram socket error");
        close(sd);
        exit(1);
    } else
        printf("Binding datagram socket...OK.\n");


    /* Join the multicast group 226.1.1.1 on the local 203.106.93.94 */
    /* interface. Note that this IP_ADD_MEMBERSHIP option must be */
    /* called for each local interface over which the multicast */
    /* datagrams are to be received. */
    group.imr_multiaddr.s_addr = inet_addr("226.1.1.1");
    group.imr_interface.s_addr = inet_addr("192.168.1.103");
    if(setsockopt(sd, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char *)&group, sizeof(group)) < 0) {
        perror("Adding multicast group error");
        close(sd);
        exit(1);
    } else
        printf("Adding multicast group...OK.\n");




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
        int n = read(sd, &ele, sizeof(struct sendele));
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
        if(result_size[i]) { // fwrite the blocks
            fwrite(result[i], 1, result_size[i], file);
        } else {  // print the IDs of lost blocks, and compute ratio
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