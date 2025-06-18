#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <syslog.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>

/*
    input:
        1: writefile: relative path to a file (including filename)
        2: writestr: text string which will be written within this file
    
    output:
        1 = success    
        -1 = error
*/
int main(int argc, char *argv[])
{
    openlog(NULL, 0, LOG_USER);
    if (argc != 3)
    {
        syslog(LOG_ERR, "expected 2 arguments, got %d\n", argc);
        return 1;
    }
    const char* writefile = argv[1];
    const char* writestr = argv[2];
    int writesize = strlen(argv[2]);

    syslog(LOG_DEBUG, "Writing %s to %s\n", writestr, writefile);
    
    int file_descriptor = open(writefile, O_WRONLY | O_TRUNC | O_CREAT, 0644);
    if (file_descriptor == -1)
    {
        syslog(LOG_ERR, "failed to open file %s with error %d\n", writefile, file_descriptor);
        return 1;
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
        // syslog(LOG_DEBUG, "wrote %d bytes to %s\n", bytes, writefile);
    }

    if (close(file_descriptor) == -1)
    {
        syslog(LOG_ERR, "failed to close file %s\n", writefile);
        return 1;
    }

    return 0;
}