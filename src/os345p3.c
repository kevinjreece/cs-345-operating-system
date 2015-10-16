// os345p3.c - Jurassic Park
// ***********************************************************************
// **   DISCLAMER ** DISCLAMER ** DISCLAMER ** DISCLAMER ** DISCLAMER   **
// **                                                                   **
// ** The code given here is the basis for the CS345 projects.          **
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
#include "os345park.h"

// ***********************************************************************
// project 3 variables

// Jurassic Park
extern JPARK myPark;
extern Semaphore* parkMutex;						// protect park access
extern Semaphore* fillSeat[NUM_CARS];			// (signal) seat ready to fill
extern Semaphore* seatFilled[NUM_CARS];		// (wait) passenger seated
extern Semaphore* rideOver[NUM_CARS];			// (signal) ride over
extern deltaClock* dc;
extern TCB tcb[];								// task control block
extern time_t dcLastDecTime;

// ***********************************************************************
// project 3 functions and tasks
void CL3_project3(int, char**);
void CL3_dc(int, char**);
int dcMonitorTask(int, char**);
int timeTask(int, char**);
void printDC(deltaClock*);
int insertDC(deltaClock*, int, Semaphore*);
void decDC(deltaClock*);
int timeTaskID;
Semaphore* dcChange;


// ***********************************************************************
// ***********************************************************************
// project3 command
int P3_project3(int argc, char* argv[])
{
	char buf[32];
	char* newArgv[2];

	// start park
	sprintf(buf, "jurassicPark");
	newArgv[0] = buf;
	createTask( buf,				// task name
		jurassicTask,				// task
		MED_PRIORITY,				// task priority
		1,								// task count
		newArgv);					// task argument

	// wait for park to get initialized...
	while (!parkMutex) SWAP;
	printf("\nStart Jurassic Park...");

	//?? create car, driver, and visitor tasks here

	return 0;
} // end project3



// ***********************************************************************
// ***********************************************************************
// delta clock command
int P3_dc(int argc, char* argv[])
{
	printf("\nDelta Clock");
	printDC(dc);
	return 0;
} // end CL3_dc


// ***********************************************************************
// test delta clock
int P3_tdc(int argc, char* argv[])
{
	createTask( "DC Test",			// task name
		dcMonitorTask,		// task
		10,					// task priority
		argc,					// task arguments
		argv);

	timeTaskID = createTask( "Time",		// task name
		timeTask,	// task
		10,			// task priority
		argc,			// task arguments
		argv);
	return 0;
} // end P3_tdc



// ***********************************************************************
// monitor the delta clock task
int dcMonitorTask(int argc, char* argv[])
{
	int i, flg;
	char buf[32];
	// create some test times for event[0-9]
	int ttime[10] = {
		90, 300, 50, 170, 340, 300, 50, 300, 40, 110	};
	Semaphore** event = malloc(10 * sizeof(Semaphore*));
	dcChange = createSemaphore("dcChange", BINARY, 0);

	for (i = 0; i < 10; i++)
	{
		sprintf(buf, "event[%d]", i);
		event[i] = createSemaphore(buf, BINARY, 0);
		insertDC(dc, ttime[i], event[i]);
		// printDC(dc);
	}
	printDC(dc);

	while (dc->clock[0].count > 0)
	{
		SEM_WAIT(dcChange)
		flg = 0;
		for (i=0; i<10; i++)
		{
			if (event[i]->state == 1)			{
					printf("Event[%d] signaled\n", i);
					event[i]->state = 0;
					flg = 1;
				}
		}
		if (flg) printDC(dc);
	}
	printf("\nNo more events in Delta Clock");

	// kill timeTask
	killTask(timeTaskID);
	return 0;
} // end dcMonitorTask

extern Semaphore* tics1sec;
extern Semaphore* tics10thsec;

// ********************************************************************************************
// display time every tics1sec
int timeTask(int argc, char* argv[])
{
	char svtime[64];						// ascii current time
	clock_t currentTime;						// current time
	dcLastDecTime = clock();

	while (1)
	{
		SEM_WAIT(tics10thsec);
		printf("\nTime = %s\n", myTime(svtime));
		currentTime = clock();
		// printf("Clocks_per_sec: %f\n", CLOCKS_PER_SEC);
		int diff = currentTime - dcLastDecTime + 100;
		// printf ("currentTime: %d\tdcLastDecTime: %d\tdelta: %d seconds: %f\n", currentTime, dcLastDecTime, diff, ((float)diff)/CLOCKS_PER_SEC);
		for (int i = 100000; i < diff; i += ONE_TENTH_SEC) {
			decDC(dc);
		}	
		dcLastDecTime = currentTime;
	}
	return 0;
} // end timeTask

// ********************************************************************************************
// print delta clock
void printDC(deltaClock* c) {
	int count = c->clock[0].count;
	int i;
	for (i = count; i > 0; i--)
	{
		int t = c->clock[i].entry.time;
		char* s = c->clock[i].entry.sem->name;
		printf("%4d%4d  %-20s\n", i, t, s);
	}
	printf("%4d%4d\n", 0, count);
	return;
}

// ********************************************************************************************
// Inserts time and semaphore into the delta clock
int insertDC(deltaClock* c, int t, Semaphore* sem) {
	int temp = t;
	// printf("inserting semaphore `%s`\n", sem->name);
	int count = c->clock[0].count;
	// delta clock is empty
	if (count == 0) {
		dc_entry entry =  { .time = t, .sem = sem };
		c->clock[1].entry = entry;
		c->clock[0].count++;
		return 1;
	}
	// delta clock is full
	else if (count == MAX_TASKS) {
		return -1;
	}
	// otherwise
	else {
		for (int i = count + 1; i > 0; --i) {
			int delta = temp - c->clock[i - 1].entry.time;
			// printf("delta = temp - c->clock[i - 1].entry.time: %d = %d - %d\n", delta, temp, c->clock[i - 1].entry.time);
			if (delta < 0 || i == 1) {// temp < current value => The new value goes in i
				dc_entry entry = { .time = temp, .sem = sem };
				c->clock[i].entry = entry;
				c->clock[0].count++;
				if (i > 1) {
					c->clock[i - 1].entry.time = abs(delta);
				}
				// printf("Count: %d\tPriority: %d\tTID: %d\ti: %d\n", q->queue[0].count, priority, tid, i);
				return i;
			}
			else {// temp >= curret value => The i - 1 value goes in i
				c->clock[i].entry = c->clock[i - 1].entry;
				temp = delta;
			}
		}		
	}
}

void decDC(deltaClock* c) {
	int count = c->clock[0].count;
	if (count == 0) { return; }
	c->clock[count].entry.time--;
	while (c->clock[count].entry.time == 0 && count > 0) {
		SEM_SIGNAL(c->clock[count].entry.sem);
		c->clock[0].count--;
		SEM_SIGNAL(dcChange);
		count = c->clock[0].count;
	}
	printf("Decrement Delta Clock:\n");
	printDC(c);
	return;
}






