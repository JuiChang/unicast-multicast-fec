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

#include <sys/select.h>

#define NUM_CHUNK 20

int recv_file(int sockfd);
int check_socket_avail(int sd, int sec);

#define ERR_EXIT(m) \
    do { \
        perror(m); \
        exit(EXIT_FAILURE); \
    } while (0)
 
int main(int argc, char *argv[])
{
	struct sockaddr_in localSock;
	struct ip_mreq group;
	int sd;
	// int datalen;
	// char databuf[1024];

	/* Create a datagram socket on which to receive. */
	sd = socket(AF_INET, SOCK_DGRAM, 0);
	if(sd < 0)
	{
		perror("Opening datagram socket error");
		exit(1);
	}
	else
	printf("Opening datagram socket....OK.\n");
	
		 
	/* Enable SO_REUSEADDR to allow multiple instances of this */
	/* application to receive copies of the multicast datagrams. */
	//{
	int reuse = 1;
	//if(setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, (char *)&reuse, sizeof(reuse)) < 0)
	if(setsockopt(sd, SOL_SOCKET, SO_REUSEPORT, (char *)&reuse, sizeof(reuse)) < 0)
	{
		perror("Setting SO_REUSEADDR error");
		close(sd);
		exit(1);
	}
	else
		printf("Setting SO_REUSEADDR...OK.\n");
	//}
	 

	/* Bind to the proper port number with the IP address */
	/* specified as INADDR_ANY. */
	memset((char *) &localSock, 0, sizeof(localSock));
	localSock.sin_family = AF_INET;
	localSock.sin_port = htons(4321);
	localSock.sin_addr.s_addr = INADDR_ANY;
	if(bind(sd, (struct sockaddr*)&localSock, sizeof(localSock)))
	{
		perror("Binding datagram socket error");
		close(sd);
		exit(1);
	}
	else
		printf("Binding datagram socket...OK.\n");
	 

	/* Join the multicast group 226.1.1.1 on the local 203.106.93.94 */
	/* interface. Note that this IP_ADD_MEMBERSHIP option must be */
	/* called for each local interface over which the multicast */
	/* datagrams are to be received. */
	group.imr_multiaddr.s_addr = inet_addr("226.1.1.1");
	group.imr_interface.s_addr = inet_addr("192.168.1.103");
	if(setsockopt(sd, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char *)&group, sizeof(group)) < 0)
	{
		perror("Adding multicast group error");
		close(sd);
		exit(1);
	}
	else
		printf("Adding multicast group...OK.\n");
	 


	recv_file(sd);


	return 0;
}


int recv_file(int sockfd) {
	// int sockfd, portno, 
	int n;
    // struct sockaddr_in serv_addr;
    // struct hostent *server;

    char sendbuf[1024] = {0};
    char file_name[50];
    char file_ext[10];
    char *chunk_buffer;
    int first_chunk_flag = 1;
    int full_chunk_size, num_full_chunk, offset;

    float percent = 0;
    FILE *file;
 

    // recv file extension
    memset(file_ext, 0, sizeof(file_ext));
    n = recvfrom(sockfd, &file_ext, sizeof(file_ext), 0, NULL, NULL);
    if (n == -1 && errno != EINTR) ERR_EXIT("recvfrom");
    // strcpy(file_name, "udp_receiver.");
    // strcat(file_name, file_ext);
	strcpy(file_name, "udp_receiver");

	struct timeval t1;
	gettimeofday(&t1, NULL);
	srand(t1.tv_usec * t1.tv_sec);
	int fn = rand() % 100000;
	char strfn[12];
	memset(strfn, 0, sizeof(strfn));
	sprintf(strfn, "%d", fn);

	strcat(file_name, strfn);
	strcat(file_name, ".");
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

	// int array to record the loss datagrams
	int *board;
	int board_size = num_full_chunk;
	int loss_count = 0;
	if(offset)
		board_size += 1;
	board = (int *)malloc(board_size * sizeof(int));
	for(int i = 0; i < board_size; ++i)
		board[i] = 0;

    do {
        memset(chunk_buffer, '\0', recv_size);
		int recv_n = recvfrom(sockfd, chunk_buffer, recv_size, 0, NULL, NULL); 
		if (recv_n == -1 && errno != EINTR) ERR_EXIT("recvfrom");

        // fwrite chunk_buffer to udp_receiver.X
		int seqnum = *(int *)(chunk_buffer);
		board[seqnum] = 1;
        fwrite(chunk_buffer + sizeof(int), 1, recv_n - sizeof(int), file);

    } while(check_socket_avail(sockfd, 1));

	// print the lost datagrams and compute the ratio
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