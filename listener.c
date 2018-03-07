#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <fcntl.h>
#include <unistd.h>

#include "commands.h"

#define SERIAL_DEVICE "/dev/ttyTHS2"

// TODO: proper error checking and handling here
void readAllOrDie(int fd, void *buf, size_t len)
{
    size_t offset = 0;
    ssize_t result;
    while (offset < len) {
        result = read(fd, buf + offset, len - offset);
        if (result < 0) {
            perror("Error reading from file descriptor");
            exit(-1);
        }
        offset += result;
    }
}

// TODO: proper error handling here
void writeAllOrDie(int fd, const void *buf, size_t len)
{
    size_t offset = 0;
    ssize_t result;
    while (offset < len) {
        result = write(fd, buf + offset, len - offset);
        if (result < 0) {
            perror("Error writing to file descriptor");
            exit(-1);
        }
        offset += result;
    }
}

int main()
{
    // Open the serial device
    // TODO: some form of error handling?
    int serialfd = open(SERIAL_DEVICE, O_RDWR | O_NOCTTY);
    if (serialfd < 0) {
        perror("Error opening serial device " SERIAL_DEVICE);
        exit(-1);
    }

    // Enter an infinite loop listening for and responding to messages
    while (true) {
        // Read message from the serial line
        uint8_t header[3];
        readAllOrDie(serialfd, header, sizeof(header));

        uint8_t command = header[0];
        uint16_t payloadLen;
        memcpy(&payloadLen, header + 1, 2);

        uint8_t *payload = malloc(payloadLen);
        readAllOrDie(serialfd, payload, payloadLen);

        Reply r;

        // Check that the received command is a valid one
        // Evaluate the command
        if (command < MIN_COMMAND_VAL || command > MAX_COMMAND_VAL)
            r = EMPTY_REPLY(ERROR_INVALID_COMMAND);
        else
            r = commands[command](payload, payloadLen);

        free(payload);

        // Generate the output header
        uint8_t outHeader[3];
        outHeader[0] = r.status;
        memcpy(outHeader + 1, &r.payloadLen, 2);

        // write out the header and the payload
        writeAllOrDie(serialfd, outHeader, sizeof(outHeader));

        if (r.payloadLen > 0) {
            writeAllOrDie(serialfd, r.payload, r.payloadLen);
            free(r.payload);
        }
    }
}
