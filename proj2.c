#define _GNU_SOURCE
#include <pthread.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <semaphore.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/wait.h>

#define NUMBER_OF_ARGUMENTS 5

FILE *f;

struct my_struct {
    int line;
    int oxygen_count;
    int hydrogen_count;
    sem_t oxy_semaphore;
    sem_t hydro_semaphore;
    sem_t mutex;
    sem_t line_mutex;
    sem_t mutex3;
};

void check_number_of_arguments(int argc) {
    if (argc != 5) {
        fprintf(stderr, "Wrong arguments\n");
        exit(EXIT_FAILURE);
    }
}

long check_correct_agruments(char **argv, int i) {
    char *endptr = NULL;
    long value = strtol(argv[i], &endptr, 10);
    if (endptr[0] != '\0') {
        fprintf(stderr,"Arguments are not numbers\n");
        exit(EXIT_FAILURE);
    }
    if (i == 3 || i == 4) {
        if (value < 0 || value > 1000) {
            fprintf(stderr,"Time arguments are wrong\n");
            exit(EXIT_FAILURE);
        }
    }
    return value;
}

void ti_sleep(struct my_struct *water, int id, long TI) {
    srand(time(NULL) * getpid());
    int time =rand() % TI;
    usleep(1000 * time);
    sem_wait(&water->mutex3);
    fprintf(f,"%d: O %d: going to queue \n",water->line, id);
    water->line++;
    sem_post(&water->mutex3);
}

void oxygen_queue(struct my_struct *water, int id, long TI) {
    sem_wait(&water->mutex);
    sem_wait(&water->line_mutex);
    fprintf(f,"%d: O %d: started \n",water->line, id);
    water->line++;
    sem_post(&water->line_mutex);
    sem_post(&water->mutex);
    ti_sleep(water, id, TI);
}

void hydrogen_queue(struct my_struct *water, int id) {
    sem_post(&water->mutex);
    sem_wait(&water->line_mutex);
    fprintf(f,"%d: H %d: started \n",water->line, id);
    water->line++;
    sem_post(&water->line_mutex);
}

int main(int argc, char **argv) {
    check_number_of_arguments(argc);
    long NO = check_correct_agruments(argv, 1);
    long NH = check_correct_agruments(argv, 2);
    long TI = check_correct_agruments(argv, 3);
    //long TB = check_correct_agruments(argv, 4);

    if ((f = fopen("proj2.out","w")) == NULL) {
        fprintf(stderr,"Failed to open file\n");
        return 1;
    }

    setbuf(f, NULL);

    struct my_struct *water = mmap(NULL, sizeof(water), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1,0);
    water->line = 1;
    sem_init(&water->mutex, 1, 0);
    sem_init(&water->line_mutex, 1, 1);
    sem_init(&water->mutex3, 1, 1);

    for (int i = 1; i <= NO; i++) {
        pid_t pid = fork();
        if (pid == 0) {
            oxygen_queue(water, i, TI);
            exit(EXIT_SUCCESS);
        }
    }
    for (int i = 1; i <= NH; i++) {
        pid_t pid = fork();
        if (pid == 0) {
            hydrogen_queue(water,i);
            exit(EXIT_SUCCESS);
        }
    }
    sem_destroy(&water->mutex);
    sem_destroy(&water->line_mutex);
    sem_destroy(&water->mutex3);
    while (wait(NULL) > 0);
    munmap(water, sizeof(water));
    fclose(f);
    return 0;
}