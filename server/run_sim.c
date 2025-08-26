#include <libgen.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

void *run_sim(void *script) {
  char *base = malloc(strlen(script));
  strcat(base, "./");
  strcat(base, basename(script));

  chdir(dirname(script));

  system(base);

  return (void *)"sucess";
}
