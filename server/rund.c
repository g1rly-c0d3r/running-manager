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

static char pipe_name[] = "/tmp/rund";
uint16_t thread_counter = 0;
char **simulations;

#include "run_sim.c"
#include "watch.c"

struct Args {
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
  struct Args args = {0, 1};
  for (int i = 0; i < count; i++) {
    if (opts[i][0] == '-') {
      switch (opts[i][1]) {
      case 'l':
        args.log_level = (int8_t)strtol(opts[i + 1], NULL, 10);
        break;
      case 't':
        args.num_threads = (uint16_t)strtol(opts[i + 1], NULL, 10);
        break;
      default:
        fprintf(stderr, "Usage: ./rund [-l <log_level>] [-t <num_threads>]\n");
        exit(1);
      }
    }
  }
  return args;
}

int main(int argc, char **argv) {
  signal(SIGINT, intHandler);

  struct Args args = parse(argc, argv);

  // remove any exisiting pipe that already exists
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
  pthread_mutex_t threadCountLock;
  pthread_mutex_init(&threadCountLock, NULL);

  Watch_Args watchArgs = {.pipeToMain = watchPipe[1],
                          .logLevel = args.log_level,
                          .named_pipe = pipe_name};

  // The watcher thread will open the named pipe and block for a command,
  // and send a the command it recives to the main thread via an unamed pipe.
  // This is so that the main thread does not have to block for a command
  // (the main thread is a supervisor).
  pthread_t watch_thread;
  pthread_create(&watch_thread, NULL, watch, (void *)&watchArgs);
  pthread_setname_np(watch_thread, "watcher");

  // main loop of the server
  // open the pipe, parse a command, execute it, and close the pipe.
  const uint8_t buffsize = 255;
  char command_buff[buffsize];
  char sim_script[buffsize];

  while (1) {
    command_buff[0] = 0;
    if (canReadFromPipe(watchPipe[0])) {
      read(watchPipe[0], command_buff, buffsize);
    }

    switch (atoi(command_buff)) {
    case EXIT:
      printf("[Main] Goodbye!\n");
      return 0;
      break;
    case STATUS:
      printf("[Main] Status command not implemented yet! Sorry!\n");
      // TODO: implement status command.
      break;
    case RUN:
      read(watchPipe[0], sim_script, buffsize);
      queue_sim(sim_script);
    }
  }

  pthread_join(watch_thread, NULL);

  printf("Exiting ... \n");
  remove(pipe_name);

  return 0;
}
