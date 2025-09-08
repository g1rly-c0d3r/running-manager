#define _GNU_SOURCE

#include "queue.c"
#include <alloca.h>
#include <libgen.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>

struct simArgs {
  uint8_t threads;
  pthread_mutex_t *threadCounter;
  char *script;
};

static void *sim(void *args);

pthread_t *run_sim(void *script, uint8_t threads, struct List *running,
             pthread_mutex_t *threadCounter) {
  pthread_t *sim_thread = malloc(sizeof(pthread_t));
  push(running, script, threads, sim_thread);

  struct simArgs *sim_args = malloc(sizeof(struct simArgs));
  *sim_args = (struct simArgs){.script = script, .threads = threads, .threadCounter = threadCounter};
  pthread_create(sim_thread, NULL, sim, (void *)sim_args);
  pthread_setname_np(*sim_thread, script);
  return sim_thread;
}

static void *sim(void *args) {
  char *script = ((struct simArgs *)args)->script;
  uint8_t threads_in_use = ((struct simArgs *)args)->threads;
  pthread_mutex_t *threadCounter = ((struct simArgs *)args)->threadCounter;
  char *buffer = alloca(512 * sizeof(char));

  // we will be running this sim and saving the output
  char *redirect = " > run.out 2> err.out";
  char *path = alloca(512 * sizeof(char));
  sprintf(path, "~/.cache/rnmn/%d/", gettid());
  mkdir(path, 0700);

  // copy script_dir to path, and then chdir to path
  sprintf(buffer, "cp -r %s/**/* %s", dirname(script), path);
  system(buffer);
  chdir(path);

  sprintf(buffer, "%s %s", basename(script), redirect);
  system(buffer);

  pthread_mutex_lock(threadCounter);
  thread_counter -= threads_in_use;
  pthread_mutex_unlock(threadCounter);

  return (void *)script;
}
