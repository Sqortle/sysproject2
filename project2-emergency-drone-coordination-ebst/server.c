#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <json-c/json.h>
#include "headers/globals.h"
#include "headers/ai.h"
#include "headers/map.h"
#include "headers/drone.h"
#include "headers/survivor.h"

#define PORT 8080
#define MAX_DRONES 10
#define BUFFER_SIZE 4096

void *handle_drone(void *arg);
void send_json(int sock, struct json_object *jobj);
struct json_object *receive_json(int sock);
void process_handshake(int sock, struct json_object *jobj);
void process_status_update(int sock, struct json_object *jobj);
void process_mission_complete(int sock, struct json_object *jobj);
void process_heartbeat_response(int sock, struct json_object *jobj);

int main() {
    survivors = create_list(sizeof(Survivor), 1000);
    helpedsurvivors = create_list(sizeof(Survivor), 1000);
    drones = create_list(sizeof(Drone), MAX_DRONES);
    init_map(40, 30);

    pthread_t survivor_thread;
    pthread_create(&survivor_thread, NULL, survivor_generator, NULL);

    pthread_t ai_thread;
    pthread_create(&ai_thread, NULL, ai_controller, NULL);

    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in server_addr = {
        .sin_family = AF_INET,
        .sin_addr.s_addr = INADDR_ANY,
        .sin_port = htons(PORT)
    };

    if (bind(server_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, MAX_DRONES) < 0) {
        perror("Listen failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    printf("Server listening on port %d\n", PORT);

    while (1) {
        struct sockaddr_in client_addr;
        socklen_t addr_len = sizeof(client_addr);
        int drone_fd = accept(server_fd, (struct sockaddr*)&client_addr, &addr_len);
        if (drone_fd < 0) {
            perror("Accept failed");
            continue;
        }
        printf("Accepted connection from %s:%d\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));

        pthread_t drone_thread;
        int *drone_fd_ptr = malloc(sizeof(int));
        *drone_fd_ptr = drone_fd;
        pthread_create(&drone_thread, NULL, handle_drone, drone_fd_ptr);
    }

    close(server_fd);
    freemap();
    survivors->destroy(survivors);
    helpedsurvivors->destroy(helpedsurvivors);
    drones->destroy(drones);
    return 0;
}

void *handle_drone(void *arg) {
    int sock = *(int*)arg;
    free(arg);

    while (1) {
        struct json_object *jobj = receive_json(sock);
        if (!jobj) {
            printf("No data received or client disconnected on sock %d\n", sock);
            pthread_mutex_lock(&drones->lock);
            Node *node = drones->head;
            while (node != NULL) {
                Drone *d = (Drone *)node->data;
                if (d->sock == sock) {
                    pthread_mutex_lock(&d->lock);
                    d->status = DISCONNECTED;
                    pthread_mutex_unlock(&d->lock);
                    break;
                }
                node = node->next;
            }
            pthread_mutex_unlock(&drones->lock);
            close(sock);
            break;
        }

        const char *type = json_object_get_string(json_object_object_get(jobj, "type"));
        printf("Received message on sock %d: type=%s\n", sock, type ? type : "NULL");
        if (!type) {
            struct json_object *error = json_object_new_object();
            json_object_object_add(error, "type", json_object_new_string("ERROR"));
            json_object_object_add(error, "code", json_object_new_int(400));
            json_object_object_add(error, "message", json_object_new_string("Missing message type"));
            send_json(sock, error);
            json_object_put(error);
        } else if (strcmp(type, "HANDSHAKE") == 0) {
            process_handshake(sock, jobj);
        } else if (strcmp(type, "STATUS_UPDATE") == 0) {
            process_status_update(sock, jobj);
        } else if (strcmp(type, "MISSION_COMPLETE") == 0) {
            process_mission_complete(sock, jobj);
        } else if (strcmp(type, "HEARTBEAT_RESPONSE") == 0) {
            process_heartbeat_response(sock, jobj);
        } else {
            struct json_object *error = json_object_new_object();
            json_object_object_add(error, "type", json_object_new_string("ERROR"));
            json_object_object_add(error, "code", json_object_new_int(400));
            json_object_object_add(error, "message", json_object_new_string("Invalid message type"));
            send_json(sock, error);
            json_object_put(error);
        }

        json_object_put(jobj);
    }
    return NULL;
}

void send_json(int sock, struct json_object *jobj) {
    const char *json_str = json_object_to_json_string_ext(jobj, JSON_C_TO_STRING_PLAIN);
    size_t len = strlen(json_str);
    char *msg = malloc(len + 2);
    snprintf(msg, len + 2, "%s\n", json_str);
    send(sock, msg, strlen(msg), 0);
    free(msg);
}

struct json_object *receive_json(int sock) {
    static char buffer[BUFFER_SIZE] = {0};
    static size_t buf_pos = 0;

    while (1) {
        // Check for complete message (newline-terminated)
        char *newline = strchr(buffer, '\n');
        if (newline) {
            *newline = '\0';
            struct json_object *jobj = json_tokener_parse(buffer);
            if (!jobj) {
                printf("Failed to parse JSON: %s\n", buffer);
            }
            // Shift remaining data
            size_t len = newline - buffer + 1;
            memmove(buffer, newline + 1, buf_pos - len);
            buf_pos -= len;
            return jobj;
        }

        // Read more data
        int bytes = recv(sock, buffer + buf_pos, BUFFER_SIZE - buf_pos - 1, 0);
        if (bytes <= 0) {
            if (buf_pos > 0) {
                buffer[buf_pos] = '\0';
                struct json_object *jobj = json_tokener_parse(buffer);
                buf_pos = 0;
                return jobj;
            }
            return NULL;
        }
        buf_pos += bytes;
        buffer[buf_pos] = '\0';
        printf("Received %d bytes on sock %d: %s\n", bytes, sock, buffer + buf_pos - bytes);
    }
}

void process_handshake(int sock, struct json_object *jobj) {
    const char *drone_id = json_object_get_string(json_object_object_get(jobj, "drone_id"));
    printf("Processing HANDSHAKE for drone_id=%s\n", drone_id);
    Drone *drone = malloc(sizeof(Drone));
    if (!drone) return;
    memset(drone, 0, sizeof(Drone));
    drone->id = atoi(drone_id + 1); // Extract number from "D1"
    drone->status = IDLE;
    drone->coord = (Coord){rand() % map.width, rand() % map.height};
    drone->target = drone->coord;
    drone->sock = sock;
    pthread_mutex_init(&drone->lock, NULL);

    pthread_mutex_lock(&drones->lock);
    drones->add(drones, drone);
    pthread_mutex_unlock(&drones->lock);

    struct json_object *ack = json_object_new_object();
    json_object_object_add(ack, "type", json_object_new_string("HANDSHAKE_ACK"));
    json_object_object_add(ack, "session_id", json_object_new_string("S123"));
    struct json_object *config = json_object_new_object();
    json_object_object_add(config, "status_update_interval", json_object_new_int(5));
    json_object_object_add(config, "heartbeat_interval", json_object_new_int(10));
    json_object_object_add(ack, "config", config);
    send_json(sock, ack);
    json_object_put(ack);
}

void process_status_update(int sock, struct json_object *jobj) {
    struct json_object *loc = json_object_object_get(jobj, "location");
    int x = json_object_get_int(json_object_object_get(loc, "x"));
    int y = json_object_get_int(json_object_object_get(loc, "y"));
    const char *status_str = json_object_get_string(json_object_object_get(jobj, "status"));
    printf("Processing STATUS_UPDATE: x=%d, y=%d, status=%s\n", x, y, status_str);

    pthread_mutex_lock(&drones->lock);
    Node *node = drones->head;
    while (node != NULL) {
        Drone *d = (Drone *)node->data;
        if (d->sock == sock) {
            pthread_mutex_lock(&d->lock);
            d->coord.x = x;
            d->coord.y = y;
            if (strcmp(status_str, "idle") == 0) d->status = IDLE;
            else if (strcmp(status_str, "busy") == 0) d->status = ON_MISSION;
            d->last_update = *localtime(&(time_t){json_object_get_int64(json_object_object_get(jobj, "timestamp"))});
            pthread_mutex_unlock(&d->lock);
            break;
        }
        node = node->next;
    }
    pthread_mutex_unlock(&drones->lock);
}

void process_mission_complete(int sock, struct json_object *jobj) {
    const char *mission_id = json_object_get_string(json_object_object_get(jobj, "mission_id"));
    printf("Processing MISSION_COMPLETE: mission_id=%s\n", mission_id);

    pthread_mutex_lock(&drones->lock);
    Node *node = drones->head;
    while (node != NULL) {
        Drone *d = (Drone *)node->data;
        if (d->sock == sock) {
            pthread_mutex_lock(&d->lock);
            d->status = IDLE;
            pthread_mutex_unlock(&d->lock);
            break;
        }
        node = node->next;
    }
    pthread_mutex_unlock(&drones->lock);

    pthread_mutex_lock(&survivors->lock);
    Node *snode = survivors->head;
    while (snode != NULL) {
        Survivor *s = (Survivor *)snode->data;
        if (strcmp(s->info, mission_id) == 0) {
            s->status = 1;
            s->helped_time = *localtime(&(time_t){json_object_get_int64(json_object_object_get(jobj, "timestamp"))});
            pthread_mutex_lock(&helpedsurvivors->lock);
            helpedsurvivors->add(helpedsurvivors, s);
            pthread_mutex_unlock(&helpedsurvivors->lock);
            survivors->removedata(survivors, s);
            survivor_cleanup(s);
            break;
        }
        snode = snode->next;
    }
    pthread_mutex_unlock(&survivors->lock);
}

void process_heartbeat_response(int sock, struct json_object *jobj) {
    printf("Processing HEARTBEAT_RESPONSE\n");
    pthread_mutex_lock(&drones->lock);
    Node *node = drones->head;
    while (node != NULL) {
        Drone *d = (Drone *)node->data;
        if (d->sock == sock) {
            pthread_mutex_lock(&d->lock);
            d->last_update = *localtime(&(time_t){json_object_get_int64(json_object_object_get(jobj, "timestamp"))});
            pthread_mutex_unlock(&d->lock);
            break;
        }
        node = node->next;
    }
    pthread_mutex_unlock(&drones->lock);
}