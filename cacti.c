#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include "actorSystemRegister.h"
#include "cacti.h"
#include "buffer.h"
#include "threadPool.h"
#include <semaphore.h>
#include <signal.h>

#define INTCODE 420

typedef struct Actor {
    void* stateptr;
    buffer_t messages;
    role_t* role;
    bool killed;
    bool is_working;
    pthread_mutex_t mutex;
    sem_t sem_full;
} Actor;
typedef Actor* actor_t;

static bool all_killed = false;
static bool interrupted = false;
static size_t actor_it = 0;
static size_t actor_ctr = 0;
static actor_t actors[CAST_LIMIT] = {NULL};
static pool_t pool;
static pthread_cond_t join_cond = PTHREAD_COND_INITIALIZER;
static pthread_mutex_t system_mutex;

void* sig_thread_fun() {
    sigset_t sigset;
    sigemptyset(&sigset);
    sigaddset(&sigset, SIGINT);
    int sig;
    sigwait(&sigset, &sig);
    if(sig == SIGINT) {
        system_int();
        return 0;
    }
    exit(INTCODE);
}
static pthread_t sig_thread;

actor_id_t actor_id_self() {
    return getThreadActor(pthread_self());
}


static void destroy_actor(actor_id_t actorId){
    if(actors[actorId] == NULL)
        return;
    destroy_buffer(&actors[actorId]->messages);
    pthread_mutex_destroy(&actors[actorId]->mutex);
    sem_destroy(&actors[actorId]->sem_full);
    free(actors[actorId]);
    actors[actorId] = NULL;
}

static int create_actor(actor_id_t* actorId, role_t *const role){
    if(actor_it >= CAST_LIMIT){
        return -1;
    }

    actors[actor_it] = malloc(sizeof(Actor));
    if(actors[actor_it] == NULL)
        return -2;

    pthread_mutexattr_t Attr;
    pthread_mutexattr_init(&Attr);
    pthread_mutexattr_settype(&Attr, PTHREAD_MUTEX_RECURSIVE);

    if(pthread_mutex_init(&actors[actor_it]->mutex, &Attr) != 0) {
        free(actors[actor_it]);
        actors[actor_it] = NULL;
        return -3;
    }
    if(sem_init(&actors[actor_it]->sem_full, 0, ACTOR_QUEUE_LIMIT) != 0) {
        pthread_mutex_destroy(&actors[actor_it]->mutex);
        free(actors[actor_it]);
        actors[actor_it] = NULL;
        return -4;
    }

    if(create_buffer(&actors[actor_it]->messages, ACTOR_QUEUE_LIMIT + 1) != 0){
        destroy_actor(actor_it);
        return -5;
    }

    actors[actor_it]->is_working = false;
    actors[actor_it]->role = role;
    actors[actor_it]->killed = false;
    actors[actor_it]->stateptr = NULL;
    *actorId = actor_it;
    actor_it++;
    actor_ctr++;
    return 0;
}


int send_message(actor_id_t actorId, message_t message) {
    if(interrupted)
        return INTCODE;
    if(message.message_type != MSG_GODIE)
        sem_wait(&actors[actorId]->sem_full);
    if(interrupted) {
        sem_post(&actors[actorId]->sem_full);
        return INTCODE;
    }

    pthread_mutex_lock(&actors[actorId]->mutex);

    if(interrupted) {
        sem_post(&actors[actorId]->sem_full);
        pthread_mutex_unlock(&actors[actorId]->mutex);
        return INTCODE;
    }
    pthread_mutex_lock(&system_mutex);
    if(actors[actorId]->killed) {
        pthread_mutex_unlock(&system_mutex);
        pthread_mutex_unlock(&actors[actorId]->mutex);
        if(message.message_type != MSG_GODIE)
            sem_post(&actors[actorId]->sem_full);
        return 0;
    }

    if((size_t) actorId > actor_it){
        pthread_mutex_unlock(&system_mutex);
        pthread_mutex_unlock(&actors[actorId]->mutex);
        if(message.message_type != MSG_GODIE)
            sem_post(&actors[actorId]->sem_full);
        return -1;
    }
    if(message.message_type == MSG_GODIE) {
        actors[actorId]->killed = true;
    }
    pthread_mutex_unlock(&system_mutex);

    message_t* toSend = malloc(sizeof(message_t));
    if(toSend == NULL) {
        if(message.message_type != MSG_GODIE)
            sem_post(&actors[actorId]->sem_full);
        pthread_mutex_unlock(&actors[actorId]->mutex);
        return -2;
    }

    toSend->message_type = message.message_type;
    toSend->data = message.data;
    toSend->nbytes = message.nbytes;
    buffer_put(actors[actorId]->messages, toSend);

    if(buffer_size(actors[actorId]->messages) == 1 &&
        !actors[actorId]->is_working ) {
        request_t request;
        create_request(&request, actorId);
        pool_add_request(pool, request);
    }
    pthread_mutex_unlock(&actors[actorId]->mutex);
    return 0;
}


