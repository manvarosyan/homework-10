#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>

#define BUFFER_SIZE 8

int buffer[BUFFER_SIZE];
int in_pos  = 0;
int out_pos = 0;

sem_t empty_slots; 
sem_t full_slots;  
pthread_mutex_t buffer_mutex;

long long total_items = 0;
long long consumed_count = 0;

typedef struct {
    int id;
    long long items_to_produce_or_consume;
    // 1 = producer, 0 = consumer
    int is_producer;  
} ThreadArgs;

void *producer(void *arg) {
    ThreadArgs *args = (ThreadArgs *)arg;
    for (long long i = 0; i < args->items_to_produce_or_consume; i++) {
        int item = args->id * 1000000 + (int)i;

        sem_wait(&empty_slots);
        pthread_mutex_lock(&buffer_mutex);

        buffer[in_pos] = item;
        in_pos = (in_pos + 1) % BUFFER_SIZE;

    
        printf("Producer %d produced %d\n", args->id, item);

        pthread_mutex_unlock(&buffer_mutex);
        sem_post(&full_slots);
    }
    return NULL;
}

void *consumer(void *arg) {
    ThreadArgs *args = (ThreadArgs *)arg;

    while (1) {
        pthread_mutex_lock(&buffer_mutex);
        if (consumed_count >= total_items) {
            pthread_mutex_unlock(&buffer_mutex);
            break;
        }
        pthread_mutex_unlock(&buffer_mutex);

        sem_wait(&full_slots);
        pthread_mutex_lock(&buffer_mutex);

        int item = buffer[out_pos];
        out_pos = (out_pos + 1) % BUFFER_SIZE;
        consumed_count++;

        printf("Consumer %d consumed %d\n", args->id, item);

        pthread_mutex_unlock(&buffer_mutex);
        sem_post(&empty_slots);
    }

    return NULL;
}

int main(int argc, char *argv[]) {
    if (argc < 4) {
        fprintf(stderr,
            "Usage: %s <num_producers P> <num_consumers C> <items_per_producer K>\n",
            argv[0]);
        return 1;
    }

    int P = atoi(argv[1]);
    int C = atoi(argv[2]);
    long long K = atoll(argv[3]);

    if (P <= 0 || C <= 0 || K <= 0) {
        fprintf(stderr, "P, C, and K must be positive integers.\n");
        return 1;
    }

    total_items = (long long)P * K;

    pthread_t *threads = malloc((P + C) * sizeof(pthread_t));
    ThreadArgs *args   = malloc((P + C) * sizeof(ThreadArgs));
    if (!threads || !args) {
        perror("malloc");
        return 1;
    }

    if (sem_init(&empty_slots, 0, BUFFER_SIZE) != 0) {
        perror("sem_init empty_slots");
        return 1;
    }
    if (sem_init(&full_slots, 0, 0) != 0) {
        perror("sem_init full_slots");
        return 1;
    }
    if (pthread_mutex_init(&buffer_mutex, NULL) != 0) {
        perror("pthread_mutex_init");
        return 1;
    }

    int idx = 0;
    for (int i = 0; i < P; i++) {
        args[idx].id = i;
        args[idx].items_to_produce_or_consume = K;
        args[idx].is_producer = 1;
        if (pthread_create(&threads[idx], NULL, producer, &args[idx]) != 0) {
            perror("pthread_create producer");
            return 1;
        }
        idx++;
    }

    for (int i = 0; i < C; i++) {
        args[idx].id = i;
        args[idx].items_to_produce_or_consume = 0;
        args[idx].is_producer = 0;
        if (pthread_create(&threads[idx], NULL, consumer, &args[idx]) != 0) {
            perror("pthread_create consumer");
            return 1;
        }
        idx++;
    }

    for (int i = 0; i < P + C; i++) {
        pthread_join(threads[i], NULL);
    }

    printf("P (producers)      = %d\n", P);
    printf("C (consumers)      = %d\n", C);
    printf("K (per producer)   = %lld\n", K);
    printf("Total items        = %lld\n", total_items);
    printf("Consumed count     = %lld\n", consumed_count);
    printf("in_pos             = %d\n", in_pos);
    printf("out_pos            = %d\n", out_pos);

    pthread_mutex_destroy(&buffer_mutex);
    sem_destroy(&empty_slots);
    sem_destroy(&full_slots);
    free(threads);
    free(args);

    return 0;
}

