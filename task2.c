#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <unistd.h>

long long balance = 0;

pthread_mutex_t balance_mutex;
pthread_spinlock_t balance_spin;

enum lock_type {
	LOCK_MUTEX,
	LOCK_SPIN
};

enum cs_type {
	CS_SHORT,
	CS_LONG
};

enum lock_type g_lock_type;
enum cs_type g_cs_type;

typedef struct {
	long long iterations;
	// 1 = deposit, 0 = withdraw
	int is_deposit;
} ThreadArgs;

void lock() {
	if (g_lock_type == LOCK_MUTEX) {
		pthread_mutex_lock(&balance_mutex);
	} else {
		pthread_spin_lock(&balance_spin);
	} 
}

void unlock() {
	if (g_lock_type == LOCK_MUTEX) {
		pthread_mutex_unlock(&balance_mutex);
	} else {
		pthread_spin_unlock(&balance_spin);
	}
}

void *worker(void *arg) {
    ThreadArgs *args = (ThreadArgs *)arg;
    for (long long i = 0; i < args->iterations; i++) {
        lock();

        if (args->is_deposit) {
            balance++;
        } else {
            balance--;
        }

        if (g_cs_type == CS_LONG) {
            usleep(100);
        }

        unlock();
    }
    return NULL;
}

int parse_lock_type(const char *s, enum lock_type *out) {
    if (strcmp(s, "mutex") == 0) {
        *out = LOCK_MUTEX;
        return 0;
    }
    if (strcmp(s, "spin") == 0) {
        *out = LOCK_SPIN;
        return 0;
    }
    return -1;
}

int parse_cs_type(const char *s, enum cs_type *out) {
    if (strcmp(s, "short") == 0) {
        *out = CS_SHORT;
        return 0;
    }
    if (strcmp(s, "long") == 0) {
        *out = CS_LONG;
        return 0;
    }
    return -1;
}

int main(int argc, char *argv[]) {
    if (argc < 6) {
        fprintf(stderr,
            "Usage: %s <mutex|spin> <short|long> "
            "<num_deposit_threads> <num_withdraw_threads> <iterations_per_thread>\n",
            argv[0]);
        return 1;
    }

    if (parse_lock_type(argv[1], &g_lock_type) != 0) {
        fprintf(stderr, "Invalid lock type: %s (use mutex or spin)\n", argv[1]);
        return 1;
    }

    if (parse_cs_type(argv[2], &g_cs_type) != 0) {
        fprintf(stderr, "Invalid CS type: %s (use short or long)\n", argv[2]);
        return 1;
    }

    int num_deposit = atoi(argv[3]);
    int num_withdraw = atoi(argv[4]);
    long long iterations = atoll(argv[5]);

    if (num_deposit <= 0 || num_withdraw <= 0 || iterations <= 0) {
        fprintf(stderr, "Thread counts and iterations must be positive.\n");
        return 1;
    }

    int total_threads = num_deposit + num_withdraw;
    pthread_t *threads = malloc(total_threads * sizeof(pthread_t));
    if (!threads) {
        perror("malloc");
        return 1;
    }

    ThreadArgs deposit_args = { .iterations = iterations, .is_deposit = 1 };
    ThreadArgs withdraw_args = { .iterations = iterations, .is_deposit = 0 };

    if (g_lock_type == LOCK_MUTEX) {
        if (pthread_mutex_init(&balance_mutex, NULL) != 0) {
            perror("pthread_mutex_init");
            free(threads);
            return 1;
        }
    } else {
        if (pthread_spin_init(&balance_spin, PTHREAD_PROCESS_PRIVATE) != 0) {
            perror("pthread_spin_init");
            free(threads);
            return 1;
        }
    }

    balance = 0;

    int idx = 0;
    for (int i = 0; i < num_deposit; i++) {
        if (pthread_create(&threads[idx++], NULL, worker, &deposit_args) != 0) {
            perror("pthread_create (deposit)");
            return 1;
        }
    }

    for (int i = 0; i < num_withdraw; i++) {
        if (pthread_create(&threads[idx++], NULL, worker, &withdraw_args) != 0) {
            perror("pthread_create (withdraw)");
            return 1;
        }
    }

    for (int i = 0; i < total_threads; i++) {
        pthread_join(threads[i], NULL);
    }

    long long total_deposits  = (long long)num_deposit * iterations;
    long long total_withdraws = (long long)num_withdraw * iterations;
    long long expected = total_deposits - total_withdraws;

    printf("Lock type      : %s\n", g_lock_type == LOCK_MUTEX ? "mutex" : "spin");
    printf("CS type        : %s\n", g_cs_type == CS_SHORT ? "short" : "long");
    printf("Deposit threads: %d\n", num_deposit);
    printf("Withdraw threads: %d\n", num_withdraw);
    printf("Iterations/thr : %lld\n", iterations);
    printf("Total deposits : %lld\n", total_deposits);
    printf("Total withdraws: %lld\n", total_withdraws);
    printf("Expected bal   : %lld\n", expected);
    printf("Final balance  : %lld\n", balance);

    if (g_lock_type == LOCK_MUTEX) {
        pthread_mutex_destroy(&balance_mutex);
    } else {
        pthread_spin_destroy(&balance_spin);
    }

    free(threads);
    return 0;
}


