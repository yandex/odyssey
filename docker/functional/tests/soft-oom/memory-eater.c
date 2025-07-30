#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <stdint.h>
#include <string.h>
#include <signal.h>

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/wait.h>

#define SHMEM_PATH "/memory-eater-shmem"
#define SHMEM_SIZE (50 * 1024 * 1024)
#define PRIVATE_SIZE (20 * 1024 * 1024)

void child_alloc_handler() {
    uint8_t *private = malloc(PRIVATE_SIZE);
    memset(private, 52, PRIVATE_SIZE);
    memset(private, 53, PRIVATE_SIZE);
    memset(private, 54, PRIVATE_SIZE);
    memset(private, 55, PRIVATE_SIZE);
}

void child_process(uint8_t *shared, int i) {
    memset(shared, i, SHMEM_SIZE);
    sigset(SIGUSR1, child_alloc_handler);
    while (1) {
        sleep(1);
    }
}

int main() {
    int shm_fd = shm_open(SHMEM_PATH, O_CREAT | O_RDWR, 0600);
    if (shm_fd == -1) {
        perror("shm_open");
        exit(1);
    }
    ftruncate(shm_fd, SHMEM_SIZE);
    uint8_t *shared = mmap(NULL, SHMEM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    close(shm_fd);

    pid_t pids[3];
    for (int i = 0; i < 3; ++i) {
        if ((pids[i] = fork()) == 0) {
            child_process(shared, i);
        }
    }

    memset(shared, 5, SHMEM_SIZE);

    child_alloc_handler();
    sigset(SIGUSR1, child_alloc_handler);

    for (int i = 0; i < 3; ++i) {
        waitpid(pids[i], NULL, 0);
    }

    munmap(shared, SHMEM_SIZE);
    shm_unlink(SHMEM_PATH);
    return 0;
}