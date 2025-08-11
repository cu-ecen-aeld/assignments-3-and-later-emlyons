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
#include <signal.h>
#include <time.h>
#include "thread_pool_dynamic.h"

#define BUFFER_SIZE 1024

#define CACHE_FILE "/var/tmp/aesdsocketdata"

void daemonize();

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

    [x]  f. Returns the full content of /var/tmp/aesdsocketdata to the client as soon as the received data packet completes.

    You may assume the total size of all packets sent (and therefore size of /var/tmp/aesdsocketdata) will be less than the size of the root filesystem, however you may not assume this total size of all packets sent will be less than the size of the available RAM for the process heap.

    [x]  g. Logs message to the syslog “Closed connection from XXX” where XXX is the IP address of the connected client.

    [x]  h. Restarts accepting connections from new clients forever in a loop until SIGINT or SIGTERM is received (see below).

    [x]  i. Gracefully exits when SIGINT or SIGTERM is received, completing any open connection operations, closing any open sockets, and deleting the file /var/tmp/aesdsocketdata.

    Logs message to the syslog “Caught signal, exiting” when SIGINT or SIGTERM is received.
*/

volatile sig_atomic_t RUN = 1;
static pthread_mutex_t file_lock;

int _setup(const char *host, const char *port, int daemon)
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

    if (daemon)
    {
        daemonize();
    }

    if (listen(sock_fd, capacity) == -1) {
        perror("listen()");
        close(sock_fd);
        return -1;
    }

    return sock_fd;
}

int _accept(int sock_fd, struct sockaddr_in* cliaddr, char ipstr[INET_ADDRSTRLEN])
{
    int client_fd;
    socklen_t cliaddrlen = sizeof(*cliaddr);
    client_fd = accept(sock_fd, (struct sockaddr*)cliaddr, &cliaddrlen);
    if (client_fd == -1) {
        perror("accept()");
        return -1;
    }
    inet_ntop(AF_INET, &(cliaddr->sin_addr), ipstr, INET_ADDRSTRLEN);
    syslog(LOG_USER, "Accepted connection from %s:%d", ipstr, ntohs(cliaddr->sin_port));
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

/* _send_cache()
 *   Send entire cache file to client
 * in: client_fd: file descriptor to client socket
 * out: 0 success, -1 error
 */
int _send_cache(int client_fd) {
    
    char cache_buffer[BUFFER_SIZE];
    int cache_fd = open(CACHE_FILE, O_RDONLY);
    if (cache_fd == -1) {
        perror("open()");
        return -1;
    }

    ssize_t bytes_read;
    while ((bytes_read = read(cache_fd, cache_buffer, sizeof(cache_buffer))) > 0) {

        ssize_t total_bytes_sent = 0;
        while (total_bytes_sent < bytes_read)
        {
            ssize_t bytes_sent = send(client_fd, cache_buffer + total_bytes_sent, bytes_read - total_bytes_sent, 0);
            if (bytes_sent == -1 || bytes_sent == 0)
            {
                close(cache_fd);
                perror("send()");
                return -1;
            }
            total_bytes_sent += bytes_sent;
        }
    }

    close(cache_fd);
    if (bytes_read == -1)
    {
        perror("read()");
        return -1;
    }
    return 0;
}

static void signal_handler(int signal_number)
{
    if (signal_number == SIGTERM || signal_number == SIGINT)
    {
        syslog(LOG_USER, "Caught signal, exiting");
        RUN = 0;// shutdown
    }
}

int is_daemon(int argc, char *argv[])
{
    int debug = 0;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-d") == 0) {
            debug = 1;
            break;
        }
    }
    return debug;
}

void daemonize()
{
    pid_t pid = fork();
    switch (pid) {
        case -1: // failure
            perror("fork()");
            exit(EXIT_FAILURE);

        case 0: // child process
            pid_t sid = setsid(); // make daemon session leader
            if (sid == -1)
            {
                perror("setsid()");
                exit(EXIT_FAILURE);
            }
            if (chdir("/") == -1)
            {
                perror("chdir()");
                exit(EXIT_FAILURE);
            }
            int fd = open("/dev/null", O_RDWR); // reroute stdio to null
            if (fd == -1)
            {
                perror("open()");
                exit(EXIT_FAILURE);
            }
            dup2(fd, STDIN_FILENO);
            dup2(fd, STDOUT_FILENO);
            dup2(fd, STDERR_FILENO);
            if (fd > 2)
            {
                close(fd);
            }

            pid = fork(); // double fork to prevent reaquiring terminal (remove daemon from session leader)
            if (pid == -1) {
                exit(EXIT_FAILURE);
            }
            else if (pid != 0) {
                exit(EXIT_SUCCESS);
            }
            return;

        default: // parent process
            exit(EXIT_SUCCESS);
    }
}

typedef struct ClientTaskParams
{
    struct sockaddr_in cliaddr;
    char ipstr[INET_ADDRSTRLEN];
    int client_fd;
    int sock_fd;
} ClientTaskParams;

