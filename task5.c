#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>

sem_t printers;                   
pthread_mutex_t count_mutex;       
int current_in_use = 0;           
int K = 0;                       

typedef struct {
    int id;
} ThreadArgs;

void *print_job(void *arg) {
    ThreadArgs *args = (ThreadArgs *)arg;

    sem_wait(&printers);

    pthread_mutex_lock(&count_mutex);
    current_in_use++;
    int val = current_in_use;
    pthread_mutex_unlock(&count_mutex);

    printf("Thread %d is printing... (printers in use: %d)\n", args->id, val);

    usleep(100000);

    pthread_mutex_lock(&count_mutex);
    current_in_use--;
    pthread_mutex_unlock(&count_mutex);

    sem_post(&printers);

    return NULL;
}

int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <num_printers K> <num_jobs N>\n", argv[0]);
        return 1;
    }

    K = atoi(argv[1]);
    int N = atoi(argv[2]);

    if (K <= 0 || N <= 0) {
        fprintf(stderr, "K and N must be positive.\n");
        return 1;
    }

    pthread_mutex_init(&count_mutex, NULL);

    sem_init(&printers, 0, K);

    pthread_t *threads = malloc(N * sizeof(pthread_t));
    ThreadArgs *args   = malloc(N * sizeof(ThreadArgs));

    for (int i = 0; i < N; i++) {
        args[i].id = i;
        pthread_create(&threads[i], NULL, print_job, &args[i]);
    }

    for (int i = 0; i < N; i++) {
        pthread_join(threads[i], NULL);
    }

    sem_destroy(&printers);
    pthread_mutex_destroy(&count_mutex);

    free(threads);
    free(args);

    return 0;
}

