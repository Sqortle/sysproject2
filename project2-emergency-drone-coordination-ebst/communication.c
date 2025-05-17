#include "headers/communication.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <errno.h>

#define BUFFER_SIZE 4096

void send_json(int sock, struct json_object *jobj) {
    if (!jobj) {
        printf("Error: NULL json object passed to send_json\n");
        return;
    }

    // Check if socket is valid
    int error = 0;
    socklen_t optlen = sizeof(error);
    if (getsockopt(sock, SOL_SOCKET, SO_ERROR, &error, &optlen) < 0) {
        printf("Error checking socket state: %s (errno: %d)\n", strerror(errno), errno);
        return;
    }
    if (error != 0) {
        printf("Socket is in error state: %s (errno: %d)\n", strerror(error), error);
        return;
    }

    const char *json_str = json_object_to_json_string_ext(jobj, JSON_C_TO_STRING_PLAIN);
    if (!json_str) {
        printf("Error: Failed to convert json object to string\n");
        return;
    }

    size_t len = strlen(json_str);
    char *msg = malloc(len + 2);
    if (!msg) {
        printf("Error: Failed to allocate memory for message\n");
        return;
    }
    
    snprintf(msg, len + 2, "%s\n", json_str);
    printf("Attempting to send message: %s", msg);
    
    ssize_t total_sent = 0;
    size_t remaining = strlen(msg);
    
    while (total_sent < remaining) {
        ssize_t sent = send(sock, msg + total_sent, remaining - total_sent, 0);
        if (sent < 0) {
            printf("Error sending message: %s (errno: %d)\n", strerror(errno), errno);
            free(msg);
            return;
        }
        total_sent += sent;
    }
    
    printf("Successfully sent %zd bytes\n", total_sent);
    free(msg);
}

struct json_object *receive_json(int sock) {
    static char buffer[BUFFER_SIZE] = {0};
    static size_t buf_pos = 0;

    while (1) {
        char *newline = strchr(buffer, '\n');
        if (newline) {
            *newline = '\0';
            printf("Received complete message: %s\n", buffer);
            struct json_object *jobj = json_tokener_parse(buffer);
            if (!jobj) {
                printf("Failed to parse JSON: %s\n", buffer);
            }
            size_t len = newline - buffer + 1;
            memmove(buffer, newline + 1, buf_pos - len);
            buf_pos -= len;
            return jobj;
        }

        int bytes = recv(sock, buffer + buf_pos, BUFFER_SIZE - buf_pos - 1, 0);
        if (bytes <= 0) {
            if (bytes < 0) {
                printf("Error receiving data: %s (errno: %d)\n", strerror(errno), errno);
            }
            if (buf_pos > 0) {
                buffer[buf_pos] = '\0';
                printf("Received partial message: %s\n", buffer);
                struct json_object *jobj = json_tokener_parse(buffer);
                buf_pos = 0;
                return jobj;
            }
            return NULL;
        }
        buf_pos += bytes;
        buffer[buf_pos] = '\0';
        printf("Received %d bytes: %s\n", bytes, buffer + buf_pos - bytes);
    }
} 