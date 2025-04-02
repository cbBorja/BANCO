#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <errno.h>

/**
 * This utility helps diagnose and fix FIFO EOF issues
 * Usage: ./fix_eof <fifo_path>
 */
int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <fifo_path>\n", argv[0]);
        return 1;
    }
    
    const char *fifo_path = argv[1];
    
    // Check if the FIFO exists
    struct stat st;
    if (stat(fifo_path, &st) == -1) {
        fprintf(stderr, "Error: FIFO %s does not exist\n", fifo_path);
        return 1;
    }
    
    if (!S_ISFIFO(st.st_mode)) {
        fprintf(stderr, "Error: %s is not a FIFO\n", fifo_path);
        return 1;
    }
    
    printf("FIFO %s exists and is valid.\n", fifo_path);
    
    // Try to open the FIFO for reading in non-blocking mode
    printf("Trying to open FIFO for reading (non-blocking)...\n");
    int fd_read = open(fifo_path, O_RDONLY | O_NONBLOCK);
    if (fd_read < 0) {
        printf("Failed to open for reading: %s\n", strerror(errno));
    } else {
        printf("Successfully opened for reading: fd=%d\n", fd_read);
        close(fd_read);
    }
    
    // Try to open the FIFO for writing in non-blocking mode
    printf("Trying to open FIFO for writing (non-blocking)...\n");
    int fd_write = open(fifo_path, O_WRONLY | O_NONBLOCK);
    if (fd_write < 0) {
        printf("Failed to open for writing: %s\n", strerror(errno));
    } else {
        printf("Successfully opened for writing: fd=%d\n", fd_write);
        
        // Write a test message
        const char *test_msg = "FIX-EOF-TEST\n";
        ssize_t bytes_written = write(fd_write, test_msg, strlen(test_msg));
        if (bytes_written < 0) {
            printf("Failed to write test message: %s\n", strerror(errno));
        } else {
            printf("Wrote %ld bytes to the FIFO\n", bytes_written);
        }
        
        close(fd_write);
    }
    
    printf("\nFIFO diagnostics complete. To fix EOF issues:\n");
    printf("1. Make sure both reader and writer are connected\n");
    printf("2. Open the write end first in blocking mode\n");
    printf("3. Then open the read end\n");
    printf("4. For non-blocking reads, handle EAGAIN errors properly\n");
    
    return 0;
}
