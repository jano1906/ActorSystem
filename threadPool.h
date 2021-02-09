#ifndef JO418361_THREADPOOL_H
#define JO418361_THREADPOOL_H

#include <stddef.h>
#include <pthread.h>
#include "cacti.h"

/// threadPool is thread manager for requesting tasks
/// pool of threads takes requests from system and calls system_execute

// system_execute need to be implemented
// by third party system, thread pool just says that it will execute
// whatever actor with given id requested
void system_execute(actor_id_t actorId);

typedef struct Pool Pool;
typedef Pool* pool_t;
typedef struct Request Request;
typedef Request* request_t;

void pool_add_request(pool_t pool, request_t request);

void pool_copy_threads(pool_t pool, pthread_t* arr);

// retvals:
// 0    - success
// -1   - pool malloc error
// -2   - tasks malloc error
// -3   - buffer create error
int create_pool(pool_t* pool, size_t size, size_t requestLimit);

// any task added to queue after calling this function will be destroyed
// pool executes tasks that are in the queue, then deallocates memory
void destroy_pool(pool_t* pool);

// retvals:
// 0    - success
// -1   - malloc error
int create_request(request_t* request, actor_id_t actorId);


#endif //JO418361_THREADPOOL_H
