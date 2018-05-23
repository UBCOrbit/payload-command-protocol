#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <fcntl.h>
#include <unistd.h>

#include "commands.h"

#define SERIAL_DEVICE "/dev/ttyUSB0"

/*
 * Justification for *OrDie functions:
 * If read or write operations on the uart device fail
 * there's nothing we can do to recover from almost all
 * of the failures, so doing a complete system restart is
 * most likely the safest thing to do.
 */

// TODO: CALL TERMIOS TO CONFIGURE TTY PROPERLY

void readAllOrDie(int fd, void *buf, size_t len)
{
    size_t offset = 0;
    ssize_t result;
    while (offset < len) {
        printf("    Waiting for data...\r");
        fflush(stdout);
        result = read(fd, buf + offset, len - offset);
        if (result < 0) {
            perror("Error reading from file descriptor");
            exit(EXIT_FAILURE);
        }
        offset += result;

        // Debug info
        printf("    Received: %ld out of %ld bytes.\n", offset, len);
    }
}

void writeAllOrDie(int fd, const void *buf, size_t len)
{
    size_t offset = 0;
    ssize_t result;
    while (offset < len) {
        result = write(fd, buf + offset, len - offset);
        if (result < 0) {
            perror("Error writing to file descriptor");
            exit(EXIT_FAILURE);
        }
        offset += result;
    }
}

void writeMessage(int fd, const Message m)
{
    // Debug info
    printf("Sending  reply:   %-19s with %8u bytes of data\n", reply_strs[m.code], m.payloadLen);

    uint8_t outHeader[3];
    outHeader[0] = m.code;
    memcpy(outHeader + 1, &m.payloadLen, 2);

    writeAllOrDie(fd, outHeader, sizeof(outHeader));
    if (m.payload != NULL)
        writeAllOrDie(fd, m.payload, m.payloadLen);
}

Message readMessage(int fd)
{
    printf("\nSTART Message Read\n\n");
    printf("    ---- HEADER ----\n");

    uint8_t inHeader[3];
    readAllOrDie(fd, inHeader, sizeof(inHeader));

    printf("    Message header received.\n\n");

    Message m;
    m.code = inHeader[0];
    memcpy(&m.payloadLen, inHeader + 1, 2);

    printf("    Command: %-19s. Payload length: %8u bytes.\n\n", command_strs[m.code], m.payloadLen);

    printf("    ---- PAYLOAD ----\n");

    m.payload = NULL;
    if (m.payloadLen > 0) {
        m.payload = malloc(m.payloadLen);
        readAllOrDie(fd, m.payload, m.payloadLen);
    }

    printf("    Message payload received.\n\n");

    // Debug info
    printf("    ---- SUMMARY ----\n");
    printf("    Received command: %-19s with %8u bytes of data\n", command_strs[m.code], m.payloadLen);
    printf("\nEND Message Read\n\n");

    return m;
}

int main()
{
    // Open the serial device
    printf("Opening serial device...");
    int serialfd = open(SERIAL_DEVICE, O_RDWR | O_NOCTTY | O_SYNC);
    if (serialfd < 0) {
        perror("Error opening serial device " SERIAL_DEVICE);
        // If the file descriptor failed to open, there's nothing we can do besides exit
        exit(EXIT_FAILURE);
    }
    printf("Done.\n");

    // Enter an infinite loop listening for and responding to messages
    while (true) {
        // Read message from the serial line
        Message m = readMessage(serialfd);

        // Check that the received command is a valid one
        // Evaluate the command
        Message reply;
        if (m.code < MIN_COMMAND_VAL || m.code > MAX_COMMAND_VAL)
            reply = EMPTY_MESSAGE(ERROR_INVALID_COMMAND);
        else
            reply = commands[m.code](m.payload, m.payloadLen);

        if (m.payload != NULL)
            free(m.payload);

        // Write out the reply message
        writeMessage(serialfd, reply);

        if (reply.payload != NULL)
            free(reply.payload);
    }
}
