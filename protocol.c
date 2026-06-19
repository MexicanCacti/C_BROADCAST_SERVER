#define MAX_MESSAGE_SIZE 4096

#include <stdlib.h>
#include <arpa/inet.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>

/*
    length - Length of msg in message
    type   - msg_type being sent

*/
struct msg_header
{
    uint32_t length;
    uint32_t type;
};

/*
    msg - Char buffer array to hold bytes being sent
    msg_length - Length of msg
    msg_type - Type of message being sent
*/
struct message
{
    char msg[MAX_MESSAGE_SIZE + 1];
    uint32_t msg_length;
    uint32_t msg_type;
};

int send_bytes(int socketFD, void* src, size_t n);
int send_message(int socketFD, uint32_t type, void* msg, uint32_t length);

int recv_bytes(int socketFD, void* ptr, size_t n);
int recv_message(int socketFD, struct message* msg);


/*
    Sends n bytes of the structure pointed to by src to the file descriptor

    returns 0 on success, -1 on error
*/
int send_bytes(int socketFD, void* src, size_t n)
{

    ssize_t writtenBytes = 0;
    do{
        ssize_t bytes = send(socketFD, src + writtenBytes, n - writtenBytes, 0);
        if(bytes < 0) return -1;
        if(bytes == 0) return -1;
        writtenBytes += bytes;

    } while(writtenBytes < n);

    return 0;
}

/*
    Attempts to create & send a message + msg_header to the given socketFD
    First: Sends the msg header
    Second: Sends a stream of bytes for the message that is of size length

    Returns 0 if message successfully sent
    Else -1 & sets errno
*/
int send_message(int socketFD, uint32_t type, void* msg, uint32_t length)
{
    if(length >= MAX_MESSAGE_SIZE)  // msg must be <= MAX_MESSAGE_SIZE, binary protocol, so don't forget to add null term if printing as a string!
    {
        errno = EMSGSIZE;
        return -1;
    }

    int returnCode = 0;
    struct msg_header header;
    header.length = htonl((uint32_t)length);
    header.type = htonl((uint32_t)type);

    // Send the header struct length (length of msg)
    returnCode = send_bytes(socketFD, &header.length, sizeof(u_int32_t));
    if(returnCode != 0) return returnCode;

    // Send the header struct type (type of msg)
    returnCode = send_bytes(socketFD, &header.type, sizeof(uint32_t));
    if(returnCode != 0) return returnCode;

    // Send the msg
    returnCode = send_bytes(socketFD, msg, length);
    if(returnCode != 0) return returnCode;

    return 0;
}





/*
    Reads n bytes to the structure pointed to by dst

    Returns 0 on success, -1 on error
*/
int recv_bytes(int socketFD, void* dst, size_t n)
{
    int returnCode = 0;
    char    byteBuffer[n];

    ssize_t readBytes = 0;
    ssize_t bytesRead = 0;

    do{
        readBytes = recv(socketFD, byteBuffer + bytesRead, n - bytesRead, 0);
        if(readBytes < 0)
        {
            returnCode = -1;
            perror("read");
            goto cleanup;
        }
        if(readBytes == 0)
        {
            errno = ECONNRESET;
            returnCode = -1;
            goto cleanup;
        }

        bytesRead += readBytes;
    }while(bytesRead < n);

    memcpy(dst, byteBuffer, n);
    cleanup:
    return returnCode;
}

/*
    Reads a socket for the following...
    1. Reads a byte stream for the length of payload
    2. Reads a byte stream for the type of payload
    3. Reads a byte stream for the payload
    4. Uses the payload + length + type to fill in a msg object passed in

    Returns 0 on Success
    -1 on error, sets errno
*/
int recv_message(int socketFD, struct message* msg)
{
    int returnCode = 0;
    uint32_t msg_header_length_n, msg_header_type_n, msg_header_length_h, msg_header_type_h = 0;

    // Recv the header length
    returnCode = recv_bytes(socketFD, &msg_header_length_n, sizeof(uint32_t));
    if(returnCode != 0) return returnCode;

    msg_header_length_h = ntohl(msg_header_length_n);

    // Recv the header type
    returnCode = recv_bytes(socketFD, &msg_header_type_n, sizeof(uint32_t));
    if(returnCode != 0) return returnCode;

    msg_header_type_h = ntohl(msg_header_type_n);

    if(msg_header_length_h > MAX_MESSAGE_SIZE) // msg must be <= MAX_MESSAGE_SIZE, binary protocol, so don't forget to add null term if printing as a string!
    {
        errno = EMSGSIZE;
        returnCode = -1;
        return returnCode;
    }

    char messageBuffer[msg_header_length_h];

    // Recv the msg
    returnCode = recv_bytes(socketFD, &messageBuffer, msg_header_length_h);
    if(returnCode != 0) return returnCode;


    // Fill in msg object
    memcpy(msg->msg, messageBuffer, msg_header_length_h);
    msg->msg[msg_header_length_h] = '\0';
    msg->msg_length = msg_header_length_h;
    msg->msg_type = msg_header_type_h;

    cleanup:
    return returnCode;
}