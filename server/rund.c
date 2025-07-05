#include <libgen.h>
#include <pthread.h>
#include <signal.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <unistd.h>

#define NUM_COMMANDS 3

static char pipe_name[] = "/tmp/c_pipe";
int thread_counter = 0;

struct Args {
  int log_level;
  int num_threads;
};

void intHandler(int dummy) {
  dummy = 0;
  remove(pipe_name);
  printf("Exiting!\n");
  exit(dummy);
}

void *run_sim(void *script) {
  char *base = malloc(strlen(script));
  strcat(base, "./");
  strcat(base, basename(script));

  chdir(dirname(script));

  system(base);

  thread_counter--;
  return (void *)"sucess";
}

struct Args parse(int count, char **opts) {
  struct Args args = {0, 1};
  for (int i = 0; i < count; i++) {
    if (opts[i][0] == '-') {
      switch (opts[i][1]) {
      case 'l':
        args.log_level = (int)strtol(opts[i + 1], NULL, 10);
        break;
      case 't':
        args.num_threads = (int)strtol(opts[i + 1], NULL, 10);
        break;
      default:
        fprintf(stderr, "Usage: ./rund [-l <log_level>]\n");
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

  FILE *pipe_header;
  const int buffsize = 255;
  char buffer[buffsize];

  const char commands[NUM_COMMANDS][32] = {"exit\n", "status\n", "run\n"};

  pthread_t *sim_threads =
      malloc(sizeof(pthread_t) * (unsigned long)args.num_threads);

  // main loop of the server
  // open the pipe, parse a command, execute it, and close the pipe.
  while (1) {
    if (args.log_level == 2)
      printf("Opening pipe and waiting ...\n");
    pipe_header = fopen(pipe_name, "r");
    fgets(buffer, buffsize, pipe_header);
    fclose(pipe_header);
    if (args.log_level == 2)
      printf("Command recived: %s", buffer);

    if (strcasecmp(buffer, commands[0]) == 0) {
      break;
    } else if (strcasecmp(buffer, commands[1]) == 0) {
      if (args.log_level == 2)
        printf("Sending status.\n");
      pipe_header = fopen(pipe_name, "w");
      fprintf(pipe_header, "Status: Waiting for command\n");
      fclose(pipe_header);
    } else if (strcasecmp(buffer, commands[2]) == 0) {
      if (args.log_level == 2)
        printf("Getting simulation script...\n");
      pipe_header = fopen(pipe_name, "r");
      fgets(buffer, buffsize, pipe_header);
      if (args.log_level == 2)
        printf("Got script: %s\n", buffer);

      if (thread_counter < args.num_threads) {
        thread_counter++;
        pthread_create(&sim_threads[thread_counter - 1], NULL, &run_sim,
                       buffer);
      }

    } else {
      pipe_header = fopen(pipe_name, "w");
      fprintf(pipe_header, "Not a valid command!\n");
      fclose(pipe_header);
    }
  }

  printf("Exiting ... \n");
  remove(pipe_name);

  return 0;
}
