#ifndef QUEUE
#define QUEUE

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arena.h>
#include <unistd.h>

struct Queue {
  struct Node *front;
  struct Node *end;
  arena_t *queue_arena;
  struct Node *first_free;
};

struct List {
  struct ListNode *head;
  struct ListNode *tail;
  arena_t *list_arena;
  struct ListNode *first_free;
};

typedef struct ListNode ListNode;
struct ListNode {
  uint8_t threads;
  char *name;
  pthread_t *thread;
  ListNode *next_node;
  ListNode *prev_node;
};

typedef struct Node Node;
struct Node {
  uint8_t threads_needed;
  char *script;
  struct Node *next_node;
  struct Node *prev_node;
};

bool is_empty(const struct Queue *queue) {
  return queue->end == NULL && queue->front == NULL;
}

void queue_sim(struct Queue *queue, char *script) {
  struct Node *node;

  if (queue->first_free != NULL){
      node = queue->first_free;
      queue->first_free = queue->first_free->next_node;
  } else {
      node = arena_push(queue->queue_arena, sizeof(struct Node));
  }

  const uint8_t buffsize = 100;
  char buffer[buffsize];

  node->script = script;
  node->threads_needed = 1;

  FILE *check_threads = fopen(script, "r");

  while (1) {
    if (fgets(buffer, buffsize, check_threads) == NULL) {
      break;
    };
    if (strncmp(buffer, "#RNMN threads = ", 16) == 0) {
      node->threads_needed = (uint8_t)strtoul(&buffer[16], NULL, 10);
      break;
    }
  }
  fclose(check_threads);

  node->prev_node = NULL;
  if (is_empty(queue)) {
    node->next_node = NULL;
    queue->front = node;
  } else {
    node->next_node->prev_node = queue->end;
    node->next_node = queue->end;
  }
  queue->end = node;
}

void dequeue(struct Queue *queue) {
    struct Node *node = queue->front;
    queue->front = queue->front->next_node;

    node->next_node = queue->first_free;
    queue->first_free = node;

    // if we have ran all of the simulations,
    // then the free list should contain the entire queue/arena,
    // so we can get rid of the entire list
    if (is_empty(queue)){
        queue->first_free = NULL;
        arena_clear(queue->queue_arena);
    }
}

void queue_free(struct Queue *queue){
    // all nodes should be allocated on the arena, so deallocing this should get rid of it
    arena_free(queue->queue_arena);
    free(queue);
}

struct Queue *create_queue(uint8_t numSims) {
    arena_t *queue_arena = arena_create_with_capacity(numSims * sizeof(struct Node));
    struct Queue *queue = malloc(sizeof(struct Queue));
    queue->front = NULL;
    queue->end = NULL;
    queue->queue_arena = queue_arena;
    queue->first_free = NULL;

    return queue;
}

struct List *create_list(uint8_t numSims) {
  struct List *list = malloc(sizeof(struct List));
  arena_t *arena = arena_create_with_capacity(numSims * sizeof(ListNode));

  list->head = NULL;
  list->first_free = NULL;
  list->list_arena = arena;

  return list;
}

void list_free(struct List *list){
    // all nodes should be allocated on the arena, so deallocing this should get rid of it
    arena_free(list->list_arena);
    free(list);
}

void push(struct List *list, char *script, uint8_t threads,
          pthread_t *running_thread) {
    ListNode *node;
  if (list->first_free != NULL){
      node = list->first_free;
      list->first_free = list->first_free->next_node;
  } else {
      node = arena_push(list->list_arena, sizeof(ListNode));
  }

  thread_counter += threads;

  *node = (ListNode){.threads = threads,
                     .name = script,
                     .thread = running_thread,
                     .next_node = list->head,
                     .prev_node = NULL};

  if (list->head == NULL)
      list->tail = node;

  if (list->head != NULL)
    list->head->prev_node = node;

  list->head = node;
}

void remove_node(struct List *list, char *name) {
  ListNode *traveler = list->head;

  while (traveler->next_node != NULL) {
    if (strcmp(traveler->name, name) == 0) {
      traveler->prev_node->next_node = traveler->next_node;
      traveler->next_node->prev_node = traveler->prev_node;

      if (traveler == list->tail){
          arena_pop(list->list_arena, sizeof(ListNode));
      } else {
          traveler->next_node = list->first_free;
          list->first_free = traveler;
      }
      break;
    }
  }
}
#endif /* ifndef QUEUE                                                         \
#define QUEUE */
