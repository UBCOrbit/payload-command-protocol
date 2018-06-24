#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <unistd.h>

#include <dirent.h>

#include "sha256_utils.h"
#include "commands.h"

#define DOWNLOAD_FILEPATH   "download-filepath"
#define DOWNLOAD_FILEOFFSET "download-fileoffset"

#define UPLOAD_FILEMETA  "upload-filemeta"
#define UPLOAD_RECEIVED  "upload-received"

#define TEST_IMAGE_DIR_PATH = "~/orbit/fire-detection/images"

long fileLength(FILE *fp)
{
    if (fseek(fp, 0, SEEK_END) == -1) {
        perror("Error seeking file end");
        return -1;
    }

    long len = ftell(fp);
    rewind(fp);

    return len;
}

uint8_t* readFile(FILE *fp, size_t *size)
{
    long len = fileLength(fp);
    if (len == -1)
        return NULL;

    uint8_t *data = malloc(len);
    size_t read = fread(data, 1, len, fp);
    if (read != len) {
        printf("Error reading file\n");
        return NULL;
    }

    *size = len;
    return data;
}

// Poweroff Payload
Message poweroff(const uint8_t *buf, size_t buflen)
{
    if (buflen != 0)
        return EMPTY_MESSAGE(ERROR_INVALID_PAYLOAD);

    // Systemd should detect this exit and poweroff the system as a result
    exit(EXIT_SUCCESS);
}

// Create data for sending a file and return file shasum
// TODO: maybe require paths to be absolute?
Message startDownload(const uint8_t *buf, size_t buflen)
{
    if (buflen == 0)
        return EMPTY_MESSAGE(ERROR_INVALID_PAYLOAD);

    // download payload format
    //   n bytes for path

    // if there are already download metadata files then return an error code
    if (access(DOWNLOAD_FILEPATH, F_OK) == 0 || access(DOWNLOAD_FILEOFFSET, F_OK) == 0)
        return EMPTY_MESSAGE(ERROR_ALREADY_DOWNLOADING);

    // Read file path from buffer
    char *path = malloc(buflen + 1);
    memcpy(path, buf, buflen);
    path[buflen] = '\0';

    // check that the requested file exists
    if (access(path, F_OK) != 0) {
        free(path);
        return EMPTY_MESSAGE(ERROR_FILE_DOESNT_EXIST);
    }

    // Read in file and generate metadata
    FILE *fp = fopen(path, "r");
    if (fp == NULL) {
        free(path);
        return EMPTY_MESSAGE(ERROR_OPENING_FILE);
    }

    size_t fileLen;
    uint8_t *fileData;

    fileData = readFile(fp, &fileLen);
    if (fileData == NULL) {
        free(path);
        fclose(fp);
        return EMPTY_MESSAGE(ERROR_READING_FILE);
    }

    fclose(fp);

    uint8_t shaSum[32];
    sha256calc(fileData, fileLen, shaSum);

    free(fileData);

    // Create file transfer metadata files
    // Open both download meta and download packet number, but if either don't open, io error
    FILE *downMeta, *downOffset;

    downMeta = fopen(DOWNLOAD_FILEPATH, "w");
    if (downMeta == NULL) {
        free(path);
        return EMPTY_MESSAGE(ERROR_OPENING_FILE);
    }

    downOffset = fopen(DOWNLOAD_FILEOFFSET, "w");
    if (downOffset == NULL) {
        free(path);
        fclose(downMeta);
        return EMPTY_MESSAGE(ERROR_OPENING_FILE);
    }

    // write path to downMeta and zero to downPacket
    if (fputs(path, downMeta) == EOF) {
        free(path);
        fclose(downMeta);
        fclose(downOffset);
        return EMPTY_MESSAGE(ERROR_WRITING_FILE);
    }

    free(path);

    uint64_t offset = 0;
    if (fwrite(&offset, 8, 1, downOffset) != 1) {
        fclose(downMeta);
        fclose(downOffset);
        return EMPTY_MESSAGE(ERROR_WRITING_FILE);
    }

    fclose(downMeta);
    fclose(downOffset);

    // create and return the formatted reply
    // Download success reply format
    //   32 bytes for sha256sum

    Message m;
    m.code = SUCCESS;
    m.payloadLen = 32;
    m.payload = malloc(32);

    memcpy(m.payload, shaSum, 32);

    return m;
}

