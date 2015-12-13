// os345.c - OS Kernel	09/12/2013
// ***********************************************************************
// **   DISCLAMER ** DISCLAMER ** DISCLAMER ** DISCLAMER ** DISCLAMER   **
// **                                                                   **
// ** The code given here is the basis for the BYU CS345 projects.      **
// ** It comes "as is" and "unwarranted."  As such, when you use part   **
// ** or all of the code, it becomes "yours" and you are responsible to **
// ** understand any algorithm or method presented.  Likewise, any      **
// ** errors or problems become your responsibility to fix.             **
// **                                                                   **
// ** NOTES:                                                            **
// ** -Comments beginning with "// ??" may require some implementation. **
// ** -Tab stops are set at every 3 spaces.                             **
// ** -The function API's in "OS345.h" should not be altered.           **
// **                                                                   **
// **   DISCLAMER ** DISCLAMER ** DISCLAMER ** DISCLAMER ** DISCLAMER   **
// ***********************************************************************

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <setjmp.h>
#include <time.h>
#include <assert.h>

#include "os345.h"
#include "os345signals.h"
#include "os345config.h"
#include "os345lc3.h"
#include "os345fat.h"

// **********************************************************************
//	local prototypes
//
void pollInterrupts(void);
static int scheduler(void);
static int dispatcher(void);

//static void keyboard_isr(void);
//static void timer_isr(void);

int sysKillTask(int taskId);
static int initOS(void);

// **********************************************************************
// **********************************************************************
// global semaphores

Semaphore* semaphoreList;			// linked list of active semaphores

Semaphore* keyboard;				// keyboard semaphore
Semaphore* charReady;				// character has been entered
Semaphore* inBufferReady;			// input buffer ready semaphore

Semaphore* tics10sec;				// 10 second semaphore
Semaphore* tics1sec;				// 1 second semaphore
Semaphore* tics10thsec;				// 1/10 second semaphore

// **********************************************************************
// **********************************************************************
// global system variables

TCB tcb[MAX_TASKS];					// task control block
Semaphore* taskSems[MAX_TASKS];		// task semaphore
jmp_buf k_context;					// context of kernel stack
jmp_buf reset_context;				// context of kernel stack
volatile void* temp;				// temp pointer used in dispatcher

int scheduler_mode;					// scheduler mode
int superMode;						// system mode
int curTask;						// current task #
long swapCount;						// number of re-schedule cycles
char inChar;						// last entered character
int charFlag;						// 0 => buffered input
int inBufIndx;						// input pointer into input buffer
char inBuffer[INBUF_SIZE+1];		// character input buffer
//Message messages[NUM_MESSAGES];		// process message buffers

int pollClock;						// current clock()
int lastPollClock;					// last pollClock
bool diskMounted;					// disk has been mounted

time_t oldTime1;					// old 1sec time
time_t oldTime10;					// old 10sec time
clock_t myClkTime;
clock_t myOldClkTime;
PQueue* rq;							// ready priority queue
deltaClock* dc;						// delta clock
Semaphore* dcMutex;					// controls access to the delta clock
clock_t dcLastDecTime;				// tracks the last time the delta clock was decremented for catching up if needed

#define MAX_TIME_SHARES 1024



