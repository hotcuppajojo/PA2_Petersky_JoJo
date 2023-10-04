threaded_sum: threaded_sum.c
	gcc -Wall -o threaded_sum threaded_sum.c -pthread
clean:
	rm *.o threaded_sum
