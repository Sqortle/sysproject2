/**
 * @file list.c
 * @author adaskin
 * @brief A simple doubly linked list stored in an array (contiguous memory).
 * @version 0.2
 * @date 2025-05-15
 * @copyright Copyright (c) 2024-2025
 */
#include "headers/list.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

List *create_list(size_t datasize, int capacity) {
    printf("Creating list with datasize=%zu, capacity=%d\n", datasize, capacity);
    List *list = malloc(sizeof(List));
    if (!list) {
        printf("Failed to allocate memory for List structure\n");
        return NULL;
    }
    printf("Allocated List structure at %p\n", (void*)list);
    memset(list, 0, sizeof(List));

    printf("Initializing mutex...\n");
    pthread_mutex_init(&list->lock, NULL);
    list->datasize = datasize;
    list->nodesize = sizeof(Node);  // Node size is now just the structure size
    printf("Node size: %zu bytes\n", list->nodesize);
    
    printf("Allocating memory for nodes...\n");
    list->startaddress = malloc(list->nodesize * capacity);
    if (!list->startaddress) {
        printf("Failed to allocate memory for nodes\n");
        pthread_mutex_destroy(&list->lock);
        free(list);
        return NULL;
    }
    printf("Allocated nodes at %p\n", (void*)list->startaddress);
    
    list->endaddress = list->startaddress + (list->nodesize * capacity);
    printf("End address: %p\n", (void*)list->endaddress);
    
    printf("Zeroing node memory...\n");
    memset(list->startaddress, 0, list->nodesize * capacity);
    
    // Initialize each node's data pointer
    printf("Initializing node data pointers...\n");
    for (int i = 0; i < capacity; i++) {
        Node *node = (Node *)(list->startaddress + (i * list->nodesize));
        node->data = malloc(datasize);
        if (!node->data) {
            printf("Failed to allocate memory for node %d data\n", i);
            // Clean up previously allocated memory
            for (int j = 0; j < i; j++) {
                Node *prev_node = (Node *)(list->startaddress + (j * list->nodesize));
                free(prev_node->data);
            }
            free(list->startaddress);
            pthread_mutex_destroy(&list->lock);
            free(list);
            return NULL;
        }
        memset(node->data, 0, datasize);
    }
    
    list->lastprocessed = (Node *)list->startaddress;
    list->number_of_elements = 0;
    list->capacity = capacity;
    list->free_list = NULL;

    printf("Setting up function pointers...\n");
    list->self = list;
    list->add = add;
    list->removedata = removedata;
    list->removenode = removenode;
    list->pop = pop;
    list->peek = peek;
    list->destroy = destroy;
    list->printlist = printlist;
    list->printlistfromtail = printlistfromtail;
    
    printf("List creation complete\n");
    return list;
}

static Node *find_memcell_fornode(List *list) {
    printf("[find_memcell_fornode] Entered.\n");
    if (!list) {
        printf("Error: list is NULL\n");
        return NULL;
    }
    
    // First check the free list
    if (list->free_list) {
        printf("Found node in free list at %p\n", (void*)list->free_list);
        Node *node = list->free_list;
        list->free_list = node->next;
        return node;
    }

    // If no free nodes in free list, search from last processed position
    Node *current = list->lastprocessed;
    Node *start = (Node *)list->startaddress;
    Node *end = (Node *)((char *)list->startaddress + (list->capacity * list->nodesize));
    int nodes_checked = 0;
    
    // First search from last processed to end
    while (current < end && nodes_checked < list->capacity) {
        if (!current->occupied) {
            printf("Found free node at %p\n", (void*)current);
            return current;
        }
        current = (Node *)((char *)current + list->nodesize);
        nodes_checked++;
    }
    
    // If not found, search from start to last processed
    current = start;
    while (current < list->lastprocessed && nodes_checked < list->capacity) {
        if (!current->occupied) {
            printf("Found free node at %p\n", (void*)current);
            return current;
        }
        current = (Node *)((char *)current + list->nodesize);
        nodes_checked++;
    }
    
    printf("No free nodes found after checking %d nodes\n", nodes_checked);
    return NULL;
}

