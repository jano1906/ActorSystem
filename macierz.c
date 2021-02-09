#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "cacti.h"

#define UNUSED(x) (void)(x)
#define MSG_GC 1
#define MSG_WC 2
#define MSG_COMPUTE 3
#define MSG_CLEANUP 4

typedef struct cell {
    int mills;
    int x;
} cell_t;

actor_id_t actor0;
actor_id_t* actors;
int completes = 0;
cell_t** matrix;
int* sums;
long no_cols;
int no_rows;

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
    long col;
    actor_id_t right;
} info_t;

typedef struct compInfo {
    int row;
    int res;
} comp_info_t;

void getCol(void** info, size_t size, void* left);
void writeCol(void** info, size_t size, void* col);
void compute(void** info, size_t size, void* comp_info);
void hello(void** info, size_t size, void* right);
void cleanUp(void** info, size_t size, void* dummy);

void cleanUp(void** info, size_t size, void* dummy) {
    UNUSED(size);
    UNUSED(dummy);
    free(*info);
    send_message(actor_id_self(), msgGoDie);
}

void getCol (void** info, size_t size, void* left) {
        UNUSED(size);
        message_t msgWC = {
            .message_type = MSG_WC,
            .nbytes = sizeof(int),
            .data = (void*) ((*(info_t**)info)->col-1)
        };
        send_message((actor_id_t)left, msgWC);
};

void writeCol(void** info, size_t size, void* col) {
    UNUSED(size);
    (*(info_t**)info)->col = (intptr_t)col;
    actors[(intptr_t)col] = actor_id_self();
    if((intptr_t) col > 0) {
        send_message(actor_id_self(), msgSpawn);
    } else {
        message_t msgCompute = {
                .message_type = MSG_COMPUTE,
        };
        send_message(actor_id_self(), msgCompute);
    }
}

void hello(void** info, size_t size, void* right){
    UNUSED(size);
    *info = malloc(sizeof(info_t));
    if(actor_id_self() != actor0) {
        (*(info_t **) info)->right = (actor_id_t) right;
        message_t msgGC = {
                .message_type = MSG_GC,
                .nbytes = sizeof(actor_id_t),
                .data = (void *) actor_id_self()
        };
        send_message((actor_id_t)right, msgGC);
    } else {
        message_t msgWC = {
                .message_type = MSG_WC,
                .nbytes = sizeof(int),
                .data = (void*) (no_cols-1)
        };
        send_message(actor_id_self(), msgWC);
    }
}

void compute(void** info, size_t size, void* comp_info) {
    UNUSED(size);
    long col = (*(info_t**) info)->col;
    actor_id_t right = (*(info_t**) info)->right;
    if(no_cols == 1) {
        for (int r = 0; r < no_rows; r++) {
            usleep(1000*matrix[r][col].mills);
            sums[r] = matrix[r][col].x;
        }
        send_message(actor_id_self(), msgCleanUp);
        return;
    }
    if(col == 0) {
        for(int r = 0;r<no_rows;r++) {
            comp_info_t *comp_info2 = malloc(sizeof(comp_info_t));
            comp_info2->row=r;
            usleep(1000*matrix[r][col].mills);
            comp_info2->res=matrix[r][col].x;
            message_t msgCompute = {
                    .message_type = MSG_COMPUTE,
                    .nbytes = sizeof(void*),
                    .data = comp_info2
            };
            send_message(right,msgCompute);
        }
        return;
    }

    int row = ((comp_info_t*)comp_info)->row;
    int res = ((comp_info_t*)comp_info)->res;
    usleep(1000*matrix[row][col].mills);
    res+= matrix[row][col].x;

    if(col == no_cols -1) {
        sums[row] = res;
        free(comp_info);
        completes++;
        if(completes == no_rows){
            for(int i=0;i<no_cols;i++){
                send_message(actors[i], msgCleanUp);
            }
        }
    } else {
        ((comp_info_t*)comp_info)->res = res;
        message_t msgCompute = {
                .message_type = MSG_COMPUTE,
                .nbytes = sizeof(void*),
                .data = comp_info
        };
        send_message(right, msgCompute);
    }
}

act_t act[5] = {&hello, &getCol, &writeCol, &compute, &cleanUp};
role_t role = {
        .nprompts = 5,
        .prompts = act
};

int main() {
    scanf("%d", &no_rows);
    scanf("%ld", &no_cols);
    matrix = malloc(no_rows * sizeof(cell_t));
    sums = malloc(no_rows * sizeof(int));
    actors = malloc(no_cols * sizeof(actor_id_t));

    for(int row =0;row<no_rows;row++){
        matrix[row] = malloc(no_cols * sizeof(cell_t));
        for(int col=0;col<no_cols;col++) {
            scanf("%d", &matrix[row][col].x);
            scanf("%d", &matrix[row][col].mills);
        }
    }

    msgSpawn.data = &role;
    actor_system_create(&actor0, &role);
    actor_system_join(actor0);

    for(int i=0;i<no_rows;i++)
        printf("%d\n", sums[i]);

    for(int row=0;row<no_rows;row++)
        free(matrix[row]);
    free(matrix);
    free(actors);
    free(sums);

    return 0;
}
