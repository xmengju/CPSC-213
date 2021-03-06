#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <assert.h>
#include <time.h>
#include "uthread.h"
#include "uthread_sem.h"

#define NUM_ITERATIONS 1000
#define TOBACCO 0
#define PAPER 1
#define MATCHES 2

static uthread_t threads[4];
static uthread_sem_t lock;
static uthread_sem_t resource[3];
static uthread_sem_t done_smoking;

static int num_woken = 0;
static int resource_of_first = -1;
static int histo_expected[3] = { 0, 0, 0 };
static int histo_actual[3] = { 0, 0, 0 };

void smoke(int res) {
  histo_actual[res]++;
  #ifdef VERBOSE
  printf("%s smoker is smoking.\n",
         res == 0 ? "Tobacco" : (res == 1 ? "Paper" : "Matches"));
  #endif
}

void* smoker(void* resource_type_vp) {
  int resource_type = (intptr_t) resource_type_vp;

  for (int i = 0; i < NUM_ITERATIONS; i++) {
    uthread_sem_wait(resource[resource_type]);
    uthread_sem_wait(lock);
    switch (num_woken) {
      case 0:
        num_woken++;
        resource_of_first = resource_type;
        break;
      case 1:
        num_woken++;
        switch(resource_of_first * 10 + resource_type) {
          case 01:
          case 10:
            uthread_sem_signal(resource[2]);
            break;
          case 02:
          case 20:
            uthread_sem_signal(resource[1]);
            break;
          case 12:
          case 21:
            uthread_sem_signal(resource[0]);
            break;
        }
        break;
      case 2:
        num_woken = 0;
        resource_of_first = -1;
        smoke(resource_type);
        uthread_sem_signal(done_smoking);
        break;
    }
    uthread_sem_signal(lock);
  }
  return NULL;
}

void* agent() {
  srand(time(NULL));
  for (int i = 0; i < NUM_ITERATIONS; i++) {
    int random = rand() % 3;
    histo_expected[(random+2) % 3]++;
    uthread_sem_signal(resource[random]);
    uthread_sem_signal(resource[(random+1) % 3]);
    uthread_sem_wait(done_smoking);
  }
  return NULL;
}

int main (int argc, char** argv) {
  // setup mutex and condition variables
  lock = uthread_sem_create(1);
  for (int i = 0; i < 3; i++) {
    resource[i] = uthread_sem_create(0);
  }
  done_smoking = uthread_sem_create(0);

  // setup threads and start agent
  uthread_init(1);
  for (int i = 0; i < 3; i++) {
    threads[i] = uthread_create(smoker, (void*)(intptr_t) i);
  }
  threads[3] = uthread_create(agent, NULL);

  // wait for threads to finish
  for (int i = 3; i >= 0; i--) {
    uthread_join(threads[i], NULL);
  }

  // verify that the histograms of actual/expected match
  for (int i = 0; i < 3; i++) {
    assert(histo_expected[i] == histo_actual[i]);
  }

  // print histograms of expected and actual smoke counts
  int sum;
  printf("Expected: [ ");
  sum = 0;
  for (int i = 0; i < 3; i++) {
    sum += histo_expected[i];
    printf("%d ", histo_expected[i]);
  }
  printf("]\tSum: %d\n", sum);

  sum = 0;
  printf("Actual:   [ ");
  for (int i = 0; i < 3; i++) {
    sum += histo_actual[i];
    printf("%d ", histo_actual[i]);
  }
  printf("]\tSum: %d\n", sum);
}
