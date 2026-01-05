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
#define RNMN_VERSION 0.1


#define BNRM  "\x1B[0m"
#define BRED  "\x1B[31m"
#define BGRN  "\x1B[32m"
#define BYEL  "\x1B[33m"
#define BBLU  "\x1B[34m"
#define BMAG  "\x1B[35m"
#define BCYN  "\x1B[36m"
#define BWHT  "\x1B[37m"

#define BBOLD "\x1B[1m"
#define BITAL "\x1B[3m"



enum lock_names{ THREAD, RUNNING };
typedef enum { READ, WRITE }pipe_ends ;
typedef enum { QUIET, NORM, DEBUG}log_level_t;
    
typedef struct {
    // number of simulations to allocate by default.
    // if more simulations are queued, more memory will be realloc'd
    uint8_t num_sims;
    log_level_t log_level;
    uint16_t num_threads;
}args_t;

#include "queue.c"
#include "run_sim.c"
#include "watch.c"

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
void print_status(struct List *running, struct Queue *waiting, pthread_mutex_t *list_lock);

int start_watcher(int *pipe, log_level_t logLevel);

void create_tmp(log_level_t log_level);

int check_watcher(int watch_pipe_reader, log_level_t loglevel, struct Queue *simQueue, struct List *runningList, pthread_mutex_t *list_lock);

void run_next_sim(arena_t *arena, struct Queue *simQueue, struct List *runninglist, pthread_mutex_t *threadlock, pthread_mutex_t *runninglock, uint16_t numthreads, log_level_t loglevel);

void remove_tmp(char *name, log_level_t ll);



void print_version(void){
    FILE *writter = fopen("/tmp/rund/status", "w");
    fprintf(writter, "%3.1f", RNMN_VERSION);
    fclose(writter);
}

void remove_tmp(char *name, log_level_t ll){
    char dir[128], cmd[128] = "rm -r ";
    strcpy(dir, name);
    dirname(dir);
    strcat(cmd, dir);
    system(cmd);
    if (ll >= DEBUG) printf("[Main] removed temporary directory.\n");
}

void run_next_sim(arena_t *arena, struct Queue *simQueue, struct List *runninglist, pthread_mutex_t *threadlock, pthread_mutex_t *runninglock, uint16_t numthreads, log_level_t loglevel){
    if (!is_empty(simQueue)) {
        pthread_mutex_lock(threadlock);
        if (numthreads - simQueue->front->threads_needed >= 0) {
            thread_counter += simQueue->front->threads_needed;
            char *script = arena_push(arena, 255);
            strncpy(script, simQueue->front->script, 254);
            run_sim(script, 
                    simQueue->front->threads_needed,
                    runninglist,
                    threadlock,
                    runninglock,
                    arena);
            dequeue(simQueue);
        }
        pthread_mutex_unlock(threadlock);
    } else if (loglevel >= DEBUG) {
        printf("[Main] Simulation queue empty! Nothing to run.\n");
    }
} // run_next_sim

int check_watcher(int watch_pipe_reader, log_level_t loglevel, struct Queue *simQueue, struct List *runningList, pthread_mutex_t *list_lock) { 
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
    if (loglevel >= NORM)
        printf("[Main] Exit command recived.\n");
    return -1;
    case STATUS:
    if (loglevel >= NORM)
        printf("[Main] Status command recived\n");

    print_status(runningList, simQueue, list_lock);
    break;
    case RUN:
    if (loglevel >= NORM)
        printf("[Main] Run command recived, name: %s\n", &command_buff[1]);
    // command_buff holds the command enum in the first element,
    // and the script name follows it.
    queue_sim(simQueue, &command_buff[1]); // this should probably be outside
    break;
    case VERSION:
        if (loglevel >= NORM)
            printf("[Main] Version command recived.\n");
        print_version();
        break;
    }

    return 0;
} // check_watcher

