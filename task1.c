#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>

long long counter = 0;

pthread_mutex_t counter_mutex;
pthread_spinlock_t counter_spin;

typedef struct {
	long long iterations;
	// 0 = no sync; 1 = mutex; 2 = spinlock
	int mode;
} ThreadArgs;

void *worker(void *arg) {
	ThreadArgs *args = (ThreadArgs *)arg;
	long long i;

	for (i = 0; i < args->iterations; i++) {
		if (args->mode == 0) {
			counter++;
		} else if (args->mode == 1) {
			pthread_mutex_lock(&counter_mutex);
			counter++;
			pthread_mutex_unlock(&counter_mutex);
		} else if (args->mode == 2) {
			pthread_spin_lock(&counter_spin);
			counter++;
			pthread_spin_unlock(&counter_spin);
		}
	}

	return NULL;
}

int parse_mode(const char *s) {
	if (strcmp(s, "nosync") == 0 || strcmp(s, "0") == 0) return 0;
	if (strcmp(s, "mutex") == 0 || strcmp(s, "1") == 0) return 1;
	if (strcmp(s, "spin") == 0 || strcmp(s, "2") == 0) return 2;
}

int main(int argc, char *argv[]) {
    if (argc < 4) {
	fprintf(stderr, "Usage: %s <mode: nosync|mutex|spin> <N_threads> <M_iterations>\n", argv[0]);
	return 1;
    }

    int mode = parse_mode(argv[1]);
    if (mode == -1) {
        fprintf(stderr, "Invalid mode: %s (use nosync|mutex|spin or 0|1|2)\n", argv[1]);
        return 1;
    }

    int N = atoi(argv[2]);
    long long M = atoll(argv[3]);

    if (N <= 0 || M <= 0) {
        fprintf(stderr, "N and M must be positive.\n");
        return 1;
    }

    pthread_t *threads = malloc(N * sizeof(pthread_t));
    if (!threads) {
        perror("malloc");
        return 1;
    }

    ThreadArgs args;
    args.iterations = M;
    args.mode = mode;

    if (mode == 1) {
        if (pthread_mutex_init(&counter_mutex, NULL) != 0) {
            perror("pthread_mutex_init");
            free(threads);
            return 1;
        }
    } else if (mode == 2) {
        if (pthread_spin_init(&counter_spin, PTHREAD_PROCESS_PRIVATE) != 0) {
            perror("pthread_spin_init");
            free(threads);
            return 1;
        }
    }

    counter = 0;

    for (int i = 0; i < N; i++) {
        if (pthread_create(&threads[i], NULL, worker, &args) != 0) {
            perror("pthread_create");
            return 1;
        }
    }

    for (int i = 0; i < N; i++) {
        pthread_join(threads[i], NULL);
    }

    long long expected = (long long)N * M;
    printf("Mode: %s\n", (mode == 0 ? "No sync" :
                           mode == 1 ? "Mutex"   :
                                       "Spinlock"));
    printf("N (threads)   = %d\n", N);
    printf("M (per thread)= %lld\n", M);
    printf("Expected      = %lld\n", expected);
    printf("Actual counter= %lld\n", counter);

    if (mode == 1) {
        pthread_mutex_destroy(&counter_mutex);
    } else if (mode == 2) {
        pthread_spin_destroy(&counter_spin);
    }

    free(threads);
    return 0;
}
