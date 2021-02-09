#include "buffer.h"
#include <pthread.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>

struct Buffer {
    void** arr;
    size_t size;
    size_t r_idx;
    size_t w_idx;
    bool killed;
    size_t howManyIn;
    pthread_mutex_t mutex;
    pthread_cond_t condFull;
    pthread_cond_t condEmpty;
};

void kill_buffer(buffer_t buffer){
    //printf("killing\n");
    if(buffer == NULL)
        return;
    pthread_mutex_lock(&buffer->mutex);
    if(buffer->killed){
        pthread_mutex_unlock(&buffer->mutex);
        return;
    }
    buffer->killed = true;
    pthread_mutex_unlock(&buffer->mutex);
    pthread_cond_broadcast(&buffer->condEmpty);
    pthread_cond_broadcast(&buffer->condFull);
}

void destroy_buffer(buffer_t* buffer){
    if(*buffer == NULL)
        return;
    if((*buffer)->arr != NULL)
        free((*buffer)->arr);

    pthread_mutex_destroy(&(*buffer)->mutex);
    pthread_cond_destroy(&(*buffer)->condFull);
    pthread_cond_destroy(&(*buffer)->condEmpty);
    free(*buffer);
    *buffer = NULL;
}

int create_buffer(buffer_t* buffer, size_t size){
    *buffer = malloc(sizeof(Buffer));
    if(*buffer == NULL)
        return -1;
    (*buffer)->arr = malloc(sizeof(void*) * size);
    if((*buffer)->arr == NULL){
        free(*buffer);
        *buffer = NULL;
        return -2;
    }
    (*buffer)->size = size;
    (*buffer)->r_idx = 0;
    (*buffer)->w_idx = 0;
    (*buffer)->killed = false;
    (*buffer)->howManyIn = 0;

    int success = 0;
    success += pthread_mutex_init(&(*buffer)->mutex, NULL);
    success += pthread_cond_init(&(*buffer)->condEmpty, NULL);
    success += pthread_cond_init(&(*buffer)->condFull, NULL);
    if(success < 0) {
        destroy_buffer(buffer);
        return -3;
    }
    return 0;
}


void buffer_put(buffer_t buffer, void* thing){
    pthread_mutex_lock(&buffer->mutex);
    while(buffer->howManyIn == buffer->size && !buffer->killed){
        pthread_cond_wait(&buffer->condFull,&buffer->mutex);
    }

    if(buffer->killed){
        pthread_mutex_unlock(&buffer->mutex);
        free(thing);
        return;
    }

    buffer->arr[buffer->w_idx] = thing;
    buffer->w_idx++;
    buffer->w_idx%=buffer->size;
    buffer->howManyIn++;

    pthread_mutex_unlock(&buffer->mutex);
    pthread_cond_signal(&buffer->condEmpty);
}

void* buffer_take(buffer_t buffer){
    pthread_mutex_lock(&buffer->mutex);
    while(buffer->howManyIn == 0 && !buffer->killed){
        pthread_cond_wait(&buffer->condEmpty, &buffer->mutex);
    }
    if(buffer->howManyIn == 0){
        pthread_mutex_unlock(&buffer->mutex);
        return NULL;
    }

    void* retVal = buffer->arr[buffer->r_idx];
    buffer->r_idx++;
    buffer->r_idx%=buffer->size;
    buffer->howManyIn--;

    pthread_mutex_unlock(&buffer->mutex);
    pthread_cond_signal(&buffer->condFull);

    return retVal;
}

size_t buffer_size(buffer_t buffer){
    return buffer->howManyIn;
}

void buffer_print(buffer_t buffer) {
    size_t start = buffer->r_idx;
    size_t finish = buffer->w_idx;
    printf("start\n");
    while(start != finish){
        printf("%d, ", *(int*)buffer->arr[start]);
        start++;
        start%=buffer->size;
    }
    printf("end\n");
}