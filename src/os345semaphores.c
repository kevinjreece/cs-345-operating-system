// os345semaphores.c - OS Semaphores
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


extern TCB tcb[];							// task control block
extern int curTask;							// current task #

extern int superMode;						// system mode
extern Semaphore* semaphoreList;			// linked list of active semaphores

extern PQueue* rq;							// priority queue
void printQ(PQueue*);

// **********************************************************************
// **********************************************************************
// signal semaphore
//
//	if task blocked by semaphore, then clear semaphore and wakeup task
//	else signal semaphore
//
void semSignal(Semaphore* s)
{
	// assert there is a semaphore and it is a legal type
	assert("semSignal Error" && s && ((s->type == 0) || (s->type == 1)));

	// check semaphore type
	if (s->type == 0)
	{
		// binary semaphore

		TID id = deQ(s->q, -1);// Get highest priority blocked item
		
		if (id >= 0) {// Was there another item waiting in the semaphore array?
			s->state = 0;				// clear semaphore
			tcb[id].event = 0;			// clear event pointer
			tcb[id].state = S_READY;	// unblock task

			// ?? move task from blocked to ready queue
			enQ(rq, id, tcb[id].priority);

			if (!superMode) swapTask();
			return;
		}
			
		// nothing waiting on semaphore, go ahead and just signal
		s->state = 1;						// nothing waiting, signal
		if (!superMode) swapTask();
		return;
	}
	else
	{
		// counting semaphore
		// ?? implement counting semaphore
		s->state++;
		if (s->state <= 0) {// Are there additional tasks waiting?

			TID id = deQ(s->q, -1);
			enQ(rq, id, tcb[id].priority);
			tcb[id].event = 0;			// clear event pointer
			tcb[id].state = S_READY;	// unblock task

			if (!superMode) swapTask();
			return;
		}
	}
} // end semSignal



// **********************************************************************
// **********************************************************************
// wait on semaphore
//
//	if semaphore is signaled, return immediately
//	else block task
//
int semWait(Semaphore* s)
{
	assert("semWait Error" && s);												// assert semaphore
	assert("semWait Error" && ((s->type == 0) || (s->type == 1)));	// assert legal type
	assert("semWait Error" && !superMode);								// assert user mode

	// check semaphore type
	if (s->type == 0)
	{
		// binary semaphore
		// if state is zero, then block task

		if (s->state == 0)
		{
			tcb[curTask].event = s;		// block task
			tcb[curTask].state = S_BLOCKED;

			// ?? move task from ready queue to blocked queue
			enQ(s->q, deQ(rq, curTask), tcb[curTask].priority);

			swapTask();						// reschedule the tasks
			return 1;
		}
		// state is non-zero (semaphore already signaled)
		s->state = 0;						// reset state, and don't block
		return 0;
	}
	else
	{
		// counting semaphore
		// ?? implement counting semaphore

		s->state--;// Signal that someone needs a resource
		if (s->state >= 0) return 0;// If there was a resource available, return and execute unblocked

		tcb[curTask].event = s;
		tcb[curTask].state = S_BLOCKED;

		enQ(s->q, deQ(rq, curTask), tcb[curTask].priority);
		swapTask();
		return 1;

	}
} // end semWait



// **********************************************************************
// **********************************************************************
// try to wait on semaphore
//
//	if semaphore is signaled, return 1
//	else return 0
//
int semTryLock(Semaphore* s)
{
	assert("semTryLock Error" && s);												// assert semaphore
	assert("semTryLock Error" && ((s->type == 0) || (s->type == 1)));	// assert legal type
	assert("semTryLock Error" && !superMode);									// assert user mode

	// check semaphore type
	if (s->type == 0)
	{
		// binary semaphore
		// if state is zero, then block task

		// ?? temporary label
		if (s->state == 0)
		{
			return 0;
		}
		// state is non-zero (semaphore already signaled)
		s->state = 0;						// reset state, and don't block
		return 1;
	}
	else
	{
		// counting semaphore
		// ?? implement counting semaphore
		if (s->state <= 0) {
			return 0;
		}

		s->state--;
		return 1;
	}
} // end semTryLock


// **********************************************************************
// **********************************************************************
// Create a new semaphore.
// Use heap memory (malloc) and link into semaphore list (Semaphores)
// 	name = semaphore name
//		type = binary (0), counting (1)
//		state = initial semaphore state
// Note: memory must be released when the OS exits.
//
Semaphore* createSemaphore(char* name, int type, int state)
{
	Semaphore* sem = semaphoreList;
	Semaphore** semLink = &semaphoreList;

	// assert semaphore is binary or counting
	assert("createSemaphore Error" && ((type == 0) || (type == 1)));	// assert type is validate

	// look for duplicate name
	while (sem)
	{
		if (!strcmp(sem->name, name))
		{
			printf("\nSemaphore %s already defined", sem->name);

			// ?? What should be done about duplicate semaphores ??
			// semaphore found - change to new state
			sem->type = type;					// 0=binary, 1=counting
			sem->state = state;				// initial semaphore state
			sem->taskNum = curTask;			// set parent task #
			return sem;
		}
		// move to next semaphore
		semLink = (Semaphore**)&sem->semLink;
		sem = (Semaphore*)sem->semLink;
	}

	// allocate memory for new semaphore
	sem = (Semaphore*)malloc(sizeof(Semaphore));

	// set semaphore values
	sem->name = (char*)malloc(strlen(name)+1);
	strcpy(sem->name, name);				// semaphore name
	sem->type = type;							// 0=binary, 1=counting
	sem->state = state;						// initial semaphore state
	sem->taskNum = curTask;					// set parent task #

	sem->q = malloc((MAX_TASKS + 1) * sizeof(int));
	sem->q->queue[0].count = 0;

	// prepend to semaphore list
	sem->semLink = (struct semaphore*)semaphoreList;
	semaphoreList = sem;						// link into semaphore list
	return sem;									// return semaphore pointer
} // end createSemaphore



// **********************************************************************
// **********************************************************************
// Delete semaphore and free its resources
//
bool deleteSemaphore(Semaphore** semaphore)
{
	Semaphore* sem = semaphoreList;
	Semaphore** semLink = &semaphoreList;

	// assert there is a semaphore
	assert("deleteSemaphore Error" && *semaphore);

	// look for semaphore
	while(sem)
	{
		if (sem == *semaphore)
		{
			// semaphore found, delete from list, release memory
			*semLink = (Semaphore*)sem->semLink;

			// free the name array before freeing semaphore
			printf("\ndeleteSemaphore(%s)", sem->name);

			// ?? free all semaphore memory
			// ?? What should you do if there are tasks in this
			//    semaphores blocked queue????
			int i;
			for (i = 1; i <= sem->q->queue[0].count; i++) { killTask(sem->q->queue[i].entry.tid); }

			
			free(sem->name);
			free(sem->q);
			free(sem);

			return TRUE;
		}
		// move to next semaphore
		semLink = (Semaphore**)&sem->semLink;
		sem = (Semaphore*)sem->semLink;
	}

	// could not delete
	return FALSE;
} // end deleteSemaphore