void client_task(void* params)
{
    ClientTaskParams* p = (ClientTaskParams*)params;
    char buffer[BUFFER_SIZE];
    size_t line_buffer_size = BUFFER_SIZE;
    size_t used_size = 0;
    char* line_buffer = malloc(BUFFER_SIZE);
    if (line_buffer == NULL) {
        free(p);
        perror("malloc()");
        return;
    }

    int connected = 1;
    while(connected && RUN)
    {
        int bytes_received = _receive(p->sock_fd, p->client_fd, buffer, BUFFER_SIZE);
        if (bytes_received == -1) {
            close(p->client_fd);
            free(p);
            perror("_receive()");
            return;
        }
        else if (bytes_received == 0) {
            connected = 0;
        }

        // append until no more data
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
                    close(p->client_fd);
                    free(p);
                    perror("realloc()");
                    return;
                }
                line_buffer = temp;
                line_buffer_size = 2*line_buffer_size;
            }

            *(line_buffer + used_size++) = buffer[i];
            if (buffer[i] == '\n')
            {
                pthread_mutex_lock(&file_lock);
                
                // flush to cache
                if (_cache(CACHE_FILE, line_buffer, used_size) == -1) {
                    free(line_buffer);
                    close(p->client_fd);
                    free(p);
                    perror("cache()");
                    pthread_mutex_unlock(&file_lock);
                    return;
                }

                if (_send_cache(p->client_fd) == -1) {
                    free(line_buffer);
                    close(p->client_fd);
                    free(p);
                    perror("send()");
                    pthread_mutex_unlock(&file_lock);
                    return;
                }

                pthread_mutex_unlock(&file_lock);
                // reset buffer
                used_size = 0;
                connected = 0;
                break;
            }
        }
    }
    free(line_buffer);
    close(p->client_fd);
    syslog(LOG_USER, "Closed connection from %s:%d", p->ipstr, ntohs(p->cliaddr.sin_port));
    free(p);
}

void timestamp_task(void* arg)
{
    ThreadPool* thread_pool = (ThreadPool*)arg;
    while (RUN)
    {
        sleep(10);
        if (!RUN)
        {
            break;
        }
        time_t rawtime;
        struct tm* timeinfo;
        time(&rawtime);
        timeinfo = localtime(&rawtime);
        char buffer[20];
        strftime(buffer, sizeof(buffer), "%Y:%m:%d:%H:%M:%S", timeinfo);
        char timestamp[31];
        snprintf(timestamp, sizeof(timestamp), "timestamp:%s\n", buffer);

        pthread_mutex_lock(&thread_pool->m_lock);
        pool_cleanup(thread_pool->m_cleanup);
        pthread_mutex_unlock(&thread_pool->m_lock);

        pthread_mutex_lock(&file_lock);
        if (queue_size(thread_pool->m_threads) > 0) {
            _cache(CACHE_FILE, timestamp, sizeof(timestamp));
        }
        pthread_mutex_unlock(&file_lock);
    }
}

int main(int argc, char *argv[])
{ 
    struct sigaction new_action;
    memset(&new_action, 0, sizeof(struct sigaction));
    new_action.sa_handler = signal_handler;
    if (sigaction(SIGTERM, &new_action, NULL) != 0)
    {
        perror("sigaction(SIGTERM)");
        return -1;
    }
    if (sigaction(SIGINT, &new_action, NULL) != 0)
    {
        perror("sigaction(SIGINT)");
        return -1;
    }

    openlog(NULL, LOG_PID, LOG_USER);
    
    int daemon = is_daemon(argc, argv);
    int sock_fd = _setup("0.0.0.0", "9000", daemon);
    if (sock_fd == -1) {
        perror("setup()");
        return -1;
    }

    ThreadPool* thread_pool;
    if (pool_make_thread_pool(&thread_pool) != 0)
    {
        close(sock_fd);
        perror("make_thread_pool()");
        return -1;
    }

    pthread_mutex_init(&file_lock, NULL);

    pool_dispatch(thread_pool, timestamp_task, thread_pool);

    while(RUN)
    {

        // accept new connection
        ClientTaskParams* client_params = (ClientTaskParams*)malloc(sizeof(ClientTaskParams));
        struct sockaddr_in cliaddr;
        char ipstr[INET_ADDRSTRLEN];
        client_params->client_fd = _accept(sock_fd, &client_params->cliaddr, client_params->ipstr);
        if (client_params->client_fd >= 0)
        {
            pool_dispatch(thread_pool, client_task, (void*)client_params);
        }
    }
    // add to signal handler
    printf("shutting down...");
    close(sock_fd);  // Or continue with listen(), accept(), etc.
    pthread_mutex_destroy(&file_lock);
    if (remove(CACHE_FILE) == -1)
    {
        perror("remove()");
        return -1;
    }
    
    return 0;
}