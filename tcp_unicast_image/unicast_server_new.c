/*
    C socket server example, handles multiple clients using threads
    Compile
    gcc server.c -lpthread -o server

    Run:
    ./unicast_server <port> <filename>
*/
 
#include<stdio.h>
#include<string.h>    //strlen
#include<stdlib.h>    //strlen
#include<sys/socket.h>
#include<arpa/inet.h> //inet_addr
#include<unistd.h>    //write
#include<pthread.h> //for threading , link with lpthread

#include <inttypes.h>
#include <math.h>
#include <errno.h>
#include <sys/ioctl.h>

#define NUM_CHUNK 20
 
struct pthread_arg {
    int client_sock;
    char *file_name;
};

void error(const char *msg);
const char *get_filename_ext(const char *filename);
//the thread function
void *connection_handler(void *);
 
int main(int argc , char *argv[])
{
    int socket_desc , c, portno;
    struct sockaddr_in server , client;

    struct pthread_arg ptharg;

    ptharg.file_name = argv[2];
     
    //Create socket
    socket_desc = socket(AF_INET , SOCK_STREAM , 0);
    if (socket_desc == -1)
    {
        printf("Could not create socket");
    }
    puts("Socket created");
     
    //Prepare the sockaddr_in structure
    portno = atoi(argv[1]);
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = INADDR_ANY;
    server.sin_port = htons(portno);
     
    //Bind
    if( bind(socket_desc,(struct sockaddr *)&server , sizeof(server)) < 0)
    {
        //print the error message
        perror("bind failed. Error");
        return 1;
    }
    puts("bind done");


     
    //Listen
    listen(socket_desc , 3);
     
    //Accept and incoming connection
    puts("Waiting for incoming connections...");
    c = sizeof(struct sockaddr_in);
	pthread_t thread_id;

	
    while( (ptharg.client_sock = accept(socket_desc, (struct sockaddr *)&client, (socklen_t*)&c)) )
    {   
        puts("Connection accepted");
         
        if( pthread_create( &thread_id , NULL ,  connection_handler , (void*) &ptharg) < 0)
        {
            perror("could not create thread");
            return 1;
        }
         
        //Now join the thread , so that we dont terminate before the thread
        //pthread_join( thread_id , NULL);
        puts("Handler assigned");
    }
     
    if (ptharg.client_sock < 0)
    {
        perror("accept failed");
        return 1;
    }
     
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

/*
 * This will handle connection for each client
 * */
void *connection_handler(void *ptharg)
{
    //Get the socket descriptor
    int clisockfd = ((struct pthread_arg *)ptharg)->client_sock;
    char *file_name = ((struct pthread_arg *)ptharg)->file_name;
    int read_size;
    char *message , client_message[2000];


    /////////////////////////////// my code from hw1
    int n;

    int full_chunk_size, offset;
    char file_ext[10]; // file extension of the input file
    char *chunk_buffer; // save the file chunk to write to client later
    char confirm_buffer[256];
    FILE *file;

    file = fopen(file_name, "r");
    if (!file) error("ERROR : opening file.\n");
    
    // determine file file length, full_chunk_size, then malloc chunk_buffer 
    size_t pos = ftell(file);    // Current position
    fseek(file, 0, SEEK_END);    // Go to end
    size_t file_length = ftell(file); // read the position which is the size
    fseek(file, pos, SEEK_SET);  // restore original position
    printf("file length = %zu\n", file_length);
    full_chunk_size = file_length / NUM_CHUNK;
    offset = file_length % NUM_CHUNK;
    // both of (file_length) and NUM_CHUNK are int, so:
    //      1. (file_length) is equal to (full_chunk_size * NUM_CHUNK + offset)
    //      2. there may be (NUM_CHUNK + 1) chunks if the offset isn't 0
    //      3. the last chunk has the size (offset)
    //      4. malloc chunk_buffer with (full_chunk_size) which is >= (offset)
    chunk_buffer = (char *)malloc(full_chunk_size);



    //////// write & read

    // send file extension to client
    bzero(file_ext, sizeof(file_ext));
    strcpy(file_ext, get_filename_ext(file_name));  
    n = write(clisockfd, &file_ext, sizeof(file_ext));
    if (n < 0) error("ERROR writing to socket");

    // send full_chunk_size, offset
    n = write(clisockfd, &full_chunk_size, sizeof(full_chunk_size));
    if (n < 0) error("ERROR writing to socket");
    n = write(clisockfd, &offset, sizeof(offset));
    if (n < 0) error("ERROR writing to socket");

    while (1) {

        // fread a file chunk to chunk_buffer
        bzero(chunk_buffer, full_chunk_size);
        int bytes_read = fread(chunk_buffer, 1, full_chunk_size, file);
        if (bytes_read == 0){ // boundary condition (last)
            printf("Sender: File transfer finished.\n");
            break;
        }
        
        // write a file chunk to client 
        // printf("do write\n");
        n = write(clisockfd, chunk_buffer, bytes_read);
        if (n < 0) error("ERROR writing to socket");  
        
    }
    
         
    return 0;
} 