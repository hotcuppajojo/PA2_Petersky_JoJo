
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
} thread_data_t;

int readFile(char filename[], int data[]);
void* arraySum(void *arg);

#define MAX_VALUES 100000000 // max values you can read from a file

int main(int argc, char *argv[]) {
    int retVal = 0; // Variable to hold the return value
    pthread_t *threads = NULL;
    thread_data_t *threadData = NULL;
    // Check if the correct number of command-line arguments are provided
    if (argc != 3) {
        printf("Usage: %s <filename> <number of threads>\n", argv[0]);
        return -1;
    }
    // Allocate memory for the data read from the file.
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
    if (numberOfThreads > numberOfValues) {
        printf("Too many threads requested\n");
        retVal = -1;
        goto cleanup;
    }

    long long int totalSum = 0;
    struct timeval start, end; // Structures to hold the start and end times for performance measurement
    pthread_mutex_t lock; // Mutex for synchronizing access to the shared totalSum variable
    // Allocate memory for thread identifiers
    threads = (pthread_t *) malloc(numberOfThreads * sizeof(pthread_t));
    if (!threads) {
        perror("Failed to allocate memory for threads");
        retVal = -1;
        goto cleanup;
    }
    // Allocate memory for thread data which will be passed to each thread
    threadData = (thread_data_t *) malloc(numberOfThreads * sizeof(thread_data_t));
    if (!threadData) {
        perror("Failed to allocate memory for thread data");
        retVal = -1;
        goto cleanup;
    }
    // Capture the start time for performance measurement
    gettimeofday(&start, NULL);
    // Initialize the mutex for synchronization
    if (pthread_mutex_init(&lock, NULL) != 0) {
        perror("Mutex initialization failed");
        free(data);
        return -1;
    }
    // Calculate how many values each thread should process
    int valuesPerThread = numberOfValues / numberOfThreads;
    int createdThreads = 0; // Counter to track how many threads have been successfully created
    // Create the threads and assign them work
    for (int i = 0; i < numberOfThreads; i++) {
        threadData[i].data = data;
        threadData[i].startInd = i * valuesPerThread;
        // If it's the last thread, handle all remaining values
        // Otherwise, handle valuesPerThread values
        threadData[i].endInd = (i == numberOfThreads - 1) ? numberOfValues - 1 : (i + 1) * valuesPerThread - 1;
        threadData[i].lock = &lock; // Provide each thread with the mutex for synchronization
        threadData[i].totalSum = &totalSum; // Point to the shared totalSum variable
        // Create the thread
        if (pthread_create(&threads[i], NULL, arraySum, &threadData[i]) != 0) {
            perror("Thread creation failed");
            // If thread creation fails, join any previously created threads to clean up
            for (int j = 0; j < createdThreads; j++) {
                pthread_join(threads[j], NULL);
            }
            retVal = -1;
            goto cleanup;
        }
        createdThreads++; // Increment the successful thread creation counter
    }
    // Wait for all threads to complete their tasks
    for (int i = 0; i < numberOfThreads; i++) {
        pthread_join(threads[i], NULL);
    }
    // Capture the end time for performance measurement
    gettimeofday(&end, NULL); // store the end time
    // Calculate the elapsed time
    long seconds = end.tv_sec - start.tv_sec;
    long microseconds = end.tv_usec - start.tv_usec;
    long total_microseconds = (seconds * 1000000) + microseconds;
    // Output the results
    printf("Total Sum: %lld\n", totalSum);
    printf("Time taken: %ld microseconds\n", total_microseconds);
    // Clean up resources
    pthread_mutex_destroy(&lock); // Release the mutex
    cleanup: // Free allocated memory
    if (threads) {
        free(threads);
    }
    if (threadData) {
        free(threadData);
    }
    if (data) {
        free(data);
    }

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
        threadSum += data->data[i];
    }
    
    // Lock critical section
    pthread_mutex_lock(data->lock);
    *(data->totalSum) += threadSum;
    pthread_mutex_unlock(data->lock);

    return NULL;
}
