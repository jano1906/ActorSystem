#include <stdlib.h>
#include "buffer.h"
#include "threadPool.h"

#include<stdio.h>

struct Request {
    actor_id_t actorId;
};

struct Pool {
    pthread_t* threads;
    size_t size;
    buffer_t requests;
};

int create_request(request_t* request, actor_id_t actorId){
    *request = malloc(sizeof(Request));
    if(*request == NULL)
        return -1;
    (*request)->actorId = actorId;
    return 0;
}

void pool_copy_threads(pool_t pool, pthread_t* arr){
    for(size_t i =0; i<pool->size;i++)
        arr[i] = pool->threads[i];
}

void pool_add_request(pool_t pool, request_t request){
    buffer_put(pool->requests, request);
}

static void exec(request_t request){
    system_execute(request->actorId);
    free(request);
}

static void* startThread(void* pool){
    while(1){
        request_t request = (request_t)buffer_take(((pool_t)pool)->requests);
        if(request == NULL)
            return 0;
        exec(request);
        //printf("threadpool requests after exec:\n");
        //buffer_print(((pool_t)pool)->requests);
    }
}

void destroy_pool(pool_t* pool){
    //printf("pool: %d", *pool);
    if(*pool == NULL)
        return;

    if((*pool)->threads == NULL){
        free(*pool);
        *pool = NULL;
        return;
    }

    if((*pool)->requests == NULL){
        free((*pool)->threads);
        free(*pool);
        *pool= NULL;
        return;
    }

    //buffer_print((*pool)->requests);
    kill_buffer((*pool)->requests);
    //printf("joining threads\n");
    for (size_t i = 0; i < (*pool)->size; i++) {
        if(pthread_join((*pool)->threads[i], NULL) != 0) {exit(1);}
        //printf("%d joined!\n", i);
    }

    destroy_buffer(&(*pool)->requests);
    free((*pool)->threads);
    free(*pool);
    *pool = NULL;
}
int create_pool(pool_t* pool, size_t size, size_t requestLimit){
    *pool = malloc(sizeof(Pool));
    if(*pool == NULL)
        return -1;
    (*pool)->threads = malloc(sizeof(pthread_t) * size);
    if((*pool)->threads == NULL){
        free(*pool);
        *pool = NULL;
        return -2;
    }
    (*pool)->size = size;

    int success = 0;
    success += create_buffer(&(*pool)->requests, requestLimit);
    if(success < 0){
        destroy_pool(pool);
        return -3;
    }
    for(size_t i = 0;i<size;i++){
        if(pthread_create(&(*pool)->threads[i], NULL, &startThread, *pool)) {exit(1);}
    }
    return 0;
}