#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <time.h>
#endif

typedef struct {
    int id;
    int attempts;
    int local_sold;
    int local_failed;
} WorkerArgs;

static int initial_tickets = 100;
static int tickets_left = 100;
static pthread_mutex_t ticket_lock = PTHREAD_MUTEX_INITIALIZER;

static void tiny_delay(int seed) {
    volatile int waste = 0;
    for (int i = 0; i < 400 + (seed % 200); i++) {
        waste += i;
    }
}

static void widen_race_window(void) {
#ifdef _WIN32
    Sleep(1);
#else
    struct timespec delay = {0, 1000000};
    nanosleep(&delay, NULL);
#endif
}

static void *unsafe_worker(void *raw_args) {
    WorkerArgs *args = (WorkerArgs *)raw_args;
    int sold = 0;
    int failed = 0;

    for (int attempt = 0; attempt < args->attempts; attempt++) {
        int observed = tickets_left;
        if (observed > 0) {
            tiny_delay(args->id + attempt);
            widen_race_window();
            tickets_left = observed - 1;
            sold++;
        } else {
            failed++;
        }
    }

    args->local_sold = sold;
    args->local_failed = failed;
    return NULL;
}

static void *mutex_worker(void *raw_args) {
    WorkerArgs *args = (WorkerArgs *)raw_args;
    int sold = 0;
    int failed = 0;

    for (int attempt = 0; attempt < args->attempts; attempt++) {
        pthread_mutex_lock(&ticket_lock);
        if (tickets_left > 0) {
            tiny_delay(args->id + attempt);
            tickets_left--;
            sold++;
        } else {
            failed++;
        }
        pthread_mutex_unlock(&ticket_lock);
    }

    args->local_sold = sold;
    args->local_failed = failed;
    return NULL;
}

static int parse_positive(const char *raw, const char *name) {
    char *end = NULL;
    long value = strtol(raw, &end, 10);
    if (*raw == '\0' || *end != '\0' || value <= 0 || value > 1000000) {
        fprintf(stderr, "%s must be a positive integer, got '%s'\n", name, raw);
        exit(2);
    }
    return (int)value;
}

int main(int argc, char **argv) {
    const char *mode = argc > 1 ? argv[1] : "unsafe";
    initial_tickets = argc > 2 ? parse_positive(argv[2], "tickets") : 100;
    int worker_count = argc > 3 ? parse_positive(argv[3], "threads") : 16;
    int attempts = argc > 4 ? parse_positive(argv[4], "attempts") : 80;

    if (strcmp(mode, "unsafe") != 0 && strcmp(mode, "mutex") != 0) {
        fprintf(stderr, "Usage: %s [unsafe|mutex] [tickets] [threads] [attempts]\n", argv[0]);
        return 2;
    }

    tickets_left = initial_tickets;

    pthread_t *threads = calloc((size_t)worker_count, sizeof(pthread_t));
    WorkerArgs *args = calloc((size_t)worker_count, sizeof(WorkerArgs));
    if (threads == NULL || args == NULL) {
        fprintf(stderr, "allocation failed\n");
        free(threads);
        free(args);
        return 1;
    }

    for (int i = 0; i < worker_count; i++) {
        args[i].id = i;
        args[i].attempts = attempts;
        void *(*worker)(void *) = strcmp(mode, "mutex") == 0 ? mutex_worker : unsafe_worker;
        int rc = pthread_create(&threads[i], NULL, worker, &args[i]);
        if (rc != 0) {
            fprintf(stderr, "pthread_create failed at worker %d\n", i);
            free(threads);
            free(args);
            return 1;
        }
    }

    int sold = 0;
    int failed = 0;
    for (int i = 0; i < worker_count; i++) {
        pthread_join(threads[i], NULL);
        sold += args[i].local_sold;
        failed += args[i].local_failed;
    }

    printf("C ticket demo (%s)\n", mode);
    printf("  tickets=%d, threads=%d, attempts_per_thread=%d, total_requests=%d\n",
           initial_tickets, worker_count, attempts, worker_count * attempts);
    printf("  sold=%d\n", sold);
    printf("  failed/no ticket=%d\n", failed);
    printf("  remaining=%d\n", tickets_left);
    printf("  invariant sold + remaining == initial: %s\n",
           (sold + tickets_left == initial_tickets) ? "true" : "false");
    printf("  oversold: %s\n", sold > initial_tickets ? "true" : "false");

    free(threads);
    free(args);
    return 0;
}
