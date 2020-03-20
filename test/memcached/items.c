#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct settings {
  size_t maxbytes;
  int maxconns;
  int port;
};

void *slab_automove_init(struct settings *settings);
void slab_automove_free(void *arg);
void slab_automove_run(void *arg, int *src, int *dst);

typedef void *(*slab_automove_init_func)(struct settings *settings);
typedef void (*slab_automove_free_func)(void *arg);
typedef void (*slab_automove_run_func)(void *arg, int *src, int *dst);

typedef struct {
    slab_automove_init_func init;
    slab_automove_free_func free;
    slab_automove_run_func run;
} slab_automove_reg_t;

typedef struct {
  uint32_t window_size;
  uint32_t window_cur;
  double max_age_ratio;
} slab_automove;

slab_automove_reg_t slab_automove_default = {.init = slab_automove_init,
                                             .free = slab_automove_free,
                                             .run = slab_automove_run};

static pthread_t lru_maintainer_tid;
struct settings settings;

static void *lru_maintainer_thread(void *arg) {
  settings.maxconns = 10;
  slab_automove_reg_t *sam = &slab_automove_default;
  void *am = sam->init(&settings);
  int src, dst;
  sam->run(am, &src, &dst);
  sam->free(am);
  printf("src=%d, dst=%d\n", src, dst);
  return NULL;
}

void *slab_automove_init(struct settings *settings) {
  printf("Dummy slab_automove_init, maxconns=%d\n", settings->maxconns);
  slab_automove *a = calloc(1, sizeof(slab_automove));
  return a;
}
void slab_automove_free(void *arg) {
  if (arg)
    printf("Dummy slab_automove_free with %p\n", arg);
  else
    printf("Dummy slab_automove_free\n");
}
void slab_automove_run(void *arg, int *src, int *dst) {
  *src = 5;
  *dst = 8;
  printf("Dummy slab_automove_run\n");
}

int main(int argc, char *argv[]) {
  int ret;
  void *arg = &argc;
  if ((ret = pthread_create(&lru_maintainer_tid, NULL, lru_maintainer_thread,
                            arg)) != 0) {
    fprintf(stderr, "Can't create LRU maintainer thread: %s\n", strerror(ret));
    return -1;
  }
  if ((ret = pthread_join(lru_maintainer_tid, NULL)) != 0) {
    fprintf(stderr, "Failed to stop LRU maintainer thread: %s\n",
            strerror(ret));
    return -1;
  }
  return 0;
}
