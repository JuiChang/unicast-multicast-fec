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

#define NUM_CHUNK 20


void error(const char *msg);
const char *get_filename_ext(const char *filename);
void udp_subchunk_size(int file_length, int num_chunk, int *subchunk_size, int *num_subchunk);
int send_file(int sockfd, struct sockaddr_in groupSock, char *file_name);
 
int main (int argc, char *argv[ ])
{
	struct in_addr localInterface;
	struct sockaddr_in groupSock;
	int sd;
	char databuf[1024] = "Multicast test message.";
	int datalen = sizeof(databuf);

	/* Create a datagram socket on which to send. */
	sd = socket(AF_INET, SOCK_DGRAM, 0);
	if(sd < 0)
	{
		perror("Opening datagram socket error");
		exit(1);
	}
	else
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
	if(setsockopt(sd, IPPROTO_IP, IP_MULTICAST_IF, (char *)&localInterface, sizeof(localInterface)) < 0)
	{
		perror("Setting local interface error");
		exit(1);
	}
	else
		printf("Setting the local interface...OK\n");


	send_file(sd, groupSock, argv[1]);
		

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

int send_file(int sockfd, struct sockaddr_in groupSock, char *file_name) {

    int n;

    int full_chunk_size, num_full_chunk, offset;  // the size of chunk which is sent
    char recvbuf[1024] = {0};  // receiving the first call from client
    char file_ext[10];
    char *chunk_buffer;
    char confirm_buffer[256];
    FILE *file;

    file = fopen(file_name, "r");
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

    // send file extension
    memset(file_ext, 0, sizeof(file_ext));
    strcpy(file_ext, get_filename_ext(file_name));
    n = sendto(sockfd, &file_ext, sizeof(file_ext), 0, (struct sockaddr*)&groupSock, sizeof(groupSock));
    if (n < 0) error("ERROR : sendto()"); 

    

    // send full_chunk_size, num_full_chunk, offset
    n = sendto(sockfd, &full_chunk_size, sizeof(full_chunk_size), 0, (struct sockaddr*)&groupSock, sizeof(groupSock));
    if (n < 0) error("ERROR : sendto()"); 
    
    while (1) {

        // fread a file chunk to chunk_buffer
        memset(chunk_buffer, 0, full_chunk_size);
        int bytes_read = fread(chunk_buffer, 1, full_chunk_size, file);

        // boundary condition (last)
        if (bytes_read == 0){ 
            printf("Sender: File transfer finished.\n");
            break;
        }

        // write a file chunk to client 
        n = sendto(sockfd, chunk_buffer, bytes_read, 0, (struct sockaddr*)&groupSock, sizeof(groupSock));  
        if (n < 0) error("ERROR : sendto() 2");
    }

    // close(sockfd);

    return 0;

}