void system_execute(actor_id_t actorId){
    pthread_mutex_lock(&actors[actorId]->mutex);
    setThreadActor(pthread_self(), actorId);
    actors[actorId]->is_working = true;

    message_t* message = buffer_take(actors[actorId]->messages);
    sem_post(&actors[actorId]->sem_full);
    pthread_mutex_unlock(&actors[actorId]->mutex);

    if(message->message_type == MSG_SPAWN && !interrupted){
        actor_id_t newActorId;

        pthread_mutex_lock(&system_mutex);
        create_actor(&newActorId, (role_t*)message->data);
        pthread_mutex_unlock(&system_mutex);

        message_t msgHello = {
                .message_type = MSG_HELLO,
                .nbytes = sizeof(actor_id_t),
                .data = (void*)actorId,
        };
        send_message(newActorId, msgHello);
    } else if(message->message_type == MSG_GODIE){
        pthread_mutex_lock(&system_mutex);
        actor_ctr--;
        if(actor_ctr == 0) {
            all_killed = true;
            pthread_cond_broadcast(&join_cond);
        }
        pthread_mutex_unlock(&system_mutex);
    } else if(message->message_type != MSG_SPAWN) {
        actors[actorId]->role->prompts[message->message_type](
                &actors[actorId]->stateptr,
                message->nbytes,
                message->data
        );
    }

    pthread_mutex_lock(&actors[actorId]->mutex);
    actors[actorId]->is_working = false;

    if(buffer_size(actors[actorId]->messages) > 0) {
        request_t request;
        create_request(&request, actorId);
        pool_add_request(pool, request);
    }
    pthread_mutex_unlock(&actors[actorId]->mutex);
    free(message);
}

int actor_system_create(actor_id_t *actor, role_t *const role){
    sigset_t sigset;
    sigemptyset(&sigset);
    sigaddset(&sigset, SIGINT);
    sigprocmask(SIG_BLOCK, &sigset, NULL);

    if(pthread_create(&sig_thread, NULL, &sig_thread_fun, NULL) != 0) {
        return -1;
    }
    pthread_detach(sig_thread);

    resetReg();
    all_killed = false;
    interrupted = false;
    actor_it = 0;
    actor_ctr = 0;

    pthread_mutexattr_t Attr;
    pthread_mutexattr_init(&Attr);
    pthread_mutexattr_settype(&Attr, PTHREAD_MUTEX_RECURSIVE);

    if(pthread_mutex_init(&system_mutex, &Attr) != 0) {
        return -1;
    }

    if(create_pool(&pool, POOL_SIZE, CAST_LIMIT) != 0) {
        return -1;
    }
    initReg(pool);
    if(create_actor(actor, role) != 0) {
        destroy_pool(&pool);
        return -2;
    }
    message_t msgHello = {
            .message_type = MSG_HELLO
    };
    send_message(*actor, msgHello);
    return 0;
}

static void destroy_system(){
    destroy_pool(&pool);
    for(size_t i=0;i<actor_it;i++)
        destroy_actor(i);

    pthread_mutex_destroy(&system_mutex);
    sigset_t sigset;
    sigfillset(&sigset);
    sigprocmask(SIG_UNBLOCK, &sigset, NULL);
}

void actor_system_join(actor_id_t actor){
    pthread_mutex_lock(&system_mutex);
    if(actor < 0 || (unsigned)actor >= actor_it) {
        pthread_mutex_unlock(&system_mutex);
        return;
    }
    while(!all_killed) {
        pthread_cond_wait(&join_cond, &system_mutex);
    }
    destroy_system();
    pthread_cancel(sig_thread);
    pthread_mutex_unlock(&system_mutex);
}


void system_int() {
    pthread_mutex_lock(&system_mutex);
    interrupted = true;
    message_t msgGoDie = {
            .message_type = MSG_GODIE
    };
    for(size_t i=0;i<actor_it;i++) {
        pthread_mutex_lock(&actors[i]->mutex);
        if(actors[i]->killed) {
            pthread_mutex_unlock(&actors[i]->mutex);
            continue;
        }
        actors[i]->killed = true;

        message_t* toSend = malloc(sizeof(message_t));
        if(toSend == NULL) {
            exit(INTCODE);
        }

        *toSend = msgGoDie;
        buffer_put(actors[i]->messages, toSend);
        sem_post(&actors[i]->sem_full);
       if(buffer_size(actors[i]->messages) == 1 &&
           !actors[i]->is_working ) {
            request_t request;
            create_request(&request, i);
            pool_add_request(pool, request);
        }
       pthread_mutex_unlock(&actors[i]->mutex);
    }
    pthread_mutex_unlock(&system_mutex);
    actor_system_join(0);
    exit(1);
}