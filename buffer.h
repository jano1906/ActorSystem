#ifndef JO418361_BUFFER_H
#define JO418361_BUFFER_H

#include <stddef.h>

typedef struct Buffer Buffer;
typedef Buffer* buffer_t;

// returns thing from buffer or waits till there is one,
// if no things are left but kill_buffer was called returns NULL
void* buffer_take(buffer_t buffer);

// all things put into buffer should be freed by
// one who takes from buffer
void buffer_put(buffer_t buffer, void* thing);

// retvals:
// 0    - success
// -1   - buffer malloc error
// -2   - arr malloc error
// -3   - mutex || semaphore init error
int create_buffer(buffer_t* buffer, size_t size);

// cleanup - assumes that no things are left in the buffer,
// if things are still in the buffer might cause memory leak
void destroy_buffer(buffer_t* buffer);

// any thing user tries to buffer_put after calling this function
// is freed immediately,
// buffer_take returns values till there are things in the buffer
// when no things are left returns NULL
void kill_buffer(buffer_t buffer);

size_t buffer_size(buffer_t buffer);

void buffer_print(buffer_t buffer);

#endif //JO418361_BUFFER_H
