#ifndef commands_h_INCLUDED
#define commands_h_INCLUDED

#include <stdint.h>
#include <stdlib.h>

// 2^15 bytes, 32 kb
#define PACKET_SIZE 0x8000

#define MIN_COMMAND_VAL 1
#define MAX_COMMAND_VAL 7

#define START_DOWNLOAD  1
#define START_UPLOAD    2
#define REQUEST_PACKET  3
#define SEND_PACKET     4
#define CANCEL_UPLOAD   5
#define CANCEL_DOWNLOAD 6
#define FINALIZE_UPLOAD 7

#define SUCCESS                   0
#define ERROR_FILE_IO             1
#define ERROR_FILE_DOESNT_EXIST   2
#define ERROR_ALREADY_DOWNLOADING 3
#define ERROR_ALREADY_UPLOADING   4
#define ERROR_NOT_DOWNLOADING     5
#define ERROR_NOT_UPLOADING       6
#define ERROR_DOWNLOAD_OVER       7
#define ERROR_SHASUM_MISMATCH     8
#define ERROR_INVALID_COMMAND     9

typedef struct {
    uint8_t status;
    uint16_t payloadLen;
    uint8_t *payload;
} Reply;

#define EMPTY_REPLY(s) (Reply){s,0,NULL}

Reply startDownload(const uint8_t *, size_t);
Reply startUpload(const uint8_t *, size_t);
Reply requestPacket(const uint8_t *, size_t);
Reply sendPacket(const uint8_t *, size_t);
Reply cancelUpload(const uint8_t *, size_t);
Reply cancelDownload(const uint8_t *, size_t);
Reply finalizeUpload(const uint8_t *, size_t);

static Reply (*const commands[])(const uint8_t *, size_t) = {
    NULL,
    startDownload,
    startUpload,
    requestPacket,
    sendPacket,
    cancelUpload,
    cancelDownload,
    finalizeUpload
};

#endif // commands_h_INCLUDED

