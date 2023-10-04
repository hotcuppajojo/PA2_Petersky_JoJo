
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h> // for gettimeofday

typedef struct _thread_data_t {
    const int *data;
    int startInd;
    int endInd;
    pthread_mutex_t *lock;
    long long int *totalSum;
    int *errorFlag;
} thread_data_t;

int readFile(char filename[], int data[]);
void* arraySum(void *arg);

#define MAX_VALUES 100000000 // max values you can read from a file
#define LLONG_MAX 9223372036854775807LL
#define LLONG_MIN (-LLONG_MAX - 1LL)

int main(int argc, char *argv[]) {
    int retVal = 0; // Variable to hold the return value
    int errorFlag = 0;
    pthread_t *threads = NULL;
    thread_data_t *threadData = NULL;
    // Ensure correct number of command-line arguments are provided
    if (argc != 3) {
        printf("Usage: %s <filename> <number of threads>\n", argv[0]);
        return -1;
    }
    // Allocate memory for data read from the file
    int *data = (int *) malloc(MAX_VALUES * sizeof(int));
    if (!data) {
        perror("Failed to allocate memory for data");
        retVal = -1;
        goto cleanup;
    }
    // Read file and populate the data array
    int numberOfValues = readFile(argv[1], data);
    if (numberOfValues == -1 || numberOfValues == MAX_VALUES) {
        retVal = -1;
        goto cleanup;
    }
    // Parse the number of threads requested from the command line
    int numberOfThreads = atoi(argv[2]);
    if (numberOfThreads > numberOfValues || numberOfThreads <= 0) {
        printf("Too many threads requested\n");
        retVal = -1;
        goto cleanup;
    }

    long long int totalSum = 0;
    struct timeval start, end;  // Structures for performance measurement
    pthread_mutex_t lock; // Mutex for synchronizing access to the shared totalSum variable
    // Memory allocation for thread identifiers and data
    threads = (pthread_t *) malloc(numberOfThreads * sizeof(pthread_t));
    threadData = (thread_data_t *) malloc(numberOfThreads * sizeof(thread_data_t));
    if (!threads || !threadData) {
        perror("Failed to allocate memory for threads or thread data");
        retVal = -1;
        goto cleanup;
    }

    gettimeofday(&start, NULL); // Capture start time for performance measurement

    // Mutex initialization for synchronization
    if (pthread_mutex_init(&lock, NULL) != 0) {
        perror("Mutex initialization failed");
        retVal = -1;
        goto cleanup;
    }

    int valuesPerThread = numberOfValues / numberOfThreads;

    // Thread creation loop
    for (int i = 0; i < numberOfThreads; i++) {
        // Initialize thread data
        threadData[i].data = data;
        threadData[i].startInd = i * valuesPerThread;
        threadData[i].endInd = (i == numberOfThreads - 1) ? numberOfValues - 1 : (i + 1) * valuesPerThread - 1;
        threadData[i].lock = &lock;
        threadData[i].totalSum = &totalSum;
        threadData[i].errorFlag = &errorFlag;

        // Create the thread
        if (pthread_create(&threads[i], NULL, arraySum, &threadData[i]) != 0) {
            perror("Thread creation failed");
            // If thread creation fails, join any threads created so far to cleanup
            for (int j = 0; j < i; j++) {
                pthread_join(threads[j], NULL);
            }
            retVal = -1;
            goto cleanup;
        }
    }

    // Wait for all threads to finish
    for (int i = 0; i < numberOfThreads; i++) {
        pthread_join(threads[i], NULL);
    }
    
    if (errorFlag == -1) {
        printf("Error detected in one of the threads.\n");
        retVal = -1;
    }

    gettimeofday(&end, NULL); // Capture end time for performance measurement

    // Calculate elapsed time
    long total_microseconds = ((end.tv_sec - start.tv_sec) * 1000000) + (end.tv_usec - start.tv_usec);

    // Output results
    printf("Total Sum: %lld\n", totalSum);
    printf("Time taken: %ld microseconds\n", total_microseconds);

    pthread_mutex_destroy(&lock); // Release mutex

    cleanup: // Memory cleanup
    free(threads);
    free(threadData);
    free(data);

    return retVal;
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
        if (threadSum > 0 && data->data[i] > 0 && threadSum > LLONG_MAX - data->data[i]) {
            perror("Overflow detected in thread summation");
            *data->errorFlag = -1; // Set the error flag here
            pthread_exit(NULL); // Terminate the thread
        } else if (threadSum < 0 && data->data[i] < 0 && threadSum < LLONG_MIN - data->data[i]) {
            perror("Underflow detected in thread summation");
            *data->errorFlag = -1; // Set the error flag here
            pthread_exit(NULL); // Terminate the thread
        }
        threadSum += data->data[i];
    }
    
    // Lock critical section
    pthread_mutex_lock(data->lock);
    // Check for overflow before updating totalSum
    if ((*data->totalSum) > 0 && threadSum > 0 && (*data->totalSum) > LLONG_MAX - threadSum) {
        perror("Overflow detected in total summation");
        *data->errorFlag = -1; // Set the error flag here
        pthread_mutex_unlock(data->lock);
        pthread_exit(NULL); // Terminate the thread
    } else if ((*data->totalSum) < 0 && threadSum < 0 && (*data->totalSum) < LLONG_MIN - threadSum) {
        perror("Underflow detected in total summation");
        *data->errorFlag = -1; // Set the error flag here
        pthread_mutex_unlock(data->lock);
        pthread_exit(NULL); // Terminate the thread
    }
    (*data->totalSum) += threadSum;
    pthread_mutex_unlock(data->lock);

    return NULL;
}