// Create data for receiving a file
Message startUpload(const uint8_t *buf, size_t buflen)
{
    if (buflen != 32)
        return EMPTY_MESSAGE(ERROR_INVALID_PAYLOAD);

    // Upload payload format
    //   32 bytes for sha256sum

    // If any of the upload files exist, return already uploading
    if (access(UPLOAD_FILEMETA, F_OK) == 0 || access(UPLOAD_RECEIVED, F_OK) == 0)
        return EMPTY_MESSAGE(ERROR_ALREADY_UPLOADING);

    // Open upload meta, upload packet number, and received data file, if either don't return io error
    FILE *upMeta, *upReceived;

    upMeta = fopen(UPLOAD_FILEMETA, "w");
    if (upMeta == NULL)
        return EMPTY_MESSAGE(ERROR_OPENING_FILE);

    upReceived = fopen(UPLOAD_RECEIVED, "w");
    if (upReceived == NULL) {
        fclose(upMeta);
        return EMPTY_MESSAGE(ERROR_OPENING_FILE);
    }

    // write shasum to the upload meta file
    if (fwrite(buf, 32, 1, upMeta) != 1) {
        fclose(upMeta);
        fclose(upReceived);
        return EMPTY_MESSAGE(ERROR_WRITING_FILE);
    }

    fclose(upMeta);
    fclose(upReceived);

    return EMPTY_MESSAGE(SUCCESS);
}

// Send a packet to CDH
Message requestPacket(const uint8_t *buf, size_t buflen)
{
    if (buflen != 0)
        return EMPTY_MESSAGE(ERROR_INVALID_PAYLOAD);

    // check if all the download metadata files exist
    if (access(DOWNLOAD_FILEPATH, F_OK) != 0 || access(DOWNLOAD_FILEOFFSET, F_OK) != 0)
        return EMPTY_MESSAGE(ERROR_NOT_DOWNLOADING);

    // Open both filepath and fileoffset, and verify that they're open
    FILE *downMeta, *downOffset;

    downMeta = fopen(DOWNLOAD_FILEPATH, "r");
    if (downMeta == NULL)
        return EMPTY_MESSAGE(ERROR_OPENING_FILE);

    downOffset = fopen(DOWNLOAD_FILEOFFSET, "r+");
    if (downOffset == NULL) {
        fclose(downMeta);
        return EMPTY_MESSAGE(ERROR_OPENING_FILE);
    }

    // Read path from filepath and offset from fileoffset
    long pathlen = fileLength(downMeta);
    if (pathlen == -1) {
        fclose(downMeta);
        fclose(downOffset);
        return EMPTY_MESSAGE(ERROR_SEEKING_FILE);
    }

    char *path = malloc(pathlen + 1);

    if (fread(path, 1, pathlen, downMeta) != pathlen) {
        fclose(downMeta);
        fclose(downOffset);
        free(path);
        return EMPTY_MESSAGE(ERROR_READING_FILE);
    }

    path[pathlen] = '\0';

    uint64_t offset;
    if (fread(&offset, 8, 1, downOffset) != 1) {
        fclose(downMeta);
        fclose(downOffset);
        free(path);
        return EMPTY_MESSAGE(ERROR_READING_FILE);
    }

    fclose(downMeta);

    // Do a sanity check that the file exists
    if (access(path, F_OK) != 0) {
        fclose(downOffset);
        free(path);
        return EMPTY_MESSAGE(ERROR_FILE_DOESNT_EXIST);
    }

    // Open the download file
    // If it wasn't opened return a file io error
    FILE *downFile = fopen(path, "r");
    if (downFile == NULL) {
        fclose(downOffset);
        free(path);
        return EMPTY_MESSAGE(ERROR_OPENING_FILE);
    }

    free(path);

    // Check that the offset is less than the file length
    long filelen = fileLength(downFile);
    if (filelen == -1) {
        fclose(downOffset);
        fclose(downFile);
        return EMPTY_MESSAGE(ERROR_SEEKING_FILE);
    }

    // If the offset is equal to the file length, the file has been fully transferred
    // remove the download metadata
    if (filelen <= offset) {
        fclose(downOffset);
        fclose(downFile);
        if (remove(DOWNLOAD_FILEPATH) == -1) {
            perror("remove");
            return EMPTY_MESSAGE(ERROR_REMOVING_FILE);
        }
        if (remove(DOWNLOAD_FILEOFFSET) == -1) {
            perror("remove");
            return EMPTY_MESSAGE(ERROR_REMOVING_FILE);
        }
        return EMPTY_MESSAGE(ERROR_DOWNLOAD_OVER);
    }

    // calculate the packet length
    uint16_t packetlen = filelen - offset > PACKET_SIZE ? PACKET_SIZE : filelen - offset;

    Message m;
    m.code = SUCCESS;
    m.payloadLen = packetlen;
    m.payload = malloc(packetlen);

    // Read the next packet data from the file
    if (fseek(downFile, offset, SEEK_SET) == -1) {
        fclose(downOffset);
        fclose(downFile);
        free(m.payload);
        return EMPTY_MESSAGE(ERROR_SEEKING_FILE);
    }

    if (fread(m.payload, 1, packetlen, downFile) != packetlen) {
        fclose(downOffset);
        fclose(downFile);
        free(m.payload);
        return EMPTY_MESSAGE(ERROR_READING_FILE);
    }

    // Update the offset file
    offset += packetlen;
    rewind(downOffset);
    if (fwrite(&offset, 8, 1, downOffset) != 1) {
        fclose(downOffset);
        fclose(downFile);
        free(m.payload);
        return EMPTY_MESSAGE(ERROR_WRITING_FILE);
    }

    fclose(downOffset);
    fclose(downFile);

    return m;
}

