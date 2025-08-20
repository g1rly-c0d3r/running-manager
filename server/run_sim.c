#include <libgen.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

void queue_sim(char *script) { printf("[Main] Script name: %s", script); }

void *run_sim(void *script) {
  char *base = malloc(strlen(script));
  strcat(base, "./");
  strcat(base, basename(script));

  chdir(dirname(script));

  system(base);

  return (void *)"sucess";
}
