
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/time.h> // for gettimeofday

typedef struct _thread_data_t {
    const int *data;
    int startInd;
    int endInd;
    pthread_mutex_t *lock;
    long long int *totalSum;
} thread_data_t;

struct timeval start, end;

int readFile(char filename[], int data[]);
void* arraySum(void *arg);

#define MAX_VALUES 100000000 // max values you can read from a file

int main(int argc, char *argv[]) {
    if (argc != 3) {
        printf("Usage: %s <filename> <number of threads>\n", argv[0]);
        return -1;
    }

    int *data = (int *) malloc(MAX_VALUES * sizeof(int));
    if (!data) {
        perror("Failed to allocate memory for data");
        return -1;
    }
    
    int numberOfValues = readFile(argv[1], data);
    
    if (numberOfValues == -1 || numberOfValues == MAX_VALUES) {
        free(data);
        return -1;
    }

    int numberOfThreads = atoi(argv[2]);
    
    if (numberOfThreads > numberOfValues) {
        printf("Too many threads requested\n");
        free(data);
        return -1;
    }

    long long int totalSum = 0;
    struct timeval start, end;
    pthread_mutex_t lock;
    pthread_t threads[numberOfThreads];
    thread_data_t threadData[numberOfThreads];

    gettimeofday(&start, NULL); // store the current time

    if (pthread_mutex_init(&lock, NULL) != 0) {
        perror("Mutex initialization failed");
        free(data);
        return -1;
    }

    int valuesPerThread = numberOfValues / numberOfThreads;
    int createdThreads = 0; // To keep track of successfully created threads
    for (int i = 0; i < numberOfThreads; i++) {
        threadData[i].data = data;
        threadData[i].startInd = i * valuesPerThread;
        // If it's the last thread, handle all remaining values
        // Otherwise, handle valuesPerThread values
        threadData[i].endInd = (i == numberOfThreads - 1) ? numberOfValues - 1 : (i + 1) * valuesPerThread - 1;
        threadData[i].lock = &lock;     // Assigning the mutex lock
        threadData[i].totalSum = &totalSum; // Assigning the total sum address
        
        if (pthread_create(&threads[i], NULL, arraySum, &threadData[i]) != 0) {
            perror("Thread creation failed");

            // Join all previously created threads before exiting
            for (int j = 0; j < createdThreads; j++) {
                pthread_join(threads[j], NULL);
            }

            free(data);
            return -1;
        }
        createdThreads++;
    }

    for (int i = 0; i < numberOfThreads; i++) {
        pthread_join(threads[i], NULL);
    }

    gettimeofday(&end, NULL); // store the end time

    long seconds = end.tv_sec - start.tv_sec;
    long microseconds = end.tv_usec - start.tv_usec;
    long total_microseconds = (seconds * 1000000) + microseconds;

    printf("Total Sum: %lld\n", totalSum);
    printf("Time taken: %ld microseconds\n", total_microseconds);

    pthread_mutex_destroy(&lock); // destroy the mutex
    free(data); // free the allocated memory
    data = NULL;
    return 0;
}

int readFile(char filename[], int data[]) {
    FILE *file = fopen(filename, "r");
    if (!file) {
        printf("File not found...\n");
        return -1;
    }
    int count = 0;
    while (count < MAX_VALUES && fscanf(file, "%d", &data[count]) != EOF) {
        count++;
    }
    fclose(file);
    return count;
}

void* arraySum(void *arg) {
    thread_data_t *data = (thread_data_t *) arg;

    long long int threadSum = 0;
    for (int i = data->startInd; i <= data->endInd; i++) {
        threadSum += data->data[i];
    }
    
    // Lock critical section
    pthread_mutex_lock(data->lock);
    *(data->totalSum) += threadSum;
    pthread_mutex_unlock(data->lock);

    return NULL;
}
