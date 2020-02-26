#ifndef __ADDR_TRACKER_H_
#define __ADDR_TRACKER_H_

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#ifdef __cplusplus
extern "C" {
#endif

extern inline char *__arthas_tracker_file_name(char * buf);
extern inline void __arthas_track_addr(void *addr);

void __arthas_addr_tracker_init();
bool __arthas_addr_tracker_dump();

#ifdef __cplusplus
}  // extern "C"
#endif

#endif /* __ADDR_TRACKER_H_ */