// **********************************************************************
// **********************************************************************
// OS startup
//
// 1. Init OS
// 2. Define reset longjmp vector
// 3. Define global system semaphores
// 4. Create CLI task
// 5. Enter scheduling/idle loop
//
int main(int argc, char* argv[])
{
	// save context for restart (a system reset would return here...)
	int resetCode = setjmp(reset_context);
	superMode = TRUE;						// supervisor mode

	switch (resetCode)
	{
		case POWER_DOWN_QUIT:				// quit
			powerDown(0);
			printf("\nGoodbye!!");
			return 0;

		case POWER_DOWN_RESTART:			// restart
			inBufIndx = 0;
			inBuffer[0] = 0;
			powerDown(resetCode);
			printf("\nRestarting system...\n");

		case POWER_UP:						// startup
			break;

		default:
			printf("\nShutting down due to error %d", resetCode);
			powerDown(resetCode);
			return resetCode;
	}

	// output header message
	printf("%s", STARTUP_MSG);

	// initalize OS
	if ((resetCode = initOS())) return resetCode;

	// create global/system semaphores here
	//?? vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv

	charReady = createSemaphore("charReady", BINARY, 0);
	inBufferReady = createSemaphore("inBufferReady", BINARY, 0);
	keyboard = createSemaphore("keyboard", BINARY, 1);
	tics1sec = createSemaphore("tics1sec", BINARY, 0);
	tics10thsec = createSemaphore("tics10thsec", BINARY, 0);
	tics10sec = createSemaphore("tics10sec", COUNTING, 0);

	//?? ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

	// schedule CLI task
	createTask("myShell",			// task name
					P1_shellTask,	// task
					MED_PRIORITY,	// task priority
					argc,			// task arg count
					argv);			// task argument pointers

	// HERE WE GO................

	// Scheduling loop
	// 1. Check for asynchronous events (character inputs, timers, etc.)
	// 2. Choose a ready task to schedule
	// 3. Dispatch task
	// 4. Loop (forever!)

	while(1)									// scheduling loop
	{
		// check for character / timer interrupts
		pollInterrupts();

		// schedule highest priority ready task
		if ((curTask = scheduler()) < 0) continue;

		// dispatch curTask, quit OS if negative return
		if (dispatcher() < 0) break;
	}											// end of scheduling loop

	// exit os
	longjmp(reset_context, POWER_DOWN_QUIT);
	return 0;
} // end main


int getChildren(int id, int* children) {
	int count = 0;
	for (int i = 0; i < MAX_TASKS; i++) {
		if (tcb[i].name) {
			if (tcb[i].parent == id && i != id) {
				children[count++] = i;
			}
		}
	}
	return count;
}

void distributeTimeShares(int id, int shares) {
	int* children = malloc(25 * sizeof(int));
	int num_children = getChildren(id, children);

	int shares_per_child = shares / (num_children + 1);
	int excess = shares % (num_children + 1);
	tcb[id].time_shares = shares_per_child + excess;
	// printf("Task %d gets %d shares\n", id, shares_per_child + excess);

	for (int i = 0; i < num_children; i++) {
		distributeTimeShares(children[i], shares_per_child);
	}

	free(children);
}
 
int getNextFairTask() {
	int count = rq->queue[0].count;
	if (count == 0) { return -1; }

	for (int i = 0; i < count; i++) {
		// printf("i = %d", i);
		int entry_id = rq->queue[i + 1].entry.tid;
		// printf("looking at element %d\n", entry_id);
		if (tcb[entry_id].time_shares > 0) {
			return entry_id;
		}
	}
	distributeTimeShares(0, MAX_TIME_SHARES);
	return rq->queue[rq->queue[0].count].entry.tid;
}

int getRoundRobinPriorityTask() {
	return deQ(rq, -1);
}

void killChildren(int id) {
	int* children = malloc(64 * sizeof(int));
	int num_children = getChildren(id, children);

	for (int i = 0; i < num_children; i++) {
		killTask(children[i]);
	}
	free(children);
}

void reassignChildren(int id) {
	int* children = malloc(64 * sizeof(int));
	int num_children = getChildren(id, children);
	if (num_children == 0) {
		return;
	}
	
	int new_parent = children[0];
	tcb[new_parent].parent = tcb[id].parent;

	for (int i = 1; i < num_children; i++) {
		tcb[children[i]].parent = new_parent;
	}
}


