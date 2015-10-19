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
extern TCB tcb[];								// task control block

extern deltaClock* dc;
extern Semaphore* dcMutex;
extern time_t dcLastDecTime;

// ***********************************************************************
// project 3 park functions and tasks
int P3_carTask(int, char**);
int P3_driverTask(int, char**);
int P3_visitorTask(int, char**);
Semaphore* canLoadCar;
Semaphore* isSeatOpen;
Semaphore* isSeatTaken;
Semaphore* needDriver;
Semaphore* isDriverAwake;
Semaphore* isDriverReady;
Semaphore* canTakeSeat;
Semaphore* canDriveCar;
Semaphore* enterPark;
#define PARK_WAIT SEM_WAIT(parkMutex)
#define PARK_SIGNAL SEM_SIGNAL(parkMutex)
Semaphore* passengerMailbox;
Semaphore* driverMailbox;
int carLoadingId;

// ***********************************************************************
// project 3 delta clock functions and tasks
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
	int i;
	char buf1[32];
	char buf2[32];
	char* newArgv[2];

	// start park
	sprintf(buf1, "jurassicPark");
	newArgv[0] = buf1;
	createTask( buf1,				// task name
		jurassicTask,				// task
		MED_PRIORITY,				// task priority
		1,							// task count
		newArgv);					// task argument

	// wait for park to get initialized...
	while (!parkMutex) SWAP;
	printf("\nStart Jurassic Park...");

	PARK_WAIT;											
	myPark.numOutsidePark = NUM_VISITORS;
	PARK_SIGNAL;

	// create all semaphores
	canLoadCar = createSemaphore("canLoadCar", BINARY, 1);
	isSeatOpen = createSemaphore("isSeatOpen", BINARY, 0);
	isSeatTaken = createSemaphore("isSeatTaken", BINARY, 0);
	needDriver = createSemaphore("needDriver", BINARY, 0);
	isDriverAwake = createSemaphore("isDriverAwake", BINARY, 0);
	isDriverReady = createSemaphore("isDriverReady", BINARY, 0);
	canTakeSeat = createSemaphore("canTakeSeat", BINARY, 1);
	canDriveCar = createSemaphore("canDriveCar", BINARY, 1);
	enterPark = createSemaphore("enterPark", COUNTING, MAX_IN_PARK);

	//?? create car, driver, and visitor tasks here
	// create cars
	for (i = 0; i < NUM_CARS; i++) {
		sprintf(buf1, "carTask[%d]", i);
		newArgv[0] = buf1;
		sprintf(buf2, "%d", i);
		newArgv[1] = buf2;
		createTask(buf1, P3_carTask, MED_PRIORITY, 2, newArgv);
	}

	// create drivers
	for (i = 0; i < NUM_DRIVERS; i++) {
		sprintf(buf1, "driverTask[%d]", i);
		newArgv[0] = buf1;
		sprintf(buf2, "%d", i);
		newArgv[1] = buf2;
		createTask(buf1, P3_driverTask, MED_PRIORITY, 2, newArgv);
	}

	// create visitors
	for (i = 0; i < NUM_VISITORS; i++) {
		sprintf(buf1, "visitorTask[%d]", i);
		newArgv[0] = buf1;
		sprintf(buf2, "%d", i);
		newArgv[1] = buf2;
		createTask(buf1, P3_visitorTask, MED_PRIORITY, 2, newArgv);
	}

	SEM_WAIT(parkMutex);
	myPark.numInCarLine = myPark.numInPark = 28;
	SEM_SIGNAL(parkMutex);

	return 0;
} // end project3

