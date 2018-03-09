#ifndef commands_h_INCLUDED
#define commands_h_INCLUDED

#include <stdint.h>
#include <stdlib.h>

// 8 kb
#define PACKET_SIZE 0x2000

#define MIN_COMMAND_VAL 0
#define MAX_COMMAND_VAL 9

#define POWEROFF        0
#define START_DOWNLOAD  1
#define START_UPLOAD    2
#define REQUEST_PACKET  3
#define SEND_PACKET     4
#define CANCEL_UPLOAD   5
#define CANCEL_DOWNLOAD 6
#define FINALIZE_UPLOAD 7
#define TAKE_PHOTO      8
#define EXECUTE_COMMAND 9

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
#define ERROR_SH_FAILURE          10

typedef struct {
    uint8_t  code;
    uint16_t payloadLen;
    uint8_t *payload;
} Message;

#define EMPTY_MESSAGE(c) (Message){c,0,NULL}

Message poweroff(      const uint8_t *, size_t);
Message startDownload( const uint8_t *, size_t);
Message startUpload(   const uint8_t *, size_t);
Message requestPacket( const uint8_t *, size_t);
Message sendPacket(    const uint8_t *, size_t);
Message cancelUpload(  const uint8_t *, size_t);
Message cancelDownload(const uint8_t *, size_t);
Message finalizeUpload(const uint8_t *, size_t);
Message takePhoto(     const uint8_t *, size_t);
Message executeCommand(const uint8_t *, size_t);

static Message (*const commands[])(const uint8_t *, size_t) = {
    poweroff,
    startDownload,
    startUpload,
    requestPacket,
    sendPacket,
    cancelUpload,
    cancelDownload,
    finalizeUpload,
    takePhoto,
    executeCommand
};

static const char *const command_strs[] = {
    "poweroff",
    "start download",
    "start upload",
    "request packet",
    "send packet",
    "cancel upload",
    "cancel download",
    "finalize upload",
    "take photo",
    "execute command"
};

static const char *const reply_strs[] = {
    "success",
    "file io",
    "file doesn't exist",
    "already downloading",
    "already uploading",
    "not downloading",
    "not uploading",
    "download over",
    "shasum mismatch",
    "invalid command",
    "sh failure"
};

#endif // commands_h_INCLUDED

