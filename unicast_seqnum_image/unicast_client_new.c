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

int udp_recv(int argc, char *argv[]); // server
int check_socket_avail(int sd, int sec);

int main(int argc, char *argv[]) {
    udp_recv(argc, argv);
    return 0;
}


void error(const char *msg) {
    perror(msg);
    exit(1);
}

int udp_recv(int argc, char *argv[]){
    
    int sockfd, portno, n;
    struct sockaddr_in serv_addr;
    struct hostent *server;

    char sendbuf[1024] = {0};
    char file_name[50];
    char file_ext[10];
    char *chunk_buffer;
    int first_chunk_flag = 1;
    int full_chunk_size, num_full_chunk, offset, chunk_count= 0;

    float percent = 0;
    FILE *file;

    time_t t = time(NULL);
    struct tm tm = *localtime(&t);

    portno = atoi(argv[2]);
    sockfd = socket(PF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) error("ERROR opening socket");

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
    n = sendto(sockfd, sendbuf, strlen(sendbuf), 0, (struct sockaddr *)&servaddr, sizeof(servaddr));
    if (n < 0) error("ERROR : sendto()");   

    // recv file extension
    memset(file_ext, 0, sizeof(file_ext));
    n = recvfrom(sockfd, &file_ext, sizeof(file_ext), 0, NULL, NULL);
    if (n == -1 && errno != EINTR) ERR_EXIT("recvfrom");
    strcpy(file_name, "udp_receiver.");
    strcat(file_name, file_ext);

    // open udp_udp_receiver.X
    file = fopen(file_name,"w");
    if (file == NULL){
        printf("Unable to create file.\n");
        exit(EXIT_FAILURE);
    }

    // recv full_chunk_size and malloc buffer
    n = recvfrom(sockfd, &full_chunk_size, sizeof(full_chunk_size), 0, NULL, NULL);
    if (n == -1 && errno != EINTR) ERR_EXIT("recvfrom");
    int recv_size = full_chunk_size + sizeof(int);
    chunk_buffer = (char *)malloc(recv_size);

    // recv num_full_chunk, and offset
    n = recvfrom(sockfd, &num_full_chunk, sizeof(num_full_chunk), 0, NULL, NULL);
    if (n == -1 && errno != EINTR) ERR_EXIT("recvfrom");
    n = recvfrom(sockfd, &offset, sizeof(offset), 0, NULL, NULL);
    if (n == -1 && errno != EINTR) ERR_EXIT("recvfrom");

    int *board;
	int board_size = num_full_chunk;
	int loss_count = 0;
	if(offset)
		board_size += 1;
	board = (int *)malloc(board_size * sizeof(int));
	for(int i = 0; i < board_size; ++i)
		board[i] = 0;

    printf("1\n");
    do {
        memset(chunk_buffer, '\0', recv_size);
		int recv_n = recvfrom(sockfd, chunk_buffer, recv_size, 0, NULL, NULL); 
		if (recv_n == -1 && errno != EINTR) ERR_EXIT("recvfrom");

        // fwrite chunk_buffer to udp_receiver.X
		// printf("seq num = %d\n", *(int *)(chunk_buffer));
		int seqnum = *(int *)(chunk_buffer);
		board[seqnum] = 1;
        fwrite(chunk_buffer + sizeof(int), 1, recv_n - sizeof(int), file);

    } while(check_socket_avail(sockfd, 1));


	for(int i = 0; i < board_size; ++i) {
		if(!board[i]) {
			printf("unreceived : %d\n", i);
			++loss_count;
		}
	}
	printf("#datagrame: %d, #loss: %d\n", board_size, loss_count);
	float loss_ratio = 0;
	if(loss_count) 
		loss_ratio = (float)loss_count / (float)board_size;
	printf("loss_rate: %f\n", loss_ratio);

    close(sockfd);
    
    
    return 0;
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
		fd_set rfds;
        struct timeval tv = {sec,0};
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