// ***********************************************************************
// ***********************************************************************
// car task
int P3_carTask(int argc, char* argv[]) {
	char* carName[32];										SWAP;
	strcpy(carName, argv[0]);								SWAP;
	int carId = atoi(argv[1]);								SWAP;
	printf("\nCAR: %s created with id %d", carName, carId);	SWAP;

	Semaphore* passengers[NUM_SEATS];						SWAP;
	Semaphore* driver;										SWAP;

	while (1) {												SWAP;
		int i;												SWAP;
		// wait for car loading to be free
		SEM_WAIT(canLoadCar);								SWAP;
		carLoadingId = carId;								SWAP;
		for (i = 0; i < NUM_SEATS; i++) {					SWAP;
			// wait to be told to fill a seat by park
			SEM_WAIT(fillSeat[carId]);						SWAP;
			// signal seat open to visitors
			SEM_SIGNAL(isSeatOpen);							SWAP;
			// wait for seat taken from visitors
			SEM_WAIT(isSeatTaken);							SWAP;
			// save passenger mutex
			passengers[i] = passengerMailbox;				SWAP;
			if (i == 2) {									SWAP;
				// signal need driver to drivers
				SEM_SIGNAL(needDriver);						SWAP;
				// wake up driver
				SEM_SIGNAL(isDriverAwake);					SWAP;
				// wait for driver
				SEM_WAIT(isDriverReady);					SWAP;
				// save driver mutex
				driver = driverMailbox;						SWAP;
			}												SWAP;
			// signal passenger seated to park
			SEM_SIGNAL(seatFilled[carId]);					SWAP;
		}
		carLoadingId = -1;									SWAP;
		// signal done with car loading
		SEM_SIGNAL(canLoadCar);								SWAP;
		// wait for ride to be over
		SEM_WAIT(rideOver[carId]);							SWAP;
		// signal driver done
		SEM_SIGNAL(driver);									SWAP;

		for (i = 0; i < NUM_SEATS; i++) {					SWAP;
			// signal passenger ride over
			SEM_SIGNAL(passengers[i]);						SWAP;
		}													SWAP;
	}


	return 0;
}

// ***********************************************************************
// driver task
int P3_driverTask(int argc, char* argv[]) {
	char* driverName[32];									SWAP;
	strcpy(driverName, argv[0]);							SWAP;
	int driverId = atoi(argv[1]);							SWAP;
	printf("\nDRIVER: %s created with id %d", driverName, driverId);	SWAP;

	char buf[32];											SWAP;
	sprintf(buf, "driverSem[%d]", driverId);				SWAP;
	Semaphore* notifyDriver = createSemaphore(buf, BINARY, 0);	SWAP;

	while (1) {												SWAP;
		SEM_WAIT(isDriverAwake);							SWAP;
		if (SEM_TRYLOCK(needDriver)) {						SWAP;
			SEM_WAIT(canDriveCar);							SWAP;
			driverMailbox = notifyDriver;					SWAP;
			// update park struct with driver status
			PARK_WAIT;										SWAP;
			myPark.drivers[driverId] = carLoadingId + 1;	SWAP;
			PARK_SIGNAL;									SWAP;
			SEM_SIGNAL(isDriverReady);						SWAP;
			SEM_SIGNAL(canDriveCar);						SWAP;
			SEM_WAIT(notifyDriver);							SWAP;
			PARK_WAIT;										SWAP;
			myPark.drivers[driverId] = 0;					SWAP;
			PARK_SIGNAL;									SWAP;
		}
		else {												SWAP;
			break;											SWAP;
		}													SWAP;
		
	}														SWAP;
	return 0;
}

// ***********************************************************************
// visitor task
int P3_visitorTask(int argc, char* argv[]) {
	char* visitorName[32];									SWAP;
	strcpy(visitorName, argv[0]);							SWAP;
	int visitorId = atoi(argv[1]);							SWAP;
	printf("\nVISITOR: %s created with id %d", visitorName, visitorId);	SWAP;

	char buf[32];											SWAP;
	sprintf(buf, "visitorSem[%d]", visitorId);				SWAP;
	Semaphore* notifyVisitor = createSemaphore(buf, BINARY, 0);	SWAP;

	// get inside the park
	SEM_WAIT(enterPark);								SWAP;
	// update park struct to enter park
	PARK_WAIT;											SWAP;
	myPark.numOutsidePark--;							SWAP;
	myPark.numInPark++;									SWAP;
	myPark.numInCarLine++;								SWAP;
	PARK_SIGNAL;										SWAP;
	// take a guided tour			
	// wait for permission to take seat
	SEM_WAIT(canTakeSeat);								SWAP;
	// wait for seat open
	SEM_WAIT(isSeatOpen);								SWAP;
	// put semaphore in mailbox
	passengerMailbox = notifyVisitor;					SWAP;
	// move visitor from line to car in park
	PARK_WAIT;											SWAP;
	myPark.numInCarLine--;								SWAP;
	myPark.numInCars++;									SWAP;
	PARK_SIGNAL;										SWAP;
	// take seat
	SEM_SIGNAL(isSeatTaken);							SWAP;
	// signal can take seat
	SEM_SIGNAL(canTakeSeat);							SWAP;
	// wait for ride to be over
	SEM_WAIT(notifyVisitor);							SWAP;
	// update park struct to exit park
	PARK_WAIT;											SWAP;
	myPark.numInCars--;									SWAP;
	myPark.numInPark--;									SWAP;
	myPark.numExitedPark++;								SWAP;
	PARK_SIGNAL;										SWAP;
	// leave park
	SEM_SIGNAL(enterPark);								SWAP;



	return 0;
}

