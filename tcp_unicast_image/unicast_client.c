/*
    Run: ./unicast localhost <port>
*/
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

#include <inttypes.h>
#include <math.h>
#include <errno.h>
#include <sys/ioctl.h>

#define NUM_CHUNK 20

void error(const char *msg)
{
    perror(msg);
    exit(0);
}

int recv_file(int sockfd);

int main(int argc, char *argv[])
{
    int sockfd, portno, n;
    struct sockaddr_in serv_addr;
    struct hostent *server;

    char buffer[10000];

    if (argc < 3) {
       fprintf(stderr,"usage %s hostname port\n", argv[0]);
       exit(0);
    }

    portno = atoi(argv[2]);
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) 
        error("ERROR opening socket");

    server = gethostbyname(argv[1]);
    if (server == NULL) {
        fprintf(stderr,"ERROR, no such host\n");
        exit(0);
    }

    bzero((char *) &serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    bcopy((char *)server->h_addr, 
         (char *)&serv_addr.sin_addr.s_addr,
         server->h_length);
    serv_addr.sin_port = htons(portno);
    if (connect(sockfd,(struct sockaddr *) &serv_addr,sizeof(serv_addr)) < 0) 
        error("ERROR connecting");


    // n = read(sockfd,buffer,255);
    // if (n < 0) 
    //      error("ERROR reading from socket");
    // printf("%s\n",buffer);

    recv_file(sockfd);

    close(sockfd);
    
    return 0;
}


// from hw1 tcp_recv()
int recv_file(int sockfd) {
    int n;
    char sendbuf[1024] = {0};
    char file_name[50];
    char file_ext[10];
    char *chunk_buffer;
    int first_chunk_flag = 1;
    int full_chunk_size, num_full_chunk, offset, chunk_count= 0;

    float percent = 0;
    FILE *file;


    ////// recv file extension and open tcp_receiver.X

    // recv file extension
    bzero(file_ext,sizeof(file_ext));
    n = read(sockfd, &file_ext, sizeof(file_ext));
    if (n < 0) error("ERROR reading from socket 1");
    strcpy(file_name, "tcp_receiver.");
    strcat(file_name, file_ext);

    // open tcp_receiver.X
    file = fopen(file_name,"w");
    if (file == NULL){
        printf("Unable to create file.\n");
        exit(EXIT_FAILURE);
    }

    // recv full_chunk_size, offset
    bzero(&full_chunk_size, sizeof(full_chunk_size));
    n = read(sockfd, &full_chunk_size, sizeof(full_chunk_size));
    if (n < 0) error("ERROR reading from socket");
    chunk_buffer = (char *)malloc(full_chunk_size);

    bzero(&offset, sizeof(offset));
    n = read(sockfd, &offset, sizeof(offset));
    if (n < 0) error("ERROR reading from socket");

    while (1){
        // except boundary conditions, 
        // the workflow of this while loop :
        // "read chunk" --> "file write"
        
        // check amount of data available for sockfd
        int supposed_size;
        if (chunk_count < NUM_CHUNK) {
            supposed_size = full_chunk_size;
        } else {
            supposed_size = offset;
        }
        int count;
        do {
            ioctl(sockfd, FIONREAD, &count);
        } while (count < supposed_size);

        // read a chunk  
        bzero(chunk_buffer, full_chunk_size);
        n = read(sockfd, chunk_buffer, full_chunk_size); // it's ok that full_chunk_size > n
        if (n < 0) error("ERROR reading from socket 3");
        ++chunk_count;

        // file writing
        int s = fwrite(chunk_buffer, 1, n, file);



        // boundary condition (last)
        if ((offset == 0 && chunk_count == NUM_CHUNK) || chunk_count > NUM_CHUNK) {
            printf("TCP Receiver: file transfer finished\n");
            fclose(file);
            break;
        }
    }

    close(sockfd);
    
    return 0;

}
