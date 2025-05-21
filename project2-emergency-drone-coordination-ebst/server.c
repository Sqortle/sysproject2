#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <json-c/json.h>
#include <fcntl.h>
#include <errno.h>
#include "headers/globals.h"
#include "headers/ai.h"
#include "headers/map.h"
#include "headers/drone.h"
#include "headers/survivor.h"
#include "headers/communication.h"
#include "headers/view.h"

#define PORT 8080
#define MAX_DRONES 10
#define BUFFER_SIZE 4096

void *handle_drone(void *arg);
void process_handshake(int sock, struct json_object *jobj);
void process_status_update(int sock, struct json_object *jobj);
void process_mission_complete(int sock, struct json_object *jobj);
void process_heartbeat_response(int sock, struct json_object *jobj);
void *heartbeat_thread(void *arg);

// Global mutex for initialization
pthread_mutex_t init_mutex = PTHREAD_MUTEX_INITIALIZER;
int initialized = 0;

int initialize_globals() {
    pthread_mutex_lock(&init_mutex);
    if (initialized) {
        printf("Globals already initialized\n");
        pthread_mutex_unlock(&init_mutex);
        return 0;
    }

    printf("Initializing lists...\n");
    printf("Creating survivors list with capacity 1000...\n");
    survivors = create_list(sizeof(Survivor), 1000);
    if (!survivors) {
        printf("Failed to create survivors list\n");
        pthread_mutex_unlock(&init_mutex);
        return 1;
    }
    printf("Survivors list created successfully at %p\n", (void*)survivors);

    printf("Creating helped survivors list with capacity 1000...\n");
    helpedsurvivors = create_list(sizeof(Survivor), 1000);
    if (!helpedsurvivors) {
        printf("Failed to create helped survivors list\n");
        survivors->destroy(survivors);
        pthread_mutex_unlock(&init_mutex);
        return 1;
    }
    printf("Helped survivors list created successfully at %p\n", (void*)helpedsurvivors);

    printf("Creating drones list with capacity %d...\n", MAX_DRONES);
    drones = create_list(sizeof(Drone), MAX_DRONES);
    if (!drones) {
        printf("Failed to create drones list\n");
        survivors->destroy(survivors);
        helpedsurvivors->destroy(helpedsurvivors);
        pthread_mutex_unlock(&init_mutex);
        return 1;
    }
    printf("Drones list created successfully at %p\n", (void*)drones);

    printf("Initializing map...\n");
    init_map(30, 40);  // height = 30, width = 40
    printf("Map initialized with dimensions: %dx%d\n", map.width, map.height);

    initialized = 1;
    printf("All globals initialized successfully\n");
    pthread_mutex_unlock(&init_mutex);
    return 0;
}

void cleanup_globals() {
    pthread_mutex_lock(&init_mutex);
    if (!initialized) {
        pthread_mutex_unlock(&init_mutex);
        return;
    }

    if (survivors) {
        survivors->destroy(survivors);
        survivors = NULL;
    }
    if (helpedsurvivors) {
        helpedsurvivors->destroy(helpedsurvivors);
        helpedsurvivors = NULL;
    }
    if (drones) {
        drones->destroy(drones);
        drones = NULL;
    }
    freemap();
    initialized = 0;
    pthread_mutex_unlock(&init_mutex);
}

int main() {
    if (initialize_globals() != 0) {
        printf("Failed to initialize globals\n");
        return 1;
    }

    // Initialize SDL and create window
    if (init_sdl_window() != 0) {
        printf("Failed to initialize SDL window\n");
        cleanup_globals();
        return 1;
    }

    // Initialize drones
    initialize_drones();

    // Set running flag before creating threads
    running = 1;

    // Create survivor generator thread
    pthread_t survivor_thread;
    if (pthread_create(&survivor_thread, NULL, survivor_generator, NULL) != 0) {
        printf("Failed to create survivor generator thread\n");
        cleanup_globals();
        return 1;
    }
    printf("Survivor generator thread created\n");

    // Create AI controller thread
    pthread_t ai_thread;
    if (pthread_create(&ai_thread, NULL, ai_controller, NULL) != 0) {
        printf("Failed to create AI controller thread\n");
        cleanup_globals();
        return 1;
    }
    printf("AI controller thread created\n");

    // Create server socket
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("Socket creation failed");
        cleanup_globals();
        return 1;
    }

    // Set socket options
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    // Set up server address
    struct sockaddr_in server_addr = {
        .sin_family = AF_INET,
        .sin_addr.s_addr = INADDR_ANY,
        .sin_port = htons(PORT)
    };

    // Bind socket
    if (bind(server_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind failed");
        close(server_fd);
        cleanup_globals();
        return 1;
    }

    // Listen for connections
    if (listen(server_fd, MAX_DRONES) < 0) {
        perror("Listen failed");
        close(server_fd);
        cleanup_globals();
        return 1;
    }

    printf("Server listening on port %d\n", PORT);

    // Main event loop
    SDL_Event event;
    Uint32 lastDrawTime = SDL_GetTicks();
    const int TARGET_FPS = 60;
    const int FRAME_TIME = 1000 / TARGET_FPS;

    while (running) {
        // Handle SDL events
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT || 
                (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_q)) {
                printf("Received quit event\n");
                running = 0;
                break;
            }
        }

        // Update display at target FPS
        Uint32 currentTime = SDL_GetTicks();
        if (currentTime - lastDrawTime >= FRAME_TIME) {
            if (draw_map() != 0) {
                printf("Error drawing map\n");
                running = 0;
                break;
            }
            lastDrawTime = currentTime;
        }

        // Accept new connections (non-blocking)
        struct sockaddr_in client_addr;
        socklen_t addr_len = sizeof(client_addr);
        int drone_fd = accept(server_fd, (struct sockaddr*)&client_addr, &addr_len);
        if (drone_fd >= 0) {
            printf("Accepted connection from %s:%d\n", 
                   inet_ntoa(client_addr.sin_addr), 
                   ntohs(client_addr.sin_port));
            pthread_t drone_thread;
            int *drone_fd_ptr = malloc(sizeof(int));
            *drone_fd_ptr = drone_fd;
            pthread_create(&drone_thread, NULL, handle_drone, drone_fd_ptr);
        }

        // Small delay to prevent excessive CPU usage
        SDL_Delay(1);  // 1ms delay is enough since we have frame timing
    }

    // Cleanup and exit
    printf("Cleaning up...\n");
    close(server_fd);
    cleanup_sdl();
    cleanup_globals();
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

