#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

#define NUM_CHUNK 20

#define TCP_CONFIRM 1

#define _POSIX_C_SOURCE 200809L

#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <time.h>

#include<errno.h>

#include <sys/ioctl.h>


#define ERR_EXIT(m) \
    do { \
        perror(m); \
        exit(EXIT_FAILURE); \
    } while (0)

void error(const char *msg);
const char *get_filename_ext(const char *filename);
void udp_subchunk_size(int file_length, int num_chunk, int *subchunk_size, int *num_subchunk);

int udp_send(int argc, char *argv[]); // server
void concat_seqnum(int seqnum, void **new_buffer, int *new_size, void *old_buffer, int old_size);


int main(int argc, char *argv[]) {
    udp_send(argc, argv);
    return 0;
}


void error(const char *msg) {
    perror(msg);
    exit(1);
}

const char *get_filename_ext(const char *filename) {
    const char *dot = strrchr(filename, '.');
    if(!dot || dot == filename) return "";
    return dot + 1;
}

void udp_subchunk_size(int file_length, int num_chunk, int *subchunk_size, int *num_subchunk) {
    // the last two arguments are return values
    
    // sendto() in udp_send() cannot afford too large size of data,
    // i.e. (file_length / NUM_CHUNK) bytes may be too large to send,
    // if it is, adjust to ((file_length / NUM_CHUNK) / num_subchunk) bytes for each sent chunk

    *subchunk_size = 0;
    *num_subchunk = 1;
    for (*num_subchunk = 1; (file_length / (num_chunk * *num_subchunk) > 10000); ++*num_subchunk) {
        ;
    }
    *subchunk_size = file_length / (num_chunk * *num_subchunk);
}


int udp_send(int argc, char *argv[]){
    
    int sockfd, portno;
    socklen_t clilen;
    struct sockaddr_in serv_addr;
    struct sockaddr_in peeraddr;
    socklen_t peerlen;
    int n;

    int full_chunk_size, num_full_chunk, offset;  // the size of chunk which is sent
    char recvbuf[1024] = {0};  // receiving the first call from client
    char file_ext[10];
    char *chunk_buffer;
    char confirm_buffer[256];
    FILE *file;

    file = fopen(argv[3], "r");
    if (!file) error("ERROR : opening file.\n");
    
    // determine file length, full_chunk_size, then malloc chunk_buffer 
    size_t pos = ftell(file);    // Current position
    fseek(file, 0, SEEK_END);    // Go to end
    size_t file_length = ftell(file); // read the position which is the size
    fseek(file, pos, SEEK_SET);  // restore original position
    printf("file length = %zu\n", file_length);
    int num_subchunk = 0;
    udp_subchunk_size(file_length, NUM_CHUNK, &full_chunk_size, &num_subchunk);
    num_full_chunk = file_length / full_chunk_size;
    offset = file_length % full_chunk_size;
    chunk_buffer = (char *)malloc(full_chunk_size);


    //////// create socket and connect with client

    sockfd = socket(PF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0)
        error("ERROR opening socket");
    
    memset(&serv_addr, 0, sizeof(serv_addr));
    portno = atoi(argv[2]);
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_addr.sin_port = htons(portno);
    if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0)
        error("ERROR on binding");
    
    //////// write & read

    // recv call from client
    peerlen = sizeof(peeraddr);
    memset(recvbuf, 0, sizeof(recvbuf));
    n = recvfrom(sockfd, recvbuf, sizeof(recvbuf), 0, (struct sockaddr *)&peeraddr, &peerlen);
    if (n == -1 && errno != EINTR)
        ERR_EXIT("recvfrom error");

    // send file extension
    printf("send file extension\n");
    memset(file_ext, 0, sizeof(file_ext));
    strcpy(file_ext, get_filename_ext(argv[3]));
    n = sendto(sockfd, &file_ext, sizeof(file_ext), 0, (struct sockaddr *)&peeraddr, peerlen);
    if (n < 0) error("ERROR : sendto()");  

    // send full_chunk_size, num_full_chunk, offset
    n = sendto(sockfd, &full_chunk_size, sizeof(full_chunk_size), 0, (struct sockaddr *)&peeraddr, peerlen);
    if (n < 0) error("ERROR : sendto()"); 
    n = sendto(sockfd, &num_full_chunk, sizeof(num_full_chunk), 0, (struct sockaddr *)&peeraddr, peerlen);
    if (n < 0) error("ERROR : sendto()"); 
    n = sendto(sockfd, &offset, sizeof(offset), 0, (struct sockaddr *)&peeraddr, peerlen);
    if (n < 0) error("ERROR : sendto()"); 
    
    int seqnum = 0;
    while (1) {
        // fread a file chunk to chunk_buffer
        memset(chunk_buffer, 0, full_chunk_size);
        int bytes_read = fread(chunk_buffer, 1, full_chunk_size, file);
        if (bytes_read == 0){ 
            printf("Sender: File transfer finished.\n");
            break;
        }

        // write a file chunk to client 
        int send_size = 0;
        // concat the sequence number with the data to be sent
        concat_seqnum(seqnum, (void**)&chunk_buffer, &send_size, chunk_buffer, bytes_read);
        n = sendto(sockfd, chunk_buffer, send_size, 0, (struct sockaddr *)&peeraddr, peerlen);  
        if (n < 0) error("ERROR : sendto() 2");   

        ++seqnum;
    }

    close(sockfd);

    return 0;

}

void concat_seqnum(int seqnum, void **new_buffer, int *new_size, void *old_buffer, int old_size) {
    *new_size = old_size + sizeof(int);
    *new_buffer = malloc(*new_size);
    memcpy(*new_buffer, &seqnum, sizeof(int));
    memcpy(*new_buffer + sizeof(int), old_buffer, old_size);
}