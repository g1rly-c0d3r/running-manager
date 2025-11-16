/* main.c
 *
 * Driver for rund, the server for RNMN
 * The only include here should be rund.c
 * that handles all functionality 
 * this file is simply a driver
 * including anything else will make testing a nightmare
 *
 */
#include "rund.c"
#include <signal.h>

/*
 * Constants:
 * pipe_name: "/tmp/rund/command
 * watchPipe: pipe object ( int[2] ) with a read end and a write end
 *
 */

int main(int argc, char **argv) {
    // setup SIGINT handling
    // this will be more useful later
    struct sigaction act;
    act.sa_handler = intHandler;
    sigaction(SIGINT, &act, NULL);

    arena_t *main_arena = arena_create();
    args_t args = parse(argc, argv);

    int watchPipe[2];
    pipe(watchPipe);

    // create temporary dir in /tmp.
    // also create the fifo if it does not exist
    // if an error occurs in creating the temporary space,
    // we will exit with code 1
    create_tmp(args.log_level);

    // these two lists define simulations that are currently running, 
    // and simulations that are waiting to run
    struct Queue *simQueue = create_queue();
    struct List *runningList = create_list();
  
  
    const uint8_t numLocks = 2;
    pthread_mutex_t locks[numLocks];
    pthread_mutex_init(&locks[THREAD], NULL);
    pthread_mutex_init(&locks[PIPE], NULL);

    if (start_watcher(&watchPipe[WRITE], args.log_level, &locks[PIPE]) !=0){
        fprintf(stderr, "[Main] ERROR: can not create watcher pipe!\n");
        goto dealloc;
    }
  
    // main loop of the server
    // open the pipe, parse a command, execute it, and close the pipe.
  
    if (args.log_level >= 2)
      printf("[Main] Startup successful, entering main loop\n");
  
    while (1) {
      // to not hog a whole cpu
      // if ur reading this bcuz you want to use this software on your machine,
      // and ur mad that this is here, take it out.
      // If you're reading this, that means you can change it however you want.
      sleep(1);

      if (check_watcher(watchPipe[0], args.log_level, simQueue, runningList) == -1){
          // exit command recieved.
          goto dealloc;
      }

      run_next_sim(simQueue, runningList, &locks[THREAD], args.num_threads, args.log_level);

      if (args.log_level >= 2)
          printf("[Main] Waiting for command ... \n");
    }
  
dealloc:
    pthread_mutex_destroy(&locks[PIPE]);
    pthread_mutex_destroy(&locks[THREAD]);

    arena_free(main_arena);
    remove(pipe_name);
  
    printf("[Main] Deallocation completed, exiting.\n");
  
    return 0;
}
