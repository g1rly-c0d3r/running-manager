#define _GNU_SOURCE

#include <libgen.h>
#include <poll.h>
#include <pthread.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arena.h>
#include <errno.h>

char pipe_name[] = "/tmp/rund/command";
uint16_t thread_counter = 0;

#include "queue.c"
#include "run_sim.c"
#include "watch.c"

// for dev purposes, 
// will be removed when finished
void pass(void){}

typedef struct {
    // number of simulations to allocate by default.
    // if more simulations are queued, more memory will be realloc'd
    uint8_t num_sims;
    int8_t log_level;
    uint16_t num_threads;
}args_t;

enum lock_names{ THREAD, PIPE };
enum pipe_ends { READ, WRITE };
    

void intHandler(int dummy) {
    dummy = 0;
    remove(pipe_name);
    printf("Exiting!\n");
    exit(dummy);
}


bool canReadFromPipe(int32_t fd);

args_t parse(int count, char **opts);

// takes a list of currently running sims and queue of sims waiting to run,
// and prints them with the number of threads they need/are using.
void print_status(struct List *running, struct Queue *waiting);

int start_watcher(int *pipe, int8_t logLevel, pthread_mutex_t *lock);

void create_tmp(int8_t log_level);

int check_watcher(int watch_pipe_reader, int8_t loglevel, struct Queue *simQueue, struct List *runningList);

void run_next_sim(struct Queue *simQueue, struct List *runninglist, pthread_mutex_t *threadlock, uint16_t numthreads, int8_t loglevel);

void run_next_sim(struct Queue *simQueue, struct List *runninglist, pthread_mutex_t *threadlock, uint16_t numthreads, int8_t loglevel){
    if (!is_empty(simQueue)) {
        pthread_mutex_lock(threadlock);
        if (numthreads - simQueue->front->threads_needed >= 0) {
            thread_counter -= simQueue->front->threads_needed;
            run_sim(simQueue->front->script, 
                    simQueue->front->threads_needed,
                    runninglist,
                    threadlock);
            dequeue(simQueue);
        }
        pthread_mutex_unlock(threadlock);
    } else if (loglevel >= 3) {
        printf("[Main] Simulation queue empty! Nothing to run.\n");
    }
} // run_next_sim

int check_watcher(int watch_pipe_reader, int8_t loglevel, struct Queue *simQueue, struct List *runningList) { 
    // command_buff should be declared in check_watcher
    // buffer to recive commands from the watcher thread
    const uint8_t buffsize = 255;
    char command_buff[buffsize];

    // so that if the command from the prev iteration doesn't get ran.
    command_buff[0] = 0;
    if (canReadFromPipe(watch_pipe_reader)) {
    read(watch_pipe_reader, command_buff, buffsize);
    }

    switch (strtoul(command_buff, NULL, 10)) {
    case EXIT:
    // TODO: kill running sims before exiting
    if (loglevel >= 1)
        printf("[Main] Exit command recived.\n");
    return -1;
    case STATUS:
    if (loglevel >= 1)
        printf("[Main] Status command recived\n");

    print_status(runningList, simQueue);
    break;
    case RUN:
    if (loglevel >= 1)
        printf("[Main] Run command recived, name: %s", &command_buff[1]);
    // command_buff holds the command enum in the first element,
    // and the script name follows it.
    queue_sim(simQueue, &command_buff[1]); // this should probably be outside
    }

    return 0;
} // check_watcher

