#ifndef JO418361_ACTORSYSTEMREGISTER_H
#define JO418361_ACTORSYSTEMREGISTER_H

#include "cacti.h"
#include "threadPool.h"

void resetReg();
actor_id_t getThreadActor(pthread_t thread);
void setThreadActor(pthread_t thread, actor_id_t actorId);
void initReg(pool_t pool);

#endif //JO418361_ACTORSYSTEMREGISTER_H
