#include <bits/pthreadtypes.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define NUM_COMMANDS 3
const char commands[NUM_COMMANDS][32] = {"exit\n", "status\n", "run\n"};
enum commands { PASS, EXIT, STATUS, RUN };

enum watch_return { SUCESS, BROKEN_PIPE };

typedef struct Watch_Args Watch_Args;
struct Watch_Args {
  int pipeToMain;
  int logLevel;
  char *named_pipe;
};

void *watch(void *args) {
  int pipeToMain = (*(Watch_Args *)args).pipeToMain;
  int logLevel = (*(Watch_Args *)args).logLevel;
  char *named_pipe = (*(Watch_Args *)args).named_pipe;

  FILE *pipe_header;
  const int buffsize = 50;
  char commandBuffer[buffsize];
  ssize_t res;

  while (true) {
    if (logLevel == 2)
      printf("[Watcher] Opening pipe and waiting ...\n");
    pipe_header = fopen(named_pipe, "r");
    fgets(commandBuffer, buffsize - 1, pipe_header);
    fclose(pipe_header);
    if (logLevel == 2)
      printf("[Watcher] Command recived: %s", commandBuffer);

    if (strcasecmp(commandBuffer, commands[0]) == 0) {
      /* exit the program
       * This is achived by returning from the watch loop.*/
      res = dprintf(pipeToMain, "%d", EXIT);
      if (res != 1) {
        fprintf(stderr, "[Watcher] Error! Broken pipe!\n");
        exit(BROKEN_PIPE);
      }
      break;
    } else if (strcasecmp(commandBuffer, commands[1]) == 0) {
      /* We want to send the "status" command to the main thread. */
      res = dprintf(pipeToMain, "%d", STATUS);
      if (res != 1) {
        fprintf(stderr, "[Watcher] Error! Broken pipe!\n");
        exit(BROKEN_PIPE);
        break;
      }
    }
    // run
    else if (strcasecmp(commandBuffer, commands[2]) == 0) {

      res = dprintf(pipeToMain, "%d", RUN);
      if (res != 1) {
        fprintf(stderr, "[Watcher] Error! Broken pipe!\n");
        exit(BROKEN_PIPE);
        break;
      }

      if (logLevel == 2)
        printf("[Watcher] Getting simulation script...\n");
      pipe_header = fopen(named_pipe, "r");
      fgets(commandBuffer, buffsize, pipe_header);
      if (logLevel == 2)
        printf("[Watcher] Got script: %s", commandBuffer);
      fclose(pipe_header);

      res = write(pipeToMain, commandBuffer, (size_t)buffsize);
      if (res == -1) {
        fprintf(stderr, "[Watcher] Error! Broken pipe!\n");
        exit(BROKEN_PIPE);
        break;
      }

    } else {
      if (logLevel == 2)
        printf("[Watcher] Invalid command recived!\n");
      pipe_header = fopen(named_pipe, "w");
      fprintf(pipe_header, "Not a valid command!\n");
      fclose(pipe_header);
    }
  }

  return NULL;
}