void create_tmp(int8_t log_level){
    // clear the pipe if it exists to flush it 
    // and make sure no data is lingering 
    // and that there are no sneaky readers
    if (access(pipe_name, F_OK) == 0)
        remove(pipe_name);

    if (access(dirname(pipe_name), F_OK) != 0)
            pass();

    mkdir(dirname(pipe_name), 0755);
    if ( (errno & ( EACCES | 
                EBADF | 
                EDQUOT | 
                EFAULT | 
                ELOOP | 
                EMLINK | 
                ENAMETOOLONG |
                ENOENT | 
                ENOMEM | 
                ENOSPC |
                ENOTDIR | 
                EPERM |
                EROFS |
                EOVERFLOW)) != 0) {
        fprintf(stderr, "[Main] ERROR: can not create temporary directory.\n");
        exit(1);
    } else if (log_level > 1) {
        printf("[Main] Temporary directory created.\n");
    }
    

    if (mkfifo(pipe_name, 0666) != 0){
        fprintf(stderr, "[Main] ERROR: Can not create named pipe.\n");
        exit(1);
    }else if (log_level >1) {
        printf("[Main] Named pipe created.\n");
    
    }
}

int start_watcher(int *pipe, int8_t logLevel, pthread_mutex_t *lock){
    arena_t *arena = arena_create_with_capacity(sizeof(Watch_Args) + 16);
    Watch_Args *watchArgs = arena_push(arena, sizeof(Watch_Args));
    *watchArgs = (Watch_Args){.pipeToMain = *pipe, 
                              .logLevel = logLevel, 
                              .named_pipe = pipe_name,
                              .pipe_lock = lock};
  
    // The watcher thread will open the named pipe and block for a command,
    // and send a the command it recives to the main thread via an unamed pipe.
    // This is so that the main thread does not have to block for a command
    // (the main thread is a supervisor).
    // this is so that the main function can do other work while reading from the
    // pipe blocks.
    pthread_t watch_thread;
    pthread_create(&watch_thread, NULL, watch, (void *)arena);
    pthread_setname_np(watch_thread, "watcher");
    switch (pthread_detach(watch_thread)){
        case EINVAL:
            fprintf(stderr, "[Main] ERROR: can not detach watcher thread\n");
            return 1;
        case ESRCH:
            fprintf(stderr, "[Main] ERROR: watcher thread could not be created\n");
            return 1;
    }
    return 0;
}


bool canReadFromPipe(int32_t fd) {
  // file descriptor struct to check if POLLIN bit will be set
  // fd is the file descriptor of the pipe
  struct pollfd fds = {.fd = fd, .events = POLLIN};
  // poll with no wait time
  int res = poll(&fds, 1, 0);

  // if res < 0 then an error occurred with poll
  // POLLERR is set for some other errors
  // POLLNVAL is set if the pipe is closed
  if (res < 0 || fds.revents & (POLLERR | POLLNVAL)) {
    // an error occurred, check errno
  }
  return fds.revents & POLLIN;
}

void print_status(struct List *running, struct Queue *waiting) {
    FILE *status_file = fopen("/tmp/rund/status", "w");
    // un needed
    struct ListNode *listTraveler = running->head;
    Node *queueTraveler = waiting->front;

    fputs("Currently running simulations:\n", status_file);
    while (listTraveler->next_node != NULL){
        fprintf(status_file, "%s\n", listTraveler->name);
        listTraveler = listTraveler->next_node;
    }
    fputs("\nSimulations waiting to run:\n", status_file);

    while (queueTraveler->next_node != NULL){
        fprintf(status_file, "%s\n", queueTraveler->script);
        queueTraveler = queueTraveler->next_node;
    }
    fclose(status_file);
}

args_t parse(int count, char **opts) {
  args_t args = {10, 0, 1};
  for (int i = 0; i < count; i++) {
    if (opts[i][0] == '-') {
      switch (opts[i][1]) {
      case 'n':
        args.num_sims = (uint8_t)strtol(opts[i + 1], NULL, 10);
        break;
      case 'l':
        args.log_level = (int8_t)strtol(opts[i + 1], NULL, 10);
        break;
      case 't':
        args.num_threads = (uint16_t)strtol(opts[i + 1], NULL, 10);
        break;
      default:
        fprintf(stderr, "Usage: ./rund [-l <log_level>]"
                        "[-t <num_threads>]\n");
        exit(1);
      }
    }
  }
  return args;
}
