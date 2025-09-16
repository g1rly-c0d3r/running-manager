#define _GNU_SOURCE

#include <libgen.h>
#include <poll.h>
#include <pthread.h>
#include <signal.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <unistd.h>

char pipe_name[] = "/tmp/rund";
uint16_t thread_counter = 0;

#include "queue.c"
#include "run_sim.c"
#include "watch.c"

struct Args {
  // number of simulations to allocate by default.
  // if more simulations are queued, more memory will be realloc'd
  uint8_t num_sims;
  int8_t log_level;
  uint16_t num_threads;
};

void intHandler(int dummy) {
  dummy = 0;
  remove(pipe_name);
  printf("Exiting!\n");
  exit(dummy);
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

struct Args parse(int count, char **opts) {
  struct Args args = {10, 0, 1};
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
        fprintf(stderr, "Usage: ./rund [-n <num_sims> -l <log_level>] [-t "
                        "<num_threads>]\n");
        exit(1);
      }
    }
  }
  return args;
}

// takes a list of currently running sims and queue of sims waiting to run,
// and prints them with the number of threads they need/are using.
void print_status(struct List *running, struct Queue *waiting,
                  pthread_mutex_t *pipeLock) {
  pthread_mutex_lock(pipeLock);
  FILE *pipe = fopen(pipe_name, "w");
  void *traveler;
  traveler = running->head;

  if (traveler == NULL) {
    fprintf(pipe, "\n=========================================================="
                  "\nNo simulations currently running.\n");
  } else {
    fprintf(pipe, "\n=========================================================="
                  "\nCurrently Running:\n");
    while (((struct ListNode *)traveler)->next_node != NULL) {
      fprintf(pipe, "Name: %s, threads: %d\n",
              ((struct ListNode *)traveler)->name,
              ((struct ListNode *)traveler)->threads);
    }
  }

  traveler = waiting->front;
  if (traveler == NULL) {
    fprintf(pipe, "\n=========================================================="
                  "\nNo simulations waiting to be ran.\n");
  } else {
    fprintf(pipe,
            "\n\n=========================================================="
            "\nWaiting in Queue:\n");
    while (((struct Node *)traveler)->next_node != NULL) {
      fprintf(pipe, "Name: %s, threads: %d\n",
              ((struct Node *)traveler)->script,
              ((struct Node *)traveler)->threads_needed);
    }
  }
  fclose(pipe);
  pthread_mutex_unlock(pipeLock);
}

int main(int argc, char **argv) {
  signal(SIGINT, intHandler);

  struct Args args = parse(argc, argv);
  struct Queue *simQueue = create_queue();
  struct List *runningList = create_list();


  // remove any exisiting pipe that already exists
  // mostly just to make sure it hasnt been opened for writing,
  // and that it is actually empty.
  remove(pipe_name);

  int err = mkfifo(pipe_name, 0666);
  if (err != 0) {
    fprintf(stderr, "Error! can not create pipe!\n");
    return err;
  }
  //  FILE *pipe_header;

  int watchPipe[2];
  pipe(watchPipe);

  // Making sure that only one thread can read or write to
  // thread_counter at a time
  pthread_mutex_t *threadCountLock = malloc(sizeof(pthread_mutex_t));
  pthread_mutex_init(threadCountLock, NULL);

  pthread_mutex_t *pipeLock = malloc(sizeof(pthread_mutex_t));
  pthread_mutex_init(pipeLock, NULL);

  Watch_Args watchArgs = {.pipeToMain = watchPipe[1],
                          .logLevel = args.log_level,
                          .named_pipe = pipe_name,
                          .pipe_lock = pipeLock};

  // The watcher thread will open the named pipe and block for a command,
  // and send a the command it recives to the main thread via an unamed pipe.
  // This is so that the main thread does not have to block for a command
  // (the main thread is a supervisor).
  // this is so that the main function can do other work while reading from the
  // pipe blocks.
  pthread_t watch_thread;
  pthread_create(&watch_thread, NULL, watch, (void *)&watchArgs);
  pthread_setname_np(watch_thread, "watcher");

  // main loop of the server
  // open the pipe, parse a command, execute it, and close the pipe.
  const uint8_t buffsize = 255;
  char command_buff[buffsize];

  if (args.log_level == 2)
    printf("[Main] Startup successful, entering main loop\n");

  while (1) {
    // to not hog a whole cpu
    // if ur reading this bcuz you want to use this software on your machine,
    // and ur mad that this is here, take it out.
    // If you're reading this, that means you can change it however you want.

    sleep(1);

    // so that if the command from the prev iteration doesn't get ran.
    command_buff[0] = 0;
    if (canReadFromPipe(watchPipe[0])) {
      read(watchPipe[0], command_buff, buffsize);
    }

    switch (atoi(command_buff)) {
    case EXIT:
      // TODO: kill running sims before exiting
      printf("[Main] Goodbye!\n");
      break;
    case STATUS:
      // printf("[Main] Status command not implemented yet! Sorry!\n");
      if (args.log_level >= 1)
        printf("[Main] Printing status to pipe ... ");

      print_status(runningList, simQueue, pipeLock);
      if (args.log_level >= 1)
        printf("done\n");
      break;
    case RUN:
      // command_buff holds the command enum in the first element,
      // and the script name follows it.
      queue_sim(simQueue, &command_buff[1]);
    }

    if (!is_empty(simQueue)) {
      pthread_mutex_lock(threadCountLock);
      if (args.num_threads - simQueue->front->threads_needed >= 0) {
        thread_counter -= simQueue->front->threads_needed;
        run_sim(simQueue->front->script, simQueue->front->threads_needed,
                runningList, threadCountLock);
        dequeue(simQueue);
      }
      pthread_mutex_unlock(threadCountLock);
    } else if (args.log_level >= 3) {
      printf("[Main] Simulation queue empty! Nothing to run.\n");
    }
  }

  pthread_join(watch_thread, NULL);
  pthread_mutex_destroy(threadCountLock);

  printf("Exiting ... \n");
  remove(pipe_name);

  return 0;
}
