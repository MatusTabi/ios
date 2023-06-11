/*
 *   Project: IOS_proj2
 *   Author: Matus Tabi
 */
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
#include <stdbool.h>

FILE *f;

struct my_struct {
    int line; // number of line
    int oxygen_count; // count of oxygens in queue
    int hydrogen_count; // count of hydrogens in queue
    int count; // count number of processes in barrier
    int molecule_counter; // count number of molecules during creating
    int mol_count; // count how many molecules will be created
    int shared_NO; // number of oxygens entered in shared memory
    int shared_NH; // number of hydrogens entered in shared memory
    sem_t line_semaphore; // semaphore for correct line printing
    sem_t turnstile; // first turnstile in barrier
    sem_t turnstile_2; // second turnstile in barrier
    sem_t barrier_mutex; // barrier semaphore
    sem_t molecule_mutex; // semaphore in molecule creating
    sem_t oxygen_semaphore; // semaphore where oxygens wait
    sem_t hydrogen_semaphore; // semaphore where hydrogens wait
    bool flag; // helping variable when writing "not enough"
};

// initialization of semaphores

void semaphores_init(struct my_struct *water) {
    sem_init(&water->line_semaphore, 1, 1);
    sem_init(&water->turnstile, 1, 0);
    sem_init(&water->turnstile_2, 1, 1);
    sem_init(&water->barrier_mutex, 1, 1);
    sem_init(&water->molecule_mutex, 1, 1);
    sem_init(&water->oxygen_semaphore, 1, 0);
    sem_init(&water->hydrogen_semaphore, 1, 0);
}

// cleaning semaphores, shared memory and closing file

void cleanup(struct my_struct *water) {
    sem_destroy(&water->line_semaphore);
    sem_destroy(&water->turnstile);
    sem_destroy(&water->turnstile_2);
    sem_destroy(&water->barrier_mutex);
    sem_destroy(&water->molecule_mutex);
    sem_destroy(&water->oxygen_semaphore);
    sem_destroy(&water->hydrogen_semaphore);
    munmap(water, sizeof(water));
    fclose(f);
}

// checking correct number of arguments

void check_number_of_arguments(int argc) {
    if (argc != 5) {
        fprintf(stderr, "Wrong number of arguments\n");
        exit(EXIT_FAILURE);
    }
}

// checking, whether the arguments are entered correctly using strtol function
// if yes, function will return correct value

long check_correct_agruments(char **argv, int i) {
    char *endptr = NULL;
    long value = strtol(argv[i], &endptr, 10);
    if (endptr[0] != '\0') {
        fprintf(stderr,"Arguments are not numbers\n");
        exit(EXIT_FAILURE);
    }
    if (value < 0) {
        fprintf(stderr, "Arguments are negative number\n");
        exit(EXIT_FAILURE);
    }
    if (i == 1 || i == 2) {
        if (value == 0) {
            fprintf(stderr,"No oxygens and no hydrogens were entered\n");
            exit(EXIT_FAILURE);
        }
    }
    if (i == 3 || i == 4) {
        if (value < 0 || value > 1000) {
            fprintf(stderr,"Time arguments are wrong\n");
            exit(EXIT_FAILURE);
        }
    }
    return value;
}

void wait_random_time(long sleep_time) {
    srand(time(NULL) * getpid());
    usleep(1000 * (rand() % (sleep_time + 1)));
}

// functions, which simulate and write "going to queue"

void oxygen_queue(struct my_struct *water, int id, long TI) {
    wait_random_time(TI);
    sem_wait(&water->line_semaphore);
    fprintf(f,"%d: O %d: going to queue \n", ++water->line, id);
    sem_post(&water->line_semaphore);
}

void hydrogen_queue(struct my_struct *water, int id, long TI) {
    wait_random_time(TI);
    sem_wait(&water->line_semaphore);
    fprintf(f,"%d: H %d: going to queue \n", ++water->line, id);
    sem_post(&water->line_semaphore);
}

// barrier function
// inspired by LittleBookOfSemaphores

void barrier(struct my_struct *water, int n) {
    sem_wait(&water->barrier_mutex);
    water->count++;
    if (n == water->count) {
        sem_wait(&water->turnstile_2);
        sem_post(&water->turnstile);
    }
    sem_post(&water->barrier_mutex);

    sem_wait(&water->turnstile);
    sem_post(&water->turnstile);

    sem_wait(&water->barrier_mutex);
    water->count--;
    if (water->count == 0) {
        sem_wait(&water->turnstile);
        sem_post(&water->turnstile_2);
    }
    sem_post(&water->barrier_mutex);

    sem_wait(&water->turnstile_2);
    sem_post(&water->turnstile_2);
}

// functions, which simulate and write "molecule created"

void oxygen_created(struct my_struct *water, int id, long TB) {
    wait_random_time(TB);
    sem_wait(&water->line_semaphore);
    fprintf(f,"%d: O %d: molecule %d created\n", ++water->line, id, water->molecule_counter);
    sem_post(&water->line_semaphore);
}

void hydrogen_created(struct my_struct *water, int id, long TB) {
    wait_random_time(TB);
    sem_wait(&water->line_semaphore);
    fprintf(f,"%d: H %d: molecule %d created\n", ++water->line, id, water->molecule_counter);
    sem_post(&water->line_semaphore);
}

// functions, which write "creating molecule"

void oxygen_creating(struct my_struct *water, int id) {
    sem_wait(&water->line_semaphore);
    fprintf(f,"%d: O %d: creating molecule %d\n", ++water->line, id, water->molecule_counter);
    sem_post(&water->line_semaphore);
}

