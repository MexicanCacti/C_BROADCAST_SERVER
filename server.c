#define _GNU_SOURCE

#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <poll.h>
#include <errno.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <string.h>
#include <unistd.h>
#include <sys/queue.h>
#include <signal.h>

#include "protocol.c"
#include "linked_nodes.c"

#define BACKLOG 10
#define LISTEN_PORT "4567"
#define LISTEN_IP "127.0.0.1"
#define INITIAL_CLIENT_CONNECTIONS 10

struct client
{
    int connectionFD;
    bool isConnected; // Note: init is 0, false
} typedef client;

int get_readable_ip(struct addrinfo** addrInfo, struct sockaddr_in** ipInfo, char* ipStringBuffer, socklen_t len);

int create_and_bind_socket(struct addrinfo** addrInfo, int* listenFD);

int readMessage(int connectionFD, struct message *msg);

void broadcastMessage(linkedNodes **clientUseList, client **clientList, int skipFD, struct message *msg, int* cleanupList, size_t* cleanupCount);

int main(int argc, char** argv)
{
    signal(SIGPIPE, SIG_IGN); // So sudden disconnects doesn't completely crash the program!
    int status, returnCode = 0;
    int listenFD = -1;
    size_t maxConnections = INITIAL_CLIENT_CONNECTIONS;

    struct addrinfo     hints;
    struct addrinfo     *serverInfo = NULL;
    struct sockaddr_in  *ipInfo = NULL;

    char    ipstr[INET_ADDRSTRLEN];
    // Note calloc used b/c malloc doesn't initialize elements!
    client  *clients = calloc(maxConnections, sizeof(client));
    struct pollfd  *clientDescriptors = calloc(maxConnections, sizeof(struct pollfd));

    if(!clients || !clientDescriptors)
    {
        perror("client / client descriptors allocation error\n");
        goto free;
    }

    // Anything init to -1 is skipped by poll
    for(size_t i = 0 ; i < maxConnections; ++i)
    {
        clients[i].connectionFD = -1;
        clientDescriptors[i].fd = -1;
    }

    // A queue of indicies that can be used to populate clientDescriptors/clients list
    // When you need an index, take one from the queue, then populate
    // When a client disconnects, the client is set to not connected, the index is put back into the queue
    linkedNodes *clientIndexQueue = calloc(1,sizeof(linkedNodes));
    linkedNodes *clientUseList = calloc(1, sizeof(linkedNodes));
    if(!clientIndexQueue || !clientUseList)
    {
        perror("client index array allocation error");
        goto free;
    }

    clientIndexQueue->nodeCount = 0;
    clientIndexQueue->headNode = NULL;
    clientUseList->headNode = NULL;
    clientUseList->nodeCount = 0;
    insertNodesUpTo(&clientIndexQueue, maxConnections);
    printf("Num enqueued available: %d\n", clientIndexQueue->nodeCount);
    struct pollfd  serverPoll[1];

    if(clients == NULL)
    {
        returnCode = errno;
        perror("client array allocation error");
        goto free;
    }

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    if( (status = getaddrinfo(LISTEN_IP, LISTEN_PORT, &hints, &serverInfo)) != 0)
    {
        returnCode = status;
        fprintf(stderr, "gai error: %s\n", gai_strerror(status));
        goto free;
    }

    returnCode = get_readable_ip(&serverInfo, &ipInfo, ipstr, sizeof ipstr);
    if(returnCode != 0) goto free;

    returnCode = create_and_bind_socket(&serverInfo, &listenFD);
    if(returnCode != 0) goto free;

    status = listen(listenFD, BACKLOG);
    if(status == -1)
    {
        returnCode = errno;
        perror("listen error");
        goto close;
    }

    printf("Server listening on IP: %s and Port: %hu\n", ipstr, ntohs(ipInfo->sin_port));

    serverPoll[0].fd = listenFD;
    serverPoll[0].events = POLLIN;
    // Poll connections
    // Then for each client connection, poll for a message, broadcast to every other client

    int connectionFD = 0;
    struct sockaddr_storage connection_addr;
    socklen_t addr_size = sizeof connection_addr;
    int serverPollValue = 0;
    int connectionPollValue = 0;
    while(1)
    {

        // Poll the listening socket for connections
        if( (serverPollValue = poll(serverPoll, 1, 5000)) == -1)
        {
            perror("server connection poll");
            break;
        }
        else if(serverPollValue == 0)
        {
            goto poll_connections;
        }
        
        short serverEvents = serverPoll[0].events;
        short serverReturnEvents = serverPoll[0].revents;
        if( serverReturnEvents & POLLERR)
        {
            perror("Poll error\n");
            goto close;
        }
        else if(serverReturnEvents & POLLIN)
        {
            printf("Connection received!\n");
            connectionFD = accept(serverPoll[0].fd, (struct sockaddr*)&connection_addr, &addr_size);
            if(connectionFD == -1)
            {
                perror("Connection accept error\n");
                continue;
            }

            char connectionIP[INET_ADDRSTRLEN];
            char connectionPort[NI_MAXSERV];

            status = getnameinfo( (struct sockaddr *)&connection_addr, addr_size, connectionIP, sizeof(connectionIP), connectionPort, sizeof(connectionPort), NI_NUMERICHOST | NI_NUMERICSERV); // Flag return numeric vals
            if(status != 0) continue;
            printf("Connection from IP: %s and Port: %s\n", connectionIP, connectionPort);

            // Note check if empty, if so resize
            int clientConnectionIndex = popNode(&clientIndexQueue);
            printf("Client Connection Assigned Index: %d\n", clientConnectionIndex);
            printf("Num enqueued available: %d\n", clientIndexQueue->nodeCount);
            client c;
            c.connectionFD = connectionFD;
            c.isConnected = true;
            struct pollfd clientPollObject;
            clientPollObject.fd = c.connectionFD;
            clientPollObject.events = POLLIN | POLLERR | POLLHUP | POLLNVAL | POLLRDHUP;

            // Put into poll queue
            clientDescriptors[clientConnectionIndex] = clientPollObject;

            // Put into use list
            insertNode(&clientUseList, clientConnectionIndex);

            // Save client info
            clients[clientConnectionIndex] = c;
            
            fflush(0);

        }

        // Poll all the client connections we have in our client array!
        poll_connections:

        if( (connectionPollValue = poll(clientDescriptors, maxConnections, 5000)) == -1)
        {
            perror("connections poll error");
            goto close;
        }
        else if(connectionPollValue == 0)
        {
            printf("No events from connections\n");
            fflush(0);
            continue;
        }
        
        short connectionReturnEvents = 0;
        for(int i = 0 ; i < maxConnections; ++i)
        {   
            if(connectionPollValue == 0) break; // No socket events
            if(!clients[i].isConnected || clients[i].connectionFD == -1) continue;

            // Check the return events
            connectionReturnEvents = clientDescriptors[i].revents;
            if(connectionReturnEvents == 0) continue;


            if( connectionReturnEvents & POLLIN)
            {
                struct message msg;
                if(readMessage(clients[i].connectionFD, &msg) == 0)
                {
                    printf("Message Length: %u, Message Type: %u, Message Contents: %s\n", msg.msg_length, msg.msg_type, msg.msg);
                    int cleanupList[maxConnections];
                    size_t cleanupCount = 0;
                    broadcastMessage(&clientUseList, &clients, clients[i].connectionFD, &msg, cleanupList, &cleanupCount);
                    
                    for(size_t j = 0 ; j < cleanupCount; ++j)
                    {
                        size_t clientIndex = cleanupList[j];
                        printf("Cleanup client index %zu\n", clientIndex);
                        close(clients[clientIndex].connectionFD);
                        clients[clientIndex].isConnected = false;
                        clients[clientIndex].connectionFD = -1;
                        clientDescriptors[clientIndex].fd = -1;
                        removeNode(&clientUseList, clientIndex);
                        insertNode(&clientIndexQueue, clientIndex);
                        printf("Connection Removed, Sudden Disconnect!\n");
                    }
                    
                }
                
            }
            if( connectionReturnEvents & (POLLERR | POLLHUP | POLLRDHUP | POLLNVAL) )
            {
                close(clients[i].connectionFD);
                clients[i].isConnected = false;
                clients[i].connectionFD = -1;
                clientDescriptors[i].fd = -1;
                
                insertNode(&clientIndexQueue, i);
                removeNode(&clientUseList, i);
                printf("Connection Removed!\n");
                
            }
        
            --connectionPollValue; // In case you want to check if all polls have been ran through
        }


    }


    close:
        close(listenFD);
    free:
        freeaddrinfo(serverInfo);
        freeLinkedNodes(&clientIndexQueue);
        freeLinkedNodes(&clientUseList);
        free(clientIndexQueue);
        free(clientUseList);
        free(clients);
        free(clientDescriptors);

    return returnCode;
}