void process_handshake(int sock, struct json_object *jobj) {
    const char *drone_id = json_object_get_string(json_object_object_get(jobj, "drone_id"));
    printf("Processing HANDSHAKE for drone_id=%s\n", drone_id);
    
    if (!drones) {
        printf("Error: drones list is NULL\n");
        struct json_object *error = json_object_new_object();
        json_object_object_add(error, "type", json_object_new_string("ERROR"));
        json_object_object_add(error, "code", json_object_new_int(500));
        json_object_object_add(error, "message", json_object_new_string("Internal server error: drones list not initialized"));
        send_json(sock, error);
        json_object_put(error);
        return;
    }
    
    printf("Allocating memory for drone...\n");
    Drone *drone = malloc(sizeof(Drone));
    if (!drone) {
        printf("Failed to allocate memory for drone\n");
        struct json_object *error = json_object_new_object();
        json_object_object_add(error, "type", json_object_new_string("ERROR"));
        json_object_object_add(error, "code", json_object_new_int(500));
        json_object_object_add(error, "message", json_object_new_string("Internal server error"));
        send_json(sock, error);
        json_object_put(error);
        return;
    }
    printf("Memory allocated successfully\n");

    printf("Initializing drone data...\n");
    memset(drone, 0, sizeof(Drone));
    drone->id = atoi(drone_id + 1); // Extract number from "D1"
    drone->status = IDLE;
    drone->sock = sock;
    
    // Initialize random starting position
    drone->coord.x = rand() % map.width;
    drone->coord.y = rand() % map.height;
    drone->target = drone->coord;  // Initially target is same as current position
    
    pthread_mutex_init(&drone->lock, NULL);

    printf("Adding drone to list...\n");
    printf("Entering add function...\n");
    drones->add(drones, drone);
    printf("Drone added to list with ID %d at position (%d,%d)\n", 
           drone->id, drone->coord.x, drone->coord.y);

    // Send HANDSHAKE_ACK response
    struct json_object *ack = json_object_new_object();
    json_object_object_add(ack, "type", json_object_new_string("HANDSHAKE_ACK"));
    json_object_object_add(ack, "session_id", json_object_new_string(drone_id));
    
    struct json_object *config = json_object_new_object();
    json_object_object_add(config, "status_update_interval", json_object_new_int(5));
    json_object_object_add(config, "heartbeat_interval", json_object_new_int(10));
    json_object_object_add(ack, "config", config);
    
    send_json(sock, ack);
    printf("Sent HANDSHAKE_ACK to drone %s\n", drone_id);
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

void *heartbeat_thread(void *arg) {
    while (1) {
        sleep(10); // Send heartbeat every 10 seconds
        pthread_mutex_lock(&drones->lock);
        Node *node = drones->head;
        while (node != NULL) {
            Drone *d = (Drone *)node->data;
            if (d->status != DISCONNECTED) {
                struct json_object *heartbeat = json_object_new_object();
                json_object_object_add(heartbeat, "type", json_object_new_string("HEARTBEAT"));
                json_object_object_add(heartbeat, "timestamp", json_object_new_int64(time(NULL)));
                send_json(d->sock, heartbeat);
                printf("Sent HEARTBEAT to drone D%d\n", d->id);
                json_object_put(heartbeat);
            }
            node = node->next;
        }
        pthread_mutex_unlock(&drones->lock);
    }
    return NULL;
}