void create_tmp(log_level_t log_level){
    // clear the pipe if it exists to flush it 
    // and make sure no data is lingering 
    // and that there are no sneaky readers
    if (access(pipe_name, F_OK) == 0)
        remove(pipe_name);

    if (access("/tmp/rund/", F_OK) != 0)
            pass(NULL);

    mkdir("/tmp/rund", 0700);
    if (log_level >= DEBUG) {
        printf("[Main] Temporary directory created.\n");
    }
    


    if (mkfifo(pipe_name, 0666) != 0){
        fprintf(stderr, "[Main] ERROR: Can not create named pipe.\n");
        exit(1);
    }else if (log_level >= DEBUG) {
        printf("[Main] Named pipe created.\n");
    
    }

    if (access("/tmp/rund/status", F_OK) == 0){
        pass(NULL);
    }
    else if (mkfifo("/tmp/rund/status", 0666) != 0){
            fprintf(stderr, "[Main] ERROR: Can not create status pipe.\n");
        exit(1);
    }else if (log_level >= DEBUG) {
        printf("[Main] Named pipe created.\n");
    }

    char *home_dir = getenv("HOME");
    char buffer[128];
    strcpy(buffer, home_dir);
    strcat(buffer, "/.cache/rnmn/");

    if(access(buffer, F_OK) == 0){
        pass(NULL);
    } else {
        mkdir(buffer, 0700);
    }

    if (log_level >= NORM)
        printf("[Main] Temporary directory created.\n");
}

int start_watcher(int *pipe, log_level_t logLevel){
    arena_t *arena = arena_create_with_capacity(sizeof(Watch_Args) + 16);
    Watch_Args *watchArgs = arena_push(arena, sizeof(Watch_Args));
    *watchArgs = (Watch_Args){.pipeToMain = *pipe, 
                              .logLevel = logLevel, 
                              .named_pipe = pipe_name };
  
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

void print_status(struct List *running, struct Queue *waiting, pthread_mutex_t *list_lock) {
    FILE *status_file = fopen("/tmp/rund/status", "w");
    // un needed
    pthread_mutex_lock(list_lock);
    struct ListNode *listTraveler = running->head;
    Node *queueTraveler = waiting->front;

    char name[] =
" _____                  _              ___  ___            \n"
"| ___ \\                (_)             |  \\/  |            \n"
"| |_/ /   _ _ __  _ __  _ _ __   __ _  | .  . | __ _ _ __  \n"
"|    / | | | '_ \\| '_ \\| | '_ \\ / _` | | |\\/| |/ _` | '_ \\ \n"
"| |\\ \\ |_| | | | | | | | | | | | (_| | | |  | | (_| | | | |\n"
"\\_| \\_\\__,_|_| |_|_| |_|_|_| |_|\\__, | \\_|  |_/\\__,_|_| |_|\n"
"                                 __/ |                     \n"
"                                |___/                      \n"
;
    fprintf(status_file, "%s%s%s\n%s", BBOLD, BGRN, name, BNRM);

    if (listTraveler == NULL){
        fprintf(status_file, "\t%sNo simulations are currently running.%s\n", BBOLD, BNRM);
    } else {
        fprintf(status_file, "\t%sCurrently running simulations:%s\n", BBOLD, BNRM);
        do {
            fprintf(status_file, "\t\t%s\n", listTraveler->name);
            listTraveler = listTraveler->next_node;
        } while (listTraveler != NULL);
    }
    pthread_mutex_unlock(list_lock);

    if (queueTraveler == NULL){
        fprintf(status_file, "\n\t%sNo simulations waiting to run.%s\n",BBOLD,BNRM );
    } else{
        fprintf(status_file,"\n\t%sSimulations waiting to run:%s\n", BBOLD, BNRM);
        do{
            fprintf(status_file, "\t\t%s\n", queueTraveler->script);
            queueTraveler = queueTraveler->next_node;
        }while (queueTraveler->next_node != NULL);
    }
    fputs("\n", status_file);
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
        args.log_level = (log_level_t)strtol(opts[i + 1], NULL, 10);
        break;
      case 't':
        args.num_threads = (uint16_t)strtol(opts[i + 1], NULL, 10);
        break;
      default:
        fprintf(stderr, "Usage: ./rund [-l <log_level>]"
                        "[-t <num_threads>]"
                        "[-n <num_sims>]\n"
                        "RNMN should be started by the client, what are you doing?\n");
        exit(1);
      }
    }
  }
  return args;
}