int get_readable_ip(struct addrinfo** addrInfo, struct sockaddr_in** ipInfo, char* ipStringBuffer, socklen_t len)
{
    int returnCode = 0;
    *ipInfo = (struct sockaddr_in *)(*addrInfo)->ai_addr;
    // convert ip addr from binary to text form
    if(inet_ntop( (*addrInfo)->ai_family, & ( (*ipInfo)->sin_addr), ipStringBuffer, len) == NULL)
    {
        returnCode = errno;
        perror("ipv4 conversion error");
    }

    return returnCode;
}

int create_and_bind_socket(struct addrinfo** addrInfo, int* listenFD)
{
    int returnCode = 0;

    *listenFD = socket((*addrInfo)->ai_family, (*addrInfo)->ai_socktype, (*addrInfo)->ai_protocol );
    if(*listenFD == -1)
    {
        returnCode = errno;
        perror("socket creation error");
        return returnCode;
    }

    int yes = 1;
    // if in use/not cleaned up, mark to reuse
    setsockopt(*listenFD, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));

    returnCode = bind(*listenFD, (*addrInfo)->ai_addr, (*addrInfo)->ai_addrlen);
    if(returnCode != 0)
    {
        perror("socket binding error");
    }

    return returnCode;
}

int readMessage(int connectionFD, struct message *msg)
{
    if(connectionFD == -1) return -1;
    return recv_message(connectionFD, msg);
}

void broadcastMessage(linkedNodes **clientUseList, client **clientList, int skipFD, struct message *msg, int* cleanupList, size_t* cleanupCount)
{
    if((*clientUseList)->nodeCount == 0 || (*clientUseList)->headNode == NULL) return;
    node *sendClientIndexNode = (*clientUseList)->headNode;

    while(sendClientIndexNode != NULL)
    {
        int sendClientFD = (*clientList)[sendClientIndexNode->val].connectionFD;
        if(sendClientFD == skipFD || sendClientFD == -1)
        {
            sendClientIndexNode = sendClientIndexNode->next;
            continue;
        }

        if(send_message(sendClientFD, msg->msg_type, msg->msg, msg->msg_length) != 0)
        {
            perror("Error sending message to a client");
            cleanupList[(*cleanupCount)++] = sendClientIndexNode->val;
            printf("Added %d to cleanup List\n", sendClientIndexNode->val);
        }
        sendClientIndexNode = sendClientIndexNode->next;
    }
}