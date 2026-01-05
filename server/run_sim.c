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
#include <arena.h>
#include <string.h>

void *pass(void* args){return args;}

struct simArgs {
  uint8_t threads;
  struct List *running;
  pthread_mutex_t *threadCounter;
  pthread_mutex_t *running_lock;
  char *script;
};

static void *sim(void *args);

pthread_t *run_sim(char *script, uint8_t threads, struct List *running,
             pthread_mutex_t *threadCounter, pthread_mutex_t *running_lock, arena_t *arena) {
  pthread_t *sim_thread = arena_push(arena, sizeof(pthread_t));
  push(running, script, threads, sim_thread);

  struct simArgs *sim_args = arena_push(arena, sizeof(struct simArgs));
  *sim_args = (struct simArgs){.script = script, .threads = threads, .threadCounter = threadCounter, .running_lock = running_lock, .running = running };
  pthread_create(sim_thread, NULL, sim, (void *)sim_args);
  return sim_thread;
}

static void *sim(void *args) {
  char *script = ((struct simArgs *)args)->script;
  uint8_t threads_in_use = ((struct simArgs *)args)->threads;
  pthread_mutex_t *threadCounter = ((struct simArgs *)args)->threadCounter;
  pthread_mutex_t *list_lock = ((struct simArgs *)args)->running_lock;
  struct List *running_list = ((struct simArgs *)args)->running;


  char path[512], cmd[512], dir[512];

  //get path that we will be working in
  sprintf(path, "~/.cache/rnmn/%d/", gettid());
  mkdir(path, 0700);

  // get the path to the script's parent dir 
  // so we can tar it up and copy it
  strcpy(dir, script);
  dirname(dir);

  // we will be running this sim and saving the output
  char redirect[128];
  sprintf(redirect, " > %s/run.out 2> %s/err.out", path, path);

  // copy script_dir to path, and then chdir to path
  sprintf(cmd, "cp -r %s %s", dir, path);
  system(cmd);

  // running the simulation 
  sprintf(cmd, "%s/%s %s", path, basename(script), redirect);
  system(cmd);

  // once the simulation has finished, 
  // we need to remove the simulation object from the list of currently running simulations
  pthread_mutex_lock(threadCounter);
  thread_counter -= threads_in_use;
  pthread_mutex_unlock(threadCounter);
  pthread_mutex_lock(list_lock);
  remove_node(running_list, script);
  pthread_mutex_unlock(list_lock);

  sprintf(cmd, "tar -czf %s/run%d.tar.gz %s", path, gettid(), path);
  system(cmd);

  sprintf(cmd, "cp %s/run%d.tar.gz %s", path, gettid(), dir);
  system(cmd);

  sprintf(cmd, "rm -r %s", path);
  system(cmd);


  return (void *)script;
}
