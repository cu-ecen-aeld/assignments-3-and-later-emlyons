#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <string.h>
#include <unistd.h>
#include <syslog.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>

/*
    Create a socket based program with name aesdsocket in the “server” directory which:

    [x] a. Is compiled by the “all” and “default” target of a Makefile in the “server” directory and supports cross compilation, placing the executable file in the “server” directory and named aesdsocket.

    [x]  b. Opens a stream socket bound to port 9000, failing and returning -1 if any of the socket connection steps fail.

    [x]  c. Listens for and accepts a connection

    [x]  d. Logs message to the syslog “Accepted connection from xxx” where XXXX is the IP address of the connected client. 

    [x]  e. Receives data over the connection and appends to file /var/tmp/aesdsocketdata, creating this file if it doesn’t exist.

    Your implementation should use a newline to separate data packets received.  In other words a packet is considered complete when a newline character is found in the input receive stream, and each newline should result in an append to the /var/tmp/aesdsocketdata file.

    You may assume the data stream does not include null characters (therefore can be processed using string handling functions).

    You may assume the length of the packet will be shorter than the available heap size.  In other words, as long as you handle malloc() associated failures with error messages you may discard associated over-length packets.

    [ ]  f. Returns the full content of /var/tmp/aesdsocketdata to the client as soon as the received data packet completes.

    You may assume the total size of all packets sent (and therefore size of /var/tmp/aesdsocketdata) will be less than the size of the root filesystem, however you may not assume this total size of all packets sent will be less than the size of the available RAM for the process heap.

    [ ]  g. Logs message to the syslog “Closed connection from XXX” where XXX is the IP address of the connected client.

    [ ]  h. Restarts accepting connections from new clients forever in a loop until SIGINT or SIGTERM is received (see below).

    [ ]  i. Gracefully exits when SIGINT or SIGTERM is received, completing any open connection operations, closing any open sockets, and deleting the file /var/tmp/aesdsocketdata.

    Logs message to the syslog “Caught signal, exiting” when SIGINT or SIGTERM is received.
*/

int _setup(const char *host, const char *port)
{
    const int capacity = 1;

    struct addrinfo hints, *res, *p;
    int sock_fd;
    int status;

    // Zero out the hints structure
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;        // AF_INET or AF_INET6 to force version
    hints.ai_socktype = SOCK_STREAM;    // TCP
    hints.ai_flags = AI_PASSIVE;        // For wildcard IP address if host is NULL

    // Get address info
    if ((status = getaddrinfo(host, port, &hints, &res)) != 0) {
        perror("getaddrinfo()\n");
        return -1;
    }

    for (p = res; p != NULL; p = p->ai_next) {

        sock_fd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (sock_fd == -1) {
            perror("socket()\n");
            continue;
        }

        int yes = 1;
        if (setsockopt(sock_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) == -1) {
            perror("setsockopt()");
            close(sock_fd);
            continue;
        }

        if (bind(sock_fd, p->ai_addr, p->ai_addrlen) == -1) {
            perror("bind()\n");
            close(sock_fd);
            continue;
        }

        break; // success
    }
    if (p == NULL) {
        fprintf(stderr, "bind()\n");
        freeaddrinfo(res);
        return -1;
    }

    printf("socket bound successfully on %s:%s\n", host, port);
    freeaddrinfo(res);

    if (listen(sock_fd, capacity) == -1) {
        perror("listen()");
        close(sock_fd);
        return -1;
    }

    return sock_fd;
}

int _accept(int sock_fd)
{
    int client_fd;
    struct sockaddr_in cliaddr;
    socklen_t cliaddrlen = sizeof(cliaddr);
    client_fd = accept(sock_fd, (struct sockaddr*)&cliaddr, &cliaddrlen);
    if (client_fd == -1) {
        perror("accept()");
        return -1;
    }
    char ipstr[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &cliaddr.sin_addr, ipstr, sizeof(ipstr));
    syslog(LOG_USER, "Accepted connection from %s:%d", ipstr, ntohs(cliaddr.sin_port));
    return client_fd;
}

int _receive(int sock_fd, int client_fd, char* buffer, int size) {
    int bytes_received = recv(client_fd, buffer, size-1, 0);
    if (bytes_received == -1) {
        perror("recv()");
        return -1;
    }
    if (bytes_received == 0) {
        close(client_fd);
        return 0;
    }
    buffer[bytes_received] = '\0'; // null-terminate safely

    return bytes_received;
}

int _cache(const char* writefile, const char* writestr, int writesize)
{
    int file_descriptor = open(writefile, O_WRONLY | O_APPEND | O_CREAT, 0644);
    if (file_descriptor == -1)
    {
        syslog(LOG_ERR, "failed to open file %s with error %d\n", writefile, file_descriptor);
        return -1;
    }

    int bytes = write(file_descriptor, writestr, writesize);
    if (bytes == -1)
    {
        syslog(LOG_ERR, "failed to write to file %s\n", writefile);
    }
    else if (bytes != writesize)
    {
        syslog(LOG_ERR, "partial write to %s, %d/%d bytes written\n", writefile, bytes, writesize);
    }
    else
    {
        printf("%d: %.*s\n", writesize, writesize, writestr);
        syslog(LOG_DEBUG, "wrote %d bytes to %s\n", bytes, writefile);
    }

    if (close(file_descriptor) == -1)
    {
        syslog(LOG_ERR, "failed to close file %s\n", writefile);
        return -1;
    }
}

int _send() {
    return 0;
}

int main(int argc, char *argv[])
{

    openlog(NULL, LOG_PID, LOG_USER);
    
    int sock_fd = _setup("localhost", "9000");
    if (sock_fd == -1) {
        perror("setup()");
        return -1;
    }

    const int buffer_size = 1024;
    char buffer[buffer_size];
    size_t line_buffer_size = buffer_size;
    size_t used_size = 0;
    char* line_buffer = malloc(buffer_size);
    if (line_buffer == NULL) {
        perror("malloc()");
        close(sock_fd);
        return -1;
    }

    while(1)
    {

        // wait for client
        int client_fd = _accept(sock_fd);


        int connected = 1;
        while(connected)
        {
            int bytes_received = _receive(sock_fd, client_fd, buffer, buffer_size);
            if (bytes_received == -1) {
                close(sock_fd);
                perror("message()");
                return -1;
            }
            else if (bytes_received == 0) {
                connected = 0;
            }

            // append until no more data
            const char* message_cache = "/var/tmp/aesdsocketdata";
            for (int i = 0; i < bytes_received; i++)
            {

                size_t remaining_size = line_buffer_size - used_size;
                if (remaining_size == 0)
                {
                    // grow size if not enough room
                    // table doubling
                    char* temp = realloc(line_buffer, 2*line_buffer_size);
                    if (temp == NULL) {
                        free(line_buffer);
                        close(sock_fd);
                        perror("realloc()");
                        return -1;
                    }
                    line_buffer = temp;
                    line_buffer_size = 2*line_buffer_size;
                }

                *(line_buffer + used_size++) = buffer[i];
                if (buffer[i] == '\n')
                {
                    // flush to cache
                    if (_cache(message_cache, line_buffer, used_size) == -1) {
                        free(line_buffer);
                        close(sock_fd);
                        perror("cache()");
                        return -1;
                    }

                    // reset buffer
                    used_size = 0;
                }
            }

        }

    }
    // add to signal handler
    free(line_buffer);
    close(sock_fd);  // Or continue with listen(), accept(), etc.
    
    return 0;
}