// ***********************************************************************
// ***********************************************************************
// delta clock command
int P3_dc(int argc, char* argv[])
{
	printf("\nDelta Clock");
	printDC(dc);
	return 0;
} // end P3_dc

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
		SEM_WAIT(dcChange);
		flg = 0;
		for (i=0; i<10; i++)
		{
			if (event[i]->state == 1)			{
					printf("\nEvent[%d] signaled", i);
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
// display time every tics10thsec
int timeTask(int argc, char* argv[])
{
	char svtime[64];						// ascii current time
	clock_t currentTime;						// current time
	dcLastDecTime = clock();

	while (1)
	{
		SEM_WAIT(tics10thsec);
		// printf("\nTime = %s\n", myTime(svtime));
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
	SEM_WAIT(dcMutex);
	int count = c->clock[0].count;
	int i;
	for (i = count; i > 0; i--)
	{
		int t = c->clock[i].entry.time;
		char* s = c->clock[i].entry.sem->name;
		printf("\n%4d%4d  %-20s", i, t, s);
	}
	printf("\n%4d%4d", 0, count);
	SEM_SIGNAL(dcMutex);
	return;
}

// ********************************************************************************************
// Inserts time and semaphore into the delta clock
int insertDC(deltaClock* c, int t, Semaphore* sem) {
	SEM_WAIT(dcMutex);
	int temp = t;
	// printf("inserting semaphore `%s`\n", sem->name);
	int count = c->clock[0].count;
	// delta clock is empty
	if (count == 0) {
		dc_entry entry =  { .time = t, .sem = sem };
		c->clock[1].entry = entry;
		c->clock[0].count++;
		SEM_SIGNAL(dcMutex);
		return 1;
	}
	// delta clock is full
	else if (count == MAX_TASKS) {
		SEM_SIGNAL(dcMutex);
		return -1;
	}
	// otherwise
	else {
		for (int i = count + 1; i > 0; --i) {
			int delta = temp - c->clock[i - 1].entry.time;
			// printf("\ndelta = temp - c->clock[i - 1].entry.time: %d = %d - %d", delta, temp, c->clock[i - 1].entry.time);
			if (delta < 0 || i == 1) {// (temp < current value) => The new value goes in i
				dc_entry entry = { .time = temp, .sem = sem };
				c->clock[i].entry = entry;
				c->clock[0].count++;
				if (i > 1) {
					c->clock[i - 1].entry.time = abs(delta);
				}
				SEM_SIGNAL(dcMutex);
				return i;
			}
			else {// (temp >= curret value) => The i - 1 value goes in i
				c->clock[i].entry = c->clock[i - 1].entry;
				temp = delta;
			}
		}		
	}
}

// ********************************************************************************************
// decrements the top value of the delta clock
void decDC(deltaClock* c) {
	SEM_WAIT(dcMutex);
	int count = c->clock[0].count;

	if (count == 0) { return; }
	c->clock[count].entry.time--;
	while (c->clock[count].entry.time == 0 && count > 0) {
		SEM_SIGNAL(c->clock[count].entry.sem);
		c->clock[0].count--;
		SEM_SIGNAL(dcChange);
		count = c->clock[0].count;
	}
	// printf("Decrement Delta Clock:\n");
	// printDC(c);
	SEM_SIGNAL(dcMutex);
	return;
}






