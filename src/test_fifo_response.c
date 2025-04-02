#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <errno.h>
#include <sys/select.h>

/**
 * This utility simulates a balance query and waits for a response to test 
 * if the communication flow is working correctly.
 * 
 * Usage: ./test_fifo_response <user_slot> [account_number]
 *   
 * Example: ./test_fifo_response 0 1001
 */

#define FIFO_BASE_PATH "/tmp/banco_fifo_"
#define BUFFER_SIZE 1024

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <user_slot> [account_number]\n", argv[0]);
        return 1;
    }
    
    // Get parameters
    int user_slot = atoi(argv[1]);
    int account = 1001;  // Default account
    
    if (argc >= 3) {
        account = atoi(argv[2]);
    }
    
    // Construct FIFO paths
    char fifo_to_bank[100], fifo_from_bank[100];
    sprintf(fifo_to_bank, "%s%d_from_user", FIFO_BASE_PATH, user_slot);   // To bank
    sprintf(fifo_from_bank, "%s%d_to_user", FIFO_BASE_PATH, user_slot);   // From bank
    
    printf("Testing FIFO communication with:\n");
    printf("  - User slot: %d\n", user_slot);
    printf("  - Account: %d\n", account);
    printf("  - FIFO to bank: %s\n", fifo_to_bank);
    printf("  - FIFO from bank: %s\n", fifo_from_bank);
    
    // Check if FIFOs exist
    struct stat st;
    if (stat(fifo_to_bank, &st) == -1) {
        fprintf(stderr, "Error: FIFO %s does not exist\n", fifo_to_bank);
        return 1;
    }
    
    if (stat(fifo_from_bank, &st) == -1) {
        fprintf(stderr, "Error: FIFO %s does not exist\n", fifo_from_bank);
        return 1;
    }
    
    printf("\n[1/4] Opening FIFO to bank for writing...\n");
    int fd_to_bank = open(fifo_to_bank, O_WRONLY);
    if (fd_to_bank < 0) {
        perror("Failed to open FIFO to bank");
        return 1;
    }
    printf("✓ Successfully opened FIFO to bank (fd=%d)\n", fd_to_bank);
    
    printf("\n[2/4] Opening FIFO from bank for reading...\n");
    // First try non-blocking to see if there's already data
    int fd_from_bank = open(fifo_from_bank, O_RDONLY | O_NONBLOCK);
    if (fd_from_bank < 0) {
        perror("Failed to open FIFO from bank");
        close(fd_to_bank);
        return 1;
    }
    printf("✓ Successfully opened FIFO from bank (fd=%d)\n", fd_from_bank);
    
    // Flush any existing data
    char temp_buffer[BUFFER_SIZE];
    while (read(fd_from_bank, temp_buffer, sizeof(temp_buffer)) > 0) {
        printf("  - Flushed existing data from FIFO\n");
    }
    
    // Change to blocking mode
    int flags = fcntl(fd_from_bank, F_GETFL);
    fcntl(fd_from_bank, F_SETFL, flags & ~O_NONBLOCK);
    
    // Prepare and send query
    char query[BUFFER_SIZE];
    sprintf(query, "Consulta de saldo en la cuenta %d\n", account);

    printf("\n[3/4] Sending balance query: %s", query);
    ssize_t bytes_written = write(fd_to_bank, query, strlen(query));
    if (bytes_written < 0) {
        perror("Failed to write query to bank");
        close(fd_to_bank);
        close(fd_from_bank);
        return 1;
    }
    printf("✓ Successfully sent %ld bytes\n", bytes_written);
    
    // Wait for response with timeout
    printf("\n[4/4] Waiting for response (10 second timeout)...\n");
    fd_set readfds;
    struct timeval tv;
    FD_ZERO(&readfds);
    FD_SET(fd_from_bank, &readfds);
    tv.tv_sec = 10;
    tv.tv_usec = 0;
    
    int ret = select(fd_from_bank + 1, &readfds, NULL, NULL, &tv);
    
    if (ret == -1) {
        perror("Error in select()");
    } else if (ret == 0) {
        printf("❌ Timeout waiting for response!\n");
    } else {
        // Response available
        char response[BUFFER_SIZE] = {0};
        ssize_t bytes_read = read(fd_from_bank, response, sizeof(response) - 1);
        
        if (bytes_read > 0) {
            response[bytes_read] = '\0'; // Ensure null termination
            printf("✓ Received response (%ld bytes): '%s'\n", bytes_read, response);
        } else if (bytes_read == 0) {
            printf("❌ EOF received - bank closed the connection without sending data\n");
        } else {
            perror("Error reading from FIFO");
        }
    }
    
    // Clean up
    close(fd_to_bank);
    close(fd_from_bank);
    
    printf("\nTest completed.\n");
    return 0;
}
