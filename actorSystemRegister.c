#include "actorSystemRegister.h"

actor_id_t actors_reg[POOL_SIZE];
pthread_t threads_reg[POOL_SIZE];
void resetActorsReg(){
    for(int i=0;i<POOL_SIZE;i++)
        actors_reg[i] = 0;
}
void resetThreadsReg(){
    for(int i=0;i<POOL_SIZE;i++)
        threads_reg[i] = 0;
}

size_t findThreadIdx(pthread_t pthread){
    for(size_t idx=0;idx<POOL_SIZE;idx++) {
        if(threads_reg[idx] == pthread)
            return idx;
    }
    return POOL_SIZE+1;
}

void resetReg(){
    resetActorsReg();
    resetThreadsReg();
}

void initReg(pool_t pool){
    pool_copy_threads(pool, threads_reg);
}

actor_id_t getThreadActor(pthread_t thread){
    return actors_reg[findThreadIdx(thread)];
}

void setThreadActor(pthread_t thread, actor_id_t actorId){
    actors_reg[findThreadIdx(thread)] = actorId;
}