// Receive a packet from CDH
Message sendPacket(const uint8_t *buf, size_t buflen)
{
    if (buflen == 0)
        return EMPTY_MESSAGE(ERROR_INVALID_PAYLOAD);

    // packet payload format
    //   n bytes for raw data

    // check if all the upload metadata files exist
    if (access(UPLOAD_FILEMETA, F_OK) != 0 || access(UPLOAD_RECEIVED, F_OK) != 0)
        return EMPTY_MESSAGE(ERROR_NOT_UPLOADING);

    FILE *upReceived = fopen(UPLOAD_RECEIVED, "a");
    if (upReceived == NULL)
        return EMPTY_MESSAGE(ERROR_OPENING_FILE);

    // write packet data to the receiving file
    if (fwrite(buf, 1, buflen, upReceived) != buflen) {
        fclose(upReceived);
        return EMPTY_MESSAGE(ERROR_WRITING_FILE);
    }

    fclose(upReceived);

    return EMPTY_MESSAGE(SUCCESS);
}

// Erase upload packet data (if any)
Message cancelUpload(const uint8_t *buf, size_t buflen)
{
    if (buflen != 0)
        return EMPTY_MESSAGE(ERROR_INVALID_PAYLOAD);

    // Remove the upload files
    if (remove(UPLOAD_FILEMETA) == -1) {
        perror("remove");
        return EMPTY_MESSAGE(ERROR_REMOVING_FILE);
    }
    if (remove(UPLOAD_RECEIVED) == -1) {
        perror("remove");
        return EMPTY_MESSAGE(ERROR_REMOVING_FILE);
    }
    return EMPTY_MESSAGE(SUCCESS);
}

// Erase download packet data (if any)
Message cancelDownload(const uint8_t *buf, size_t buflen)
{
    if (buflen != 0)
        return EMPTY_MESSAGE(ERROR_INVALID_PAYLOAD);

    // Remove the download files
    if (remove(DOWNLOAD_FILEPATH) == -1) {
        perror("remove");
        return EMPTY_MESSAGE(ERROR_REMOVING_FILE);
    }
    if (remove(DOWNLOAD_FILEOFFSET) == -1) {
        perror("remove");
        return EMPTY_MESSAGE(ERROR_REMOVING_FILE);
    }
    return EMPTY_MESSAGE(SUCCESS);
}