// **********************************************************************
// **********************************************************************
// scheduler
//
static int scheduler()
{
	int nextTask;
	// ?? Design and implement a scheduler that will select the next highest
	// ?? priority ready task to pass to the system dispatcher.

	// ?? WARNING: You must NEVER call swapTask() from within this function
	// ?? or any function that it calls.  This is because swapping is
	// ?? handled entirely in the swapTask function, which, in turn, may
	// ?? call this function.  (ie. You would create an infinite loop.)

	// ?? Implement a round-robin, preemptive, prioritized scheduler.

	// ?? This code is simply a round-robin scheduler and is just to get
	// ?? you thinking about scheduling.  You must implement code to handle
	// ?? priorities, clean up dead tasks, and handle semaphores appropriately.

	if (scheduler_mode == 0) {
		if ((nextTask = getRoundRobinPriorityTask()) >= 0) {
			enQ(rq, nextTask, tcb[nextTask].priority);
		}
		else {
			return -1;
		}
	}
	else {
		if ((nextTask = getNextFairTask()) >= 0) {
			tcb[nextTask].time_shares--;
		}
		else {
			return -1;
		}	
	}

	

	if (tcb[nextTask].signal & mySIGSTOP) return -1;

	return nextTask;
} // end scheduler



// **********************************************************************
// **********************************************************************
// dispatch curTask
//
static int dispatcher()
{
	int result;

	// schedule task
	switch(tcb[curTask].state)
	{
		case S_NEW:
		{
			// new task
			printf("\nNew Task[%d] %s", curTask, tcb[curTask].name);
			tcb[curTask].state = S_RUNNING;	// set task to run state

			// save kernel context for task SWAP's
			if (setjmp(k_context))
			{
				superMode = TRUE;					// supervisor mode
				break;								// context switch to next task
			}

			// move to new task stack (leave room for return value/address)
			temp = (int*)tcb[curTask].stack + (STACK_SIZE-8);
			SET_STACK(temp);
			superMode = FALSE;						// user mode

			// begin execution of new task, pass argc, argv
			result = (*tcb[curTask].task)(tcb[curTask].argc, tcb[curTask].argv);

			// task has completed
			if (result) printf("\nTask[%d] returned %d", curTask, result);
			else printf("\nTask[%d] returned %d", curTask, result);
			tcb[curTask].state = S_EXIT;			// set task to exit state

			// return to kernal mode
			longjmp(k_context, 1);					// return to kernel
		}

		case S_READY:
		{
			tcb[curTask].state = S_RUNNING;			// set task to run
		}

		case S_RUNNING:
		{
			if (setjmp(k_context))
			{
				// SWAP executed in task
				superMode = TRUE;					// supervisor mode
				break;								// return from task
			}
			if (signals()) break;
			longjmp(tcb[curTask].context, 3); 		// restore task context
		}

		case S_BLOCKED:
		{
			break;
		}

		case S_EXIT:
		{
			if (curTask == 0) return -1;			// if CLI, then quit scheduler
			// release resources and kill task
			sysKillTask(curTask);					// kill current task
			break;
		}

		default:
		{
			printf("Unknown Task[%d] State", curTask);
			longjmp(reset_context, POWER_DOWN_ERROR);
		}
	}
	return 0;
} // end dispatcher



// **********************************************************************
// **********************************************************************
// Do a context switch to next task.

// 1. If scheduling task, return (setjmp returns non-zero value)
// 2. Else, save current task context (setjmp returns zero value)
// 3. Set current task state to READY
// 4. Enter kernel mode (longjmp to k_context)

void swapTask()
{
	assert("SWAP Error" && !superMode);		// assert user mode

	// increment swap cycle counter
	swapCount++;

	// either save current task context or schedule task (return)
	if (setjmp(tcb[curTask].context))
	{
		superMode = FALSE;					// user mode
		return;
	}

	// context switch - move task state to ready
	if (tcb[curTask].state == S_RUNNING) tcb[curTask].state = S_READY;

	// move to kernel mode (reschedule)
	longjmp(k_context, 2);
} // end swapTask



// **********************************************************************
// **********************************************************************
// system utility functions
// **********************************************************************
// **********************************************************************

