#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define MAX_SIZE 100
#define true 1
#define false 0

typedef signed char int8;
typedef char bool;


typedef struct {
	union {
		int count;
		struct {
			int8 tid;
			int8 priority;
		} entry;
	} queue[MAX_SIZE+1];
} PQueue;

void printQ(PQueue* q) {
	int count = q->queue[0].count;
	for (int i = count; i > 0; --i) {
		printf("i: %d\tPriority: %d\tTID: %d\n", i, q->queue[i].entry.priority, q->queue[i].entry.tid);
	}
	printf("i: 0\tCount: %d\n", count);
	printf("\n");
}

// Creates an entry for the given priority and tid in the spot just above the current highest priority entry less than the new priority
int8 enQ(PQueue* q, int8 priority, int8 tid) {
	int count = q->queue[0].count;

	// Check if queue is full
	if (count == MAX_SIZE) { return -1; }
	
	// If the queue is empty, put the entry in the first slot
	if (count == 0) {
		q->queue[1].entry.priority = priority;
		q->queue[1].entry.tid = tid;
		q->queue[0].count++;
		// printf("Count: %d\tPriority: %d\tTID: %d\n", q->queue[0].count, priority, tid);
		return tid;
	}

	// If the queue is not empty, start at the highest priority item
	for (int i = count; i >= 0; --i) {
		if (priority > q->queue[i].entry.priority || i == 0) {
			q->queue[i+1].entry.priority = priority;
			q->queue[i+1].entry.tid = tid;
			q->queue[0].count++;
			// printf("Count: %d\tPriority: %d\tTID: %d\ti: %d\n", q->queue[0].count, priority, tid, i);
			return tid;
		}
		else {
			q->queue[i+1].entry.priority = q->queue[i].entry.priority;
			q->queue[i+1].entry.tid = q->queue[i].entry.tid;
		}
	}

	return -1;
}

int8 deQ(PQueue* q, int8 tid) {
	// Check if queue is empty
	if (q->queue[0].count == 0) { return -1; }

	int8 ret_tid = -1;

	// If tid is -1, remove the highest priority item
	if (tid == -1) {
		ret_tid = q->queue[q->queue[0].count].entry.tid;
		q->queue[0].count--;
	}
	// If tid is not -1, find the entry with the requested tid and remove it, shifting all others to fill the gap
	else {
		bool found = false;
		int count = q->queue[0].count;
		for (int i = 1; i <= count; i++) {
			if (found) {
				q->queue[i-1].entry.priority = q->queue[i].entry.priority;
				q->queue[i-1].entry.tid = q->queue[i].entry.tid;
				continue;
			}
			if (q->queue[i].entry.tid == tid) {
				ret_tid = q->queue[i].entry.tid;
				found = true;
				q->queue[0].count--;
			}
			
		}
	}
	return ret_tid;
}

int main(int argc, char* argv[]) {
	time_t t;
	srand((unsigned) time(&t));

	PQueue* q = malloc(sizeof(PQueue));
	q->queue[0].count = 0;

	for (int i = 1; i < 5; i++) {
		int8 r = rand() % 100;
		enQ(q, r, i);
		printQ(q);
		// printf("%d: %d\n", r, i);
	}

	printQ(q);
	printf("deQ 3 returns %d\n", deQ(q, 3));
	printQ(q);
	printf("deQ 3 returns %d\n", deQ(q, 3));
	printQ(q);
	printf("deQ -1 returns %d\n", deQ(q, -1));
	printQ(q);

	// for (int i = 1; i <= q->queue[0].count; i++) {
	// 	printf("%d: %d\n", q->queue[i].entry.priority, q->queue[i].entry.tid);
	// }

	free(q);

}