Node *add(List *list, void *data) {
    printf("\n=== Adding node to list %p ===\n", (void*)list);
    if (!list || !data) {
        printf("Error: list or data is NULL\n");
        return NULL;
    }
    
    pthread_mutex_lock(&list->lock);
    printf("Lock acquired. Current elements: %d, capacity: %d\n", 
           list->number_of_elements, list->capacity);
    
    if (list->number_of_elements >= list->capacity) {
        printf("List is full! (elements: %d, capacity: %d)\n", 
               list->number_of_elements, list->capacity);
        pthread_mutex_unlock(&list->lock);
        return NULL;
    }

    printf("Finding memory cell for new node...\n");
    Node *node = find_memcell_fornode(list);
    if (node == NULL) {
        printf("Failed to find memory cell\n");
        pthread_mutex_unlock(&list->lock);
        return NULL;
    }
    
    printf("Memory cell found at %p\n", (void*)node);
    node->occupied = 1;
    printf("Copying data of size %zu bytes...\n", list->datasize);
    memcpy(node->data, data, list->datasize);
    
    // Initialize node pointers
    node->prev = NULL;
    node->next = NULL;
    
    if (list->head == NULL) {
        // First node in list
        printf("First node in list\n");
        list->head = node;
        list->tail = node;
    } else {
        // Add to head of list
        printf("Adding to head of list (current head: %p)...\n", (void*)list->head);
        node->next = list->head;
        list->head->prev = node;
        list->head = node;
    }
    
    list->lastprocessed = node;
    list->number_of_elements++;
    
    printf("Node added successfully. New element count: %d\n", list->number_of_elements);
    printf("List head: %p, List tail: %p\n", (void*)list->head, (void*)list->tail);
    
    pthread_mutex_unlock(&list->lock);
    return node;
}

int removedata(List *list, void *data) {
    pthread_mutex_lock(&list->lock);
    Node *temp = list->head;
    while (temp != NULL && memcmp(temp->data, data, list->datasize) != 0) {
        temp = temp->next;
    }
    if (temp != NULL) {
        Node *prevnode = temp->prev;
        Node *nextnode = temp->next;
        if (prevnode != NULL) {
            prevnode->next = nextnode;
        }
        if (nextnode != NULL) {
            nextnode->prev = prevnode;
        }
        temp->next = list->free_list;
        temp->prev = NULL;
        temp->occupied = 0;
        list->free_list = temp;
        list->number_of_elements--;
        if (temp == list->tail) {
            list->tail = prevnode;
        }
        if (temp == list->head) {
            list->head = nextnode;
        }
        list->lastprocessed = temp;
        pthread_mutex_unlock(&list->lock);
        return 0;
    }
    pthread_mutex_unlock(&list->lock);
    return 1;
}

void *pop(List *list, void *dest) {
    pthread_mutex_lock(&list->lock);
    if (list->head != NULL) {
        Node *node = list->head;
        if (removenode(list, node) == 0) {
            memcpy(dest, node->data, list->datasize);
            pthread_mutex_unlock(&list->lock);
            return dest;
        }
    }
    pthread_mutex_unlock(&list->lock);
    return NULL;
}

void *peek(List *list) {
    pthread_mutex_lock(&list->lock);
    void *data = (list->head != NULL) ? list->head->data : NULL;
    pthread_mutex_unlock(&list->lock);
    return data;
}

int removenode(List *list, Node *node) {
    pthread_mutex_lock(&list->lock);
    if (node != NULL) {
        Node *prevnode = node->prev;
        Node *nextnode = node->next;
        if (prevnode != NULL) {
            prevnode->next = nextnode;
        }
        if (nextnode != NULL) {
            nextnode->prev = prevnode;
        }
        node->next = list->free_list;
        node->prev = NULL;
        node->occupied = 0;
        list->free_list = node;
        list->number_of_elements--;
        if (node == list->tail) {
            list->tail = prevnode;
        }
        if (node == list->head) {
            list->head = nextnode;
        }
        list->lastprocessed = node;
        pthread_mutex_unlock(&list->lock);
        return 0;
    }
    pthread_mutex_unlock(&list->lock);
    return 1;
}

void destroy(List *list) {
    if (!list) return;
    
    pthread_mutex_lock(&list->lock);
    printf("Destroying list...\n");
    
    // Free all node data
    for (int i = 0; i < list->capacity; i++) {
        Node *node = (Node *)(list->startaddress + (i * list->nodesize));
        if (node->data) {
            printf("Freeing node %d data at %p\n", i, node->data);
            free(node->data);
        }
    }
    
    printf("Freeing node array at %p\n", list->startaddress);
    free(list->startaddress);
    list->startaddress = NULL;
    list->endaddress = NULL;
    list->head = NULL;
    list->tail = NULL;
    list->lastprocessed = NULL;
    list->free_list = NULL;
    list->number_of_elements = 0;
    
    pthread_mutex_unlock(&list->lock);
    pthread_mutex_destroy(&list->lock);
    
    printf("Freeing list structure at %p\n", list);
    free(list);
    printf("List destroyed\n");
}

void printlist(List *list, void (*print)(void *)) {
    pthread_mutex_lock(&list->lock);
    Node *temp = list->head;
    while (temp != NULL) {
        print(temp->data);
        temp = temp->next;
    }
    pthread_mutex_unlock(&list->lock);
}

void printlistfromtail(List *list, void (*print)(void *)) {
    pthread_mutex_lock(&list->lock);
    Node *temp = list->tail;
    while (temp != NULL) {
        print(temp->data);
        temp = temp->prev;
    }
    pthread_mutex_unlock(&list->lock);
}