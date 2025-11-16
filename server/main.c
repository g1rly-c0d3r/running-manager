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

    pipe(watchPipe);
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

      // =====================================================================================
      // check_watcher(watch_pipe[0], args.loglevel, simQueue) { 
      // command_buff should be declared in check_watcher
        // buffer to recive commands from the watcher thread
      const uint8_t buffsize = 255;
      char command_buff[buffsize];
  
      // so that if the command from the prev iteration doesn't get ran.
      command_buff[0] = 0;
      if (canReadFromPipe(watchPipe[0])) {
        read(watchPipe[0], command_buff, buffsize);
      }
  
      switch (strtoul(command_buff, NULL, 10)) {
      case EXIT:
        // TODO: kill running sims before exiting
        if (args.log_level >= 1)
            printf("[Main] Exit command recived.\n");
        goto dealloc;
      case STATUS:
        if (args.log_level >= 1)
            printf("[Main] Status command recived\n");

        print_status(runningList, simQueue);
        break;
      case RUN:
        if (args.log_level >= 1)
            printf("[Main] Run command recived, name: %s", &command_buff[1]);
        // command_buff holds the command enum in the first element,
        // and the script name follows it.
        queue_sim(simQueue, &command_buff[1]); // this should probably be outside
      }

      // } // check_watcher
      // ===========================================================================================
  
      // ==========================================================================================
      // run_next_sim(simQueue, locks[THREAD], args.numthreads, runningList){
      if (!is_empty(simQueue)) {
        pthread_mutex_lock(&locks[THREAD]);
        if (args.num_threads - simQueue->front->threads_needed >= 0) {
          thread_counter -= simQueue->front->threads_needed;
          run_sim(simQueue->front->script, simQueue->front->threads_needed,
                  runningList, &locks[THREAD]);
          dequeue(simQueue);
        }
        pthread_mutex_unlock(&locks[THREAD]);
      } else if (args.log_level >= 3) {
        printf("[Main] Simulation queue empty! Nothing to run.\n");
      }
      // } // run_next_sim
      // ===========================================================================================
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
