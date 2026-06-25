#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <errno.h>
#include <arpa/inet.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <netinet/in.h>

#include "protocol.c"

#define CONNECTION_PORT "4567"
#define CONNECTION_IP "127.0.0.1"

int get_readable_ip(struct addrinfo** addrInfo, struct sockaddr_in** ipInfo, char* ipStringBuffer, socklen_t len);

int main(int argc, char** argv)
{
    int status, returnCode = 0;
    int socketFD = -1;

    struct addrinfo     hints;
    struct addrinfo     *serverInfo = NULL;
    struct sockaddr_in  *ipInfo = NULL;

    char                ipstr[INET_ADDRSTRLEN];

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    if( (status = getaddrinfo(CONNECTION_IP, CONNECTION_PORT, &hints, &serverInfo)) != 0)
    {
        returnCode = status;
        fprintf(stderr, "gai error: %s\n", gai_strerror(status));
        goto free;
    }

    // Convert & store IP address for display after setup
    returnCode = get_readable_ip(&serverInfo, &ipInfo, ipstr, sizeof ipstr);
    if(returnCode != 0) goto free;

    socketFD = socket(serverInfo->ai_family, serverInfo->ai_socktype, serverInfo->ai_protocol);
    if(socketFD == -1)
    {
        returnCode = errno;
        perror("socket creation error");
        goto close;
    }

    printf("Attempting connection to IP: %s, Port: %hu\n", ipstr, ntohs(ipInfo->sin_port));

    if(connect(socketFD, serverInfo->ai_addr, serverInfo->ai_addrlen) == -1)
    {
        returnCode = errno;
        perror("client connect error");
        goto close;
    }
    printf("Connection successful!\n");

    sleep(10);

    char* msg  = "vcvcvcv";
    size_t len = strlen(msg);

    while(1)
    {
        
        send_message(socketFD, 1, msg, len);
        sleep(5);
        struct message recv_msg;
        recv_message(socketFD, &recv_msg);
        printf("Message Length: %u, Message Type: %u, Message Contents: %s\n", recv_msg.msg_length, recv_msg.msg_type, recv_msg.msg);
        
    }
    

close:
    close(socketFD);

free:
    freeaddrinfo(serverInfo);
    return returnCode;
}


int get_readable_ip(struct addrinfo** addrInfo, struct sockaddr_in** ipInfo, char* ipStringBuffer, socklen_t len)
{
    int returnVal = 0;
    *ipInfo = (struct sockaddr_in *)(*addrInfo)->ai_addr;
    if(inet_ntop((*addrInfo)->ai_family, &((*ipInfo)->sin_addr), ipStringBuffer, len) == NULL)
    {
        returnVal = errno;
        perror("ipv4 conversion error");
    }

    return returnVal;
}