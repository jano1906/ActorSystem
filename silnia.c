#include <stdio.h>
#include <stdlib.h>
#include "cacti.h"

#define UNUSED(x) (void)(x)
#define MSG_STEP 1
#define MSG_JOB 2
#define MSG_CLEANUP 3

actor_id_t actor0;

message_t msgGoDie = {
        .message_type = MSG_GODIE
};
message_t msgSpawn = {
        .message_type = MSG_SPAWN
};
message_t msgCleanUp = {
        .message_type = MSG_CLEANUP
};

typedef struct info {
  int k;
  int n;
  int partial;
  actor_id_t father;
} info_t;

void getJob(void** info, size_t size, void* sonId) {
    UNUSED(size);
    message_t msgStep = {
            .message_type = MSG_STEP,
            .nbytes = sizeof(void*),
            .data = *info
    };
    send_message((actor_id_t)sonId, msgStep);
}
void cleanUp(void** info, size_t size, void* data) {
    UNUSED(size);
    UNUSED(data);
    if(actor_id_self() != actor0)
        send_message((*((info_t**)info))->father, msgCleanUp);
    free(*info);
    send_message(actor_id_self(), msgGoDie);
}
void hello0(void** info, size_t size, void* fatherId) {
    UNUSED(size);
    UNUSED(fatherId);
    *info = malloc(sizeof(info_t));
    int n;
    scanf("%d", &n);
    (*((info_t**)info))->n = n;
    (*((info_t**)info))->k = 1;
    (*((info_t**)info))->partial = 1;
    send_message(actor_id_self(), msgSpawn);
}
void hello(void** info, size_t size, void* fatherId) {
    UNUSED(size);
    *info = malloc(sizeof(info_t));
    (*((info_t**)info))->father = (actor_id_t)fatherId;
    message_t msgJob = {
            .message_type = MSG_JOB,
            .nbytes = sizeof(actor_id_t),
            .data = (void*)actor_id_self()
    };
    send_message((actor_id_t)fatherId, msgJob);
}
void step(void** myInfo, size_t size, void* faInfo) {
    UNUSED(size);
    if(((info_t*)faInfo)->k == ((info_t*)faInfo)->n ||
        ((info_t*)faInfo)->n == 0) {
        printf("%d",((info_t*)faInfo)->partial);
        send_message(actor_id_self(), msgCleanUp);
    } else {
        (*((info_t**)myInfo))->k = ((info_t*)faInfo)->k+1;
        (*((info_t**)myInfo))->n = ((info_t*)faInfo)->n;
        (*((info_t**)myInfo))->partial =((info_t*)faInfo)->partial *
                                        (*((info_t**)myInfo))->k;
        send_message(actor_id_self(), msgSpawn);
    }
}

act_t act0[4] = {&hello0, &step, &getJob, &cleanUp};
role_t role0 = {
        .nprompts = 4,
        .prompts = act0
};

act_t act[4] = {&hello, &step, &getJob, &cleanUp};
role_t role = {
        .nprompts = 4,
        .prompts = act
};

int main() {
    msgSpawn.data = &role;
    actor_system_create(&actor0, &role0);
    actor_system_join(actor0);
    return 0;
}
