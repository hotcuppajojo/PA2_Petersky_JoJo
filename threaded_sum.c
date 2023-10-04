/* Homework 2 - CS 446/646
    File: threaded_sum.c
    Author: JoJo Petersky
*/
#include <pthread.h> // For multithreaded execution
#include <stdio.h>   // Basic I/O operations
#include <stdlib.h>  // Dynamic memory management
#include <sys/time.h> // Performance measurement
// Structure: Organize thread tasks & shared data
typedef struct _thread_data_t {
    const int *data; // Unified data source for all threads
    int startInd;    // Individual thread's starting point
    int endInd;      // Individual thread's end point
    pthread_mutex_t *lock;  // Protect totalSum during concurrent access
    long long int *totalSum; // Shared accumulator across threads
    int *errorFlag; // Global error tracking
} thread_data_t;
// File operations & summation logic
int readFile(char filename[], int data[]); // Modularize file reading
void* arraySum(void *arg); // Thread's workhorse function
// Set boundaries to ensure memory and arithmetic operations are safe
#define MAX_VALUES 100000000 // Max cap prevents over-committing memory
#define LLONG_MAX 9223372036854775807LL // Overflow check: Int64 upper bound
#define LLONG_MIN (-LLONG_MAX - 1LL) // Underflow check: Int64 lower bound
// Main function to manage threads, read data, and compute sum.
int main(int argc, char *argv[]) {
    int retVal = 0; // Variable to hold the return value
    int errorFlag = 0; // No errors encountered yet
    // Memory pointers for thread IDs and thread-specific data
    pthread_t *threads = NULL;
    thread_data_t *threadData = NULL;
    // Two arguments are essential: input file and thread count
    if (argc != 3) {
        printf("Usage: %s <filename> <number of threads>\n", argv[0]);
        return -1;
    }
    // Dynamic memory allocation ensures adaptability to varying data sizes
    int *data = (int *) malloc(MAX_VALUES * sizeof(int));
    if (!data) {
        perror("Failed to allocate memory for data");
        retVal = -1;
        goto cleanup;
    }
    // Data loaded from file, checking against predefined limits for safety
    int numberOfValues = readFile(argv[1], data);
    if (numberOfValues == -1 || numberOfValues == MAX_VALUES) {
        retVal = -1;
        goto cleanup;
    }
    // Avoid spawning more threads than data points
    int numberOfThreads = atoi(argv[2]);
    if (numberOfThreads > numberOfValues || numberOfThreads <= 0) {
        printf("Too many threads requested\n");
        retVal = -1;
        goto cleanup;
    }
    // Initialize accumulator
    long long int totalSum = 0;
    struct timeval start, end;  // Performance timers
    pthread_mutex_t lock; // Mutex for thread-safe summing
    // Manage threads based on actual count needed
    threads = (pthread_t *) malloc(numberOfThreads * sizeof(pthread_t));
    threadData = (thread_data_t *) malloc(numberOfThreads * sizeof(thread_data_t));
    if (!threads || !threadData) {
        perror("Failed to allocate memory for threads or thread data");
        retVal = -1;
        goto cleanup;
    }
    // Start stopwatch
    gettimeofday(&start, NULL);
    // Mutex for controlled access, preventing race conditions
    if (pthread_mutex_init(&lock, NULL) != 0) {
        perror("Mutex initialization failed");
        retVal = -1;
        goto cleanup;
    }
    // Equal division of work
    int valuesPerThread = numberOfValues / numberOfThreads;
    // Distribute data among threads; each one handles a segment
    for (int i = 0; i < numberOfThreads; i++) {
        // Share the common data array across threads for efficiency
        threadData[i].data = data;
        // Partition the data array so threads work on non-overlapping segments
        threadData[i].startInd = i * valuesPerThread;
        // Last thread takes any leftover data points
        threadData[i].endInd = (i == numberOfThreads - 1) ? numberOfValues - 1 : (i + 1) * valuesPerThread - 1;
        // Share the same lock to manage concurrent access to totalSum
        threadData[i].lock = &lock;
        // All threads contribute to a single, shared total sum
        threadData[i].totalSum = &totalSum;
        // Any thread can flag an arithmetic error to halt all operations
        threadData[i].errorFlag = &errorFlag;
        // Spawn the thread with its custom data segment to process
        if (pthread_create(&threads[i], NULL, arraySum, &threadData[i]) != 0) {
            perror("Thread creation failed");
            // If a thread fails to spawn, terminate all threads spawned before it to ensure no orphaned operations
            for (int j = 0; j < i; j++) {
                pthread_join(threads[j], NULL);
            }
            retVal = -1;
            goto cleanup; // Central cleanup avoids memory leaks
        }
    }
    // Sync point: Main waits for threads to finish
    for (int i = 0; i < numberOfThreads; i++) {
        pthread_join(threads[i], NULL);
    }
    // Check for arithmetic errors in any thread
    if (errorFlag == -1) {
        printf("Error detected in one of the threads.\n");
        retVal = -1;
    }
    // Stop stopwatch
    gettimeofday(&end, NULL);
    // Calculate elapsed time
    long total_microseconds = ((end.tv_sec - start.tv_sec) * 1000000) + (end.tv_usec - start.tv_usec);
    // Output results
    printf("Total Sum: %lld\n", totalSum);
    printf("Time taken: %ld microseconds\n", total_microseconds);
    pthread_mutex_destroy(&lock); // Mutex no longer needed
    // Centralized cleanup for smooth memory management
    cleanup:
    free(threads); // De-allocate thread data
    free(threadData); // De-allocate thread-specific data structures
    free(data); // De-allocate the data array

    return retVal;
}
// Load data from file into memory
int readFile(char filename[], int data[]) {
    FILE *file = fopen(filename, "r");
    // Alert user when the file doesn't open successfully
    if (!file) {
        printf("File not found...\n");
        return -1;
    }
    int count = 0;
    // Reading up to MAX_VALUES ensures we don't exceed our memory allocation
    while (count < MAX_VALUES && fscanf(file, "%d", &data[count]) != EOF) {
        count++; // Track the number of values read for distribution among threads
    }
    fclose(file); // Free OS resources timely
    return count;
}
// Compute a partial sum of the data array
void* arraySum(void *arg) {
    thread_data_t *data = (thread_data_t *) arg;
    // Each thread only processes its assigned data segment
    long long int threadSum = 0;
    // Protect against integer overflows for safety
    for (int i = data->startInd; i <= data->endInd; i++) {
        if (threadSum > 0 && data->data[i] > 0 && threadSum > LLONG_MAX - data->data[i]) {
            perror("Overflow detected in thread summation");
            *data->errorFlag = -1; // Alert main thread about the error
            pthread_exit(NULL);
        } else if (threadSum < 0 && data->data[i] < 0 && threadSum < LLONG_MIN - data->data[i]) {
            perror("Underflow detected in thread summation");
            *data->errorFlag = -1; // Alert main thread about the error
            pthread_exit(NULL);
        }
        threadSum += data->data[i];
    }
    // Mutex ensures exclusive access to totalSum, avoiding race conditions
    pthread_mutex_lock(data->lock);
    // Safeguard against potential overflows when updating shared totalSum
    if ((*data->totalSum) > 0 && threadSum > 0 && (*data->totalSum) > LLONG_MAX - threadSum) {
        perror("Overflow detected in total summation");
        *data->errorFlag = -1;
        pthread_mutex_unlock(data->lock);
        pthread_exit(NULL);
    } else if ((*data->totalSum) < 0 && threadSum < 0 && (*data->totalSum) < LLONG_MIN - threadSum) {
        perror("Underflow detected in total summation");
        *data->errorFlag = -1;
        pthread_mutex_unlock(data->lock);
        pthread_exit(NULL);
    }
    (*data->totalSum) += threadSum;
    pthread_mutex_unlock(data->lock); // Release the mutex for other threads
    return NULL;
}