void hydrogen_creating(struct my_struct *water, int id) {
    sem_wait(&water->line_semaphore);
    fprintf(f,"%d: H %d: creating molecule %d\n", ++water->line, id, water->molecule_counter);
    sem_post(&water->line_semaphore);
}

// helping function during "not enough" problem
// which releases every unused atom

void release(struct my_struct *water) {
    for (int i = 0; i < water->shared_NO - water->molecule_counter; i++) {
        sem_post(&water->oxygen_semaphore);
    }
    for (int i = 0; i < water->shared_NH - water->molecule_counter*2; i++) {
        sem_post(&water->hydrogen_semaphore);
    }
}

// functions, which creates molecules of water
// inspired by LittleBookOfSemaphores

void oxygen_molecule(struct my_struct *water, int id, long TB) {
    sem_wait(&water->molecule_mutex);
    water->oxygen_count++;
    if (water->hydrogen_count >= 2) {
        sem_post(&water->hydrogen_semaphore);
        sem_post(&water->hydrogen_semaphore);
        water->hydrogen_count -= 2;
        sem_post(&water->oxygen_semaphore);
        water->oxygen_count--;
    }
    else {
        sem_post(&water->molecule_mutex);
    }
    sem_wait(&water->oxygen_semaphore);
    if (water->flag == true) {
        sem_wait(&water->line_semaphore);
        fprintf(f,"%d: O %d: not enough H\n", ++water->line, id);
        sem_post(&water->line_semaphore);
        exit(EXIT_SUCCESS);
    }
    oxygen_creating(water, id);
    barrier(water, 3);
    oxygen_created(water, id, TB);
    barrier(water, 3);
    if (water->mol_count == water->molecule_counter) {
        water->flag = true;
        release(water);
    }
    sem_post(&water->molecule_mutex);
    water->molecule_counter++;
}

void hydrogen_molecule(struct my_struct *water, int id, long TB) {
    sem_wait(&water->molecule_mutex);
    water->hydrogen_count++;
    if (water->hydrogen_count >= 2 && water->oxygen_count >= 1) {
        sem_post(&water->hydrogen_semaphore);
        sem_post(&water->hydrogen_semaphore);
        water->hydrogen_count -= 2;
        sem_post(&water->oxygen_semaphore);
        water->oxygen_count--;
    }
    else {
        sem_post(&water->molecule_mutex);
    }
    sem_wait(&water->hydrogen_semaphore);
    if (water->flag == true) {
        sem_wait(&water->line_semaphore);
        fprintf(f,"%d: H %d: not enough O or H\n", ++water->line, id);
        sem_post(&water->line_semaphore);
        exit(EXIT_SUCCESS);
    }
    hydrogen_creating(water, id);
    barrier(water, 3);
    hydrogen_created(water, id, TB);
    barrier(water, 3);
}

// main oxygen function, which controls every oxygen processes

void oxygen(struct my_struct *water, int id, long TI, long TB) {
    sem_wait(&water->line_semaphore);
    fprintf(f,"%d: O %d: started \n", ++water->line, id);
    sem_post(&water->line_semaphore);
    oxygen_queue(water, id, TI);
    oxygen_molecule(water, id, TB);
}

// main hydrogen function, which controls every oxygen processes

void hydrogen(struct my_struct *water, int id, long TI, long TB) {
    sem_wait(&water->line_semaphore);
    fprintf(f,"%d: H %d: started \n", ++water->line, id);
    sem_post(&water->line_semaphore);
    hydrogen_queue(water, id, TI);
    hydrogen_molecule(water, id, TB);
}

// helping function, which calculates, how many molecules will be created

void calc_molecule(struct my_struct *water, long NO, long NH) {
    long temp = NO;
    for (int i = 1; i <= temp; i++) {
        if ((NO >= 1) && (NH >= 2)) {
            water->mol_count++;
        }
        else {
            break;
        }
        NO -= 1;
        NH -= 2;
    }
}

int main(int argc, char **argv) {
    struct my_struct *water = mmap(NULL, sizeof(water), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1,0);
    semaphores_init(water);
    // initilization of arguments and variables in shared memory
    check_number_of_arguments(argc);
    long NO = check_correct_agruments(argv, 1);
    long NH = check_correct_agruments(argv, 2);
    long TI = check_correct_agruments(argv, 3);
    long TB = check_correct_agruments(argv, 4);
    water->line = 0;
    water->count = 0;
    water->hydrogen_count = 0;
    water->oxygen_count = 0;
    water->molecule_counter = 1;
    water->mol_count = 0;
    water->shared_NO = NO;
    water->shared_NH = NH;
    water->flag = false;
    calc_molecule(water, NO, NH);
    // opening proj2.out file
    if ((f = fopen("proj2.out","w")) == NULL) {
        fprintf(stderr,"Failed to open file\n");
        exit(EXIT_FAILURE);
    }
    setbuf(f, NULL);
    // forking main process
    for (int i = 1; i <= NO; i++) {
        pid_t pid = fork();
        if (pid == 0) {
            oxygen(water, i, TI, TB);
            exit(EXIT_SUCCESS);
        }
    }
    for (int i = 1; i <= NH; i++) {
        pid_t pid = fork();
        if (pid == 0) {
            hydrogen(water, i, TI, TB);
            exit(EXIT_SUCCESS);
        }
    }
    while (wait(NULL) > 0);
    cleanup(water);
    return 0;
}