// **********************************************************************
// **********************************************************************
// initialize operating system
static int initOS()
{
	int i;

	// make any system adjustments (for unblocking keyboard inputs)
	INIT_OS

	// reset system variables
	curTask = 0;						// current task #
	swapCount = 0;						// number of scheduler cycles
	scheduler_mode = 0;					// default scheduler
	inChar = 0;							// last entered character
	charFlag = 0;						// 0 => buffered input
	inBufIndx = 0;						// input pointer into input buffer
	semaphoreList = 0;					// linked list of active semaphores
	diskMounted = 0;					// disk has been mounted
	dcLastDecTime = clock();

	// malloc ready queue
	rq = (PQueue*)malloc((MAX_TASKS + 1) * sizeof(int));
	rq->queue[0].count = 0;
	if (rq == NULL) return 99;

	// malloc delta clock
	dc = malloc((MAX_TASKS + 1) * sizeof(dc_entry));
	dc->clock[0].count = 0;
	if (dc == NULL) return 100;

	// create delta clock mutex
	dcMutex = createSemaphore("deltaClockMutex", BINARY, 1);

	// capture current time
	lastPollClock = clock();			// last pollClock
	time(&oldTime1);
	time(&oldTime10);

	// init system tcb's
	for (i=0; i<MAX_TASKS; i++)
	{
		tcb[i].name = NULL;				// tcb
		taskSems[i] = NULL;				// task semaphore
	}

	// init tcb
	for (i=0; i<MAX_TASKS; i++)
	{
		tcb[i].name = NULL;
	}

	// initialize lc-3 memory
	initLC3Memory(LC3_MEM_FRAME, 0xF800>>6);

	// ?? initialize all execution queues

	return 0;
} // end initOS



// **********************************************************************
// **********************************************************************
// Causes the system to shut down. Use this for critical errors
void powerDown(int code)
{
	int i;
	printf("\nPowerDown Code %d", code);

	// release all system resources.
	printf("\nRecovering Task Resources...");

	// kill all tasks
	for (i = MAX_TASKS-1; i >= 0; i--)
		if(tcb[i].name) sysKillTask(i);

	// delete all semaphores
	while (semaphoreList)
		deleteSemaphore(&semaphoreList);

	// free ready queue
	free(rq);

	// ?? release any other system resources
	// ?? deltaclock (project 3)

	RESTORE_OS
	return;
} // end powerDown

// Prints to stdout the contents of the PQueue
void printQ(PQueue* q) {
	int count = q->queue[0].count;
	for (int i = count; i > 0; --i) {
		printf("i: %d\tPriority: %d\tTID: %d\n", i, q->queue[i].entry.priority, q->queue[i].entry.tid);
	}
	printf("i: 0\tCount: %d\n", count);
	printf("\n");
}

// Creates an entry for the given priority and tid in the spot just above the current highest priority entry less than the new priority
TID enQ(PQueue* q, TID tid, int8 priority) {
	int count = q->queue[0].count;

	// Check if queue is full
	if (count == MAX_TASKS) { return -1; }
	
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

// Removes the given tid from the queue if found, or removes the highest priority entry if tid is -1
TID deQ(PQueue* q, TID tid) {
	// Check if queue is empty
	if (q->queue[0].count == 0) { return -1; }

	TID ret_tid = -1;

	// If tid is -1, remove the highest priority item
	if (tid == -1) {
		ret_tid = q->queue[q->queue[0].count].entry.tid;
		q->queue[0].count--;
	}
	// If tid is not -1, find the entry with the requested tid and remove it, shifting all others to fill the gap
	else {
		bool found = FALSE;
		int count = q->queue[0].count;
		for (int i = 1; i <= count; i++) {
			if (found) {
				q->queue[i-1].entry.priority = q->queue[i].entry.priority;
				q->queue[i-1].entry.tid = q->queue[i].entry.tid;
				continue;
			}
			if (q->queue[i].entry.tid == tid) {
				ret_tid = q->queue[i].entry.tid;
				found = TRUE;
				q->queue[0].count--;
			}
			
		}
	}
	return ret_tid;
}