// Verify the upload file integrity, move the file, and remove upload metadata
// TODO: require paths to be absolute?
Message finalizeUpload(const uint8_t *buf, size_t buflen)
{
    if (buflen == 0)
        return EMPTY_MESSAGE(ERROR_INVALID_PAYLOAD);

    // finalize upload payload format
    //   n bytes for file path

    // verify that a file is being transferred
    if (access(UPLOAD_FILEMETA, F_OK) != 0 || access(UPLOAD_RECEIVED, F_OK) != 0)
        return EMPTY_MESSAGE(ERROR_NOT_UPLOADING);

    // open uploading meta files
    FILE *upMeta, *upReceived;

    upMeta = fopen(UPLOAD_FILEMETA, "r");
    if (upMeta == NULL)
        return EMPTY_MESSAGE(ERROR_OPENING_FILE);

    upReceived = fopen(UPLOAD_RECEIVED, "r");
    if (upReceived == NULL) {
        fclose(upMeta);
        return EMPTY_MESSAGE(ERROR_OPENING_FILE);
    }

    // load file and calculate sha256sum
    size_t fileLen;
    uint8_t *fileData;

    fileData = readFile(upReceived, &fileLen);
    if (fileData == NULL) {
        fclose(upMeta);
        fclose(upReceived);
        return EMPTY_MESSAGE(ERROR_READING_FILE);
    }

    fclose(upReceived);

    uint8_t shaSum[32];
    sha256calc(fileData, fileLen, shaSum);

    free(fileData);

    // read in the given shasum
    uint8_t shaSumGiven[32];
    if (fread(shaSumGiven, 32, 1, upMeta) != 1) {
        fclose(upMeta);
        return EMPTY_MESSAGE(ERROR_READING_FILE);
    }
    fclose(upMeta);

    // verify the calculated sum matches the given sum
    if (!sha256cmp(shaSum, shaSumGiven))
        return EMPTY_MESSAGE(ERROR_SHASUM_MISMATCH);

    // Rename the upload temp file
    // TODO: log errors if any occur
    char *path = malloc(buflen + 1);
    memcpy(path, buf, buflen);
    path[buflen] = '\0';

    if (rename(UPLOAD_RECEIVED, path) == -1) {
        perror("rename");
        free(path);
        return EMPTY_MESSAGE(ERROR_RENAMING_FILE);
    }

    free(path);

    // Remove upload file metadata
    if (remove(UPLOAD_FILEMETA) == -1) {
        perror("remove");
        return EMPTY_MESSAGE(ERROR_REMOVING_FILE);
    }

    return EMPTY_MESSAGE(SUCCESS);
}

// Take a photo at the given time
Message takePhoto(const uint8_t *buf, size_t buflen)
{
    // TODO: I cannot implement this yet
    // TODO: make sure to include payload bounds checking
    if (buflen != 16) {
        return EMPTY_MESSAGE(ERROR_INVALID_PAYLOAD);
    }

    char *time = malloc(8);
    memcpy(time, buf, 8);
    
    char *id = malloc(8);
    memcpy(time, buf + 8, 8);

    int fileCount = 0;
    DIR *dirp;
    struct dirent *entry;
    
    dirp = opendir(TEST_IMAGE_DIR_PATH);
    while ((entry = readdir(dirp) != NULL) {
        if (entry->d_type == DT_REG) {
            fileCount++;
        }
    }
    closedir(dirp);

    randomImageNum = rand() % (fileCount + 1);
    
    char *selectedFile;
    fileCount = 0;
    dirp = opendir(TEST_IMGAE_DIR_PATH);
    while ((entry = readdir(dirp) != NULL) {
        if (entry->d_type == DT_REG) {
            if (fileCount == randomImageNum) {
               selectedFile = entry->d_name;
               break;
            }
            fileCount++;
        }
    }

    
    return EMPTY_MESSAGE(SUCCESS);
}

// Execute the given executable file path
Message executeCommand(const uint8_t *buf, size_t buflen)
{
    if (buflen == 0)
        return EMPTY_MESSAGE(ERROR_INVALID_PAYLOAD);

    // execute command payload format
    //   n bytes for command string

    char *cmd = malloc(buflen + 1);
    memcpy(cmd, buf, buflen);
    cmd[buflen] = '\0';

    int ret = system(cmd);
    if (ret == -1)
        // TODO: Other, more meaningful error data?
        return EMPTY_MESSAGE(ERROR_SH_FAILURE);
    else {
        Message m;
        m.code = SUCCESS;
        m.payloadLen = 1;

        uint8_t *status = malloc(1);
        *status = WEXITSTATUS(ret);
        m.payload = status;

        return m;
    }
}
