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
extern int curTask;							// current task #

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
Semaphore* needTicket;
Semaphore* wakeupDriver;
Semaphore* isDriverReady;
Semaphore* canTakeSeat;
Semaphore* canDriveCar;
Semaphore* canSellTicket;
Semaphore* enterPark;
Semaphore* rideTicket;
Semaphore* isTicketAvailable;
Semaphore* ticketTaken;
Semaphore* canBuyTicket;
Semaphore* inMuseum;
Semaphore* inGiftShop;
#define PARK_WAIT SEM_WAIT(parkMutex)
#define PARK_SIGNAL SEM_SIGNAL(parkMutex)
#define MAX_WAIT 30
Semaphore* passengerMailbox;
Semaphore* driverMailbox;
int carLoadingId;

// ***********************************************************************
// project 3 delta clock functions and tasks
int dcMonitorTask(int, char**);
int timeTask(int, char**);
void randomWait(Semaphore*, int);
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
	srand(time(NULL));										SWAP;
	int i;													SWAP;
	char buf1[32];											SWAP;
	char buf2[32];											SWAP;
	char* newArgv[2];										SWAP;

	int numVisitors = argc > 1 ? atoi(argv[1]) : NUM_VISITORS;SWAP;

	// start park
	sprintf(buf1, "jurassicPark");							SWAP;
	newArgv[0] = buf1;										SWAP;
	createTask( buf1,				// task name
		jurassicTask,				// task
		MED_PRIORITY,				// task priority
		1,							// task count
		newArgv);					SWAP;// task argument

	timeTaskID = createTask( "timeTask",		// task name
		timeTask,	// task
		MED_PRIORITY,			// task priority
		argc,			// task arguments
		argv);												SWAP;

	// wait for park to get initialized...
	while (!parkMutex) SWAP;
	printf("\nStart Jurassic Park...");						SWAP;

	PARK_WAIT;												SWAP;
	myPark.numOutsidePark = numVisitors;					SWAP;
	PARK_SIGNAL;											SWAP;

	// create all semaphores
	canLoadCar = createSemaphore("canLoadCar", BINARY, 1);	SWAP;
	isSeatOpen = createSemaphore("isSeatOpen", BINARY, 0);	SWAP;
	isSeatTaken = createSemaphore("isSeatTaken", BINARY, 0);SWAP;
	needDriver = createSemaphore("needDriver", BINARY, 0);	SWAP;
	needTicket = createSemaphore("needTicket", BINARY, 0);	SWAP;
	wakeupDriver = createSemaphore("wakeupDriver", BINARY, 0);SWAP;
	isDriverReady = createSemaphore("isDriverReady", BINARY, 0);SWAP;
	canTakeSeat = createSemaphore("canTakeSeat", BINARY, 1);SWAP;
	canDriveCar = createSemaphore("canDriveCar", BINARY, 1);SWAP;
	canSellTicket = createSemaphore("canSellTicket", BINARY, 1);SWAP;
	enterPark = createSemaphore("enterPark", COUNTING, MAX_IN_PARK);SWAP;
	rideTicket = createSemaphore("rideTicket", COUNTING, MAX_TICKETS);SWAP;
	isTicketAvailable = createSemaphore("isTicketAvailable", BINARY, 0);SWAP;
	ticketTaken = createSemaphore("ticketTaken", BINARY, 0);SWAP;
	canBuyTicket = createSemaphore("canBuyTicket", BINARY, 1);SWAP;
	inMuseum = createSemaphore("inMuseum", COUNTING, MAX_IN_MUSEUM);SWAP;
	inGiftShop = createSemaphore("inGiftShop", COUNTING, MAX_IN_GIFTSHOP);SWAP;

	//?? create car, driver, and visitor tasks here
	// create cars
	for (i = 0; i < NUM_CARS; i++) {						SWAP;
		sprintf(buf1, "carTask[%d]", i);					SWAP;
		newArgv[0] = buf1;									SWAP;
		sprintf(buf2, "%d", i);								SWAP;
		newArgv[1] = buf2;									SWAP;
		createTask(buf1, P3_carTask, MED_PRIORITY, 2, newArgv);SWAP;
	}

	// create drivers
	for (i = 0; i < NUM_DRIVERS; i++) {						SWAP;
		sprintf(buf1, "driverTask[%d]", i);					SWAP;
		newArgv[0] = buf1;									SWAP;
		sprintf(buf2, "%d", i);								SWAP;
		newArgv[1] = buf2;									SWAP;
		createTask(buf1, P3_driverTask, MED_PRIORITY, 2, newArgv);SWAP;
	}

	// create visitors
	for (i = 0; i < numVisitors; i++) {						SWAP;
		sprintf(buf1, "visitorTask[%d]", i);				SWAP;
		newArgv[0] = buf1;									SWAP;
		sprintf(buf2, "%d", i);								SWAP;
		newArgv[1] = buf2;									SWAP;
		createTask(buf1, P3_visitorTask, MED_PRIORITY, 2, newArgv);SWAP;
	}														SWAP;

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
			if (i == NUM_SEATS - 1) {						SWAP;
				// signal need driver to drive
				SEM_SIGNAL(needDriver);						SWAP;
				// wake up driver
				SEM_SIGNAL(wakeupDriver);					SWAP;
				// wait for driver
				SEM_WAIT(isDriverReady);					SWAP;
				// save driver mutex
				driver = driverMailbox;						SWAP;
			}												SWAP;
			// signal passenger seated to park
			SEM_SIGNAL(seatFilled[carId]);					SWAP;
		}													SWAP;
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
		SEM_WAIT(wakeupDriver);								SWAP;
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
		else if (SEM_TRYLOCK(needTicket)) {					SWAP;
			// wait for can sell ticket
			SEM_WAIT(canSellTicket);						SWAP;
			// update park struct with driver status
			PARK_WAIT;										SWAP;
			myPark.drivers[driverId] = -1;					SWAP;
			PARK_SIGNAL;									SWAP;
			// signal a ticket is available
			SEM_SIGNAL(isTicketAvailable);					SWAP;
			// wait for ticket to be taken
			SEM_WAIT(ticketTaken);							SWAP;
			// update park struct with driver status
			PARK_WAIT;										SWAP;
			myPark.drivers[driverId] = 0;					SWAP;
			PARK_SIGNAL;									SWAP;
			// signal can sell ticket
			SEM_SIGNAL(canSellTicket);						SWAP;
		}
		else {												SWAP;
			break;
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

	// wait before entering the park
	randomWait(notifyVisitor, MAX_WAIT);					SWAP;
	// get inside the park
	SEM_WAIT(enterPark);									SWAP;
	// update park struct to enter park
	PARK_WAIT;												SWAP;
	myPark.numOutsidePark--;								SWAP;
	myPark.numInPark++;										SWAP;
	myPark.numInTicketLine++;								SWAP;
	PARK_SIGNAL;											SWAP;
	// wait before requesting a ticket
	randomWait(notifyVisitor, MAX_WAIT);					SWAP;
	// get a ticket for a tour
	SEM_WAIT(canBuyTicket);									SWAP;
	// wait for an available ticket
	SEM_WAIT(rideTicket);									SWAP;
	// signal need driver for a ticket
	SEM_SIGNAL(needTicket);									SWAP;
	// wake up a driver
	SEM_SIGNAL(wakeupDriver);								SWAP;
	// wait for a ticket
	SEM_WAIT(isTicketAvailable);							SWAP;
	SEM_SIGNAL(ticketTaken);								SWAP;
	SEM_SIGNAL(canBuyTicket);								SWAP;

	// get in line for the museum
	PARK_WAIT;												SWAP;
	myPark.numTicketsAvailable--;							SWAP;
	myPark.numInTicketLine--;								SWAP;
	myPark.numInMuseumLine++;								SWAP;
	PARK_SIGNAL;											SWAP;

	// wait before trying to entering museum
	randomWait(notifyVisitor, MAX_WAIT);					SWAP;

	// wait to get in museum
	SEM_WAIT(inMuseum);										SWAP;

	// get into the museum
	PARK_WAIT;												SWAP;
	myPark.numInMuseumLine--;								SWAP;
	myPark.numInMuseum++;									SWAP;
	PARK_SIGNAL;											SWAP;

	SEM_SIGNAL(inMuseum);									SWAP;

	// get in line for a tour
	PARK_WAIT;												SWAP;
	myPark.numInMuseum--;									SWAP;
	myPark.numInCarLine++;									SWAP;
	PARK_SIGNAL;											SWAP;
	// take a guided tour			
	// wait for permission to take seat
	SEM_WAIT(canTakeSeat);									SWAP;
	// wait for seat open
	SEM_WAIT(isSeatOpen);									SWAP;
	// put semaphore in mailbox
	passengerMailbox = notifyVisitor;						SWAP;
	// move visitor from line to car in park
	PARK_WAIT;												SWAP;
	myPark.numInCarLine--;									SWAP;
	myPark.numInCars++;										SWAP;
	PARK_SIGNAL;											SWAP;
	// take seat
	SEM_SIGNAL(isSeatTaken);								SWAP;
	// signal can take seat
	SEM_SIGNAL(canTakeSeat);								SWAP;
	// wait for ride to be over
	SEM_WAIT(notifyVisitor);								SWAP;
	// return ticket
	SEM_SIGNAL(rideTicket);									SWAP;
	
	PARK_WAIT;												SWAP;
	myPark.numInCars--;										SWAP;
	myPark.numTicketsAvailable++;							SWAP;
	myPark.numInGiftLine++;									SWAP;
	PARK_SIGNAL;											SWAP;

	// wait before trying to get in gift shop
	randomWait(notifyVisitor, MAX_WAIT);					SWAP;
	SEM_WAIT(inGiftShop);									SWAP;

	PARK_WAIT;												SWAP;
	myPark.numInGiftLine--;									SWAP;
	myPark.numInGiftShop++;									SWAP;
	PARK_SIGNAL;											SWAP;
	SEM_SIGNAL(inGiftShop);									SWAP;

	// update park struct to exit park
	PARK_WAIT;												SWAP;
	myPark.numInGiftShop--;									SWAP;
	myPark.numInPark--;										SWAP;
	myPark.numExitedPark++;									SWAP;
	PARK_SIGNAL;											SWAP;
	// leave park
	SEM_SIGNAL(enterPark);									SWAP;

	return 0;
}

void randomWait(Semaphore* s, int max) {					SWAP;
	int r = (rand() % max) + 1;								SWAP;
	insertDC(dc, r, s);										SWAP;
	SEM_WAIT(s);											SWAP;
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
	char svtime[64];										SWAP;// ascii current time
	clock_t currentTime;									SWAP;// current time
	dcLastDecTime = clock();								SWAP;

	while (1)
	{														SWAP;
		SEM_WAIT(tics10thsec);								SWAP;
		// printf("\nTime = %s\n", myTime(svtime));
		currentTime = clock();								SWAP;
		// printf("Clocks_per_sec: %f\n", CLOCKS_PER_SEC);
		int diff = currentTime - dcLastDecTime + 100;		SWAP;
		// printf ("currentTime: %d\tdcLastDecTime: %d\tdelta: %d seconds: %f\n", currentTime, dcLastDecTime, diff, ((float)diff)/CLOCKS_PER_SEC);
		for (int i = 100000; i < diff; i += ONE_TENTH_SEC) {SWAP;
			decDC(dc);										SWAP;
		}													SWAP;
		dcLastDecTime = currentTime;						SWAP;
	}														SWAP;
	return 0;
} // end timeTask

// ********************************************************************************************
// print delta clock
void printDC(deltaClock* c) {
	SEM_WAIT(dcMutex);										SWAP;
	int count = c->clock[0].count;							SWAP;
	int i;													SWAP;
	for (i = count; i > 0; i--)								
	{														SWAP;
		int t = c->clock[i].entry.time;						SWAP;
		char* s = c->clock[i].entry.sem->name;				SWAP;
		printf("\n%4d%4d  %-20s", i, t, s);					SWAP;
	}														SWAP;
	printf("\n%4d%4d", 0, count);							SWAP;
	SEM_SIGNAL(dcMutex);									SWAP;
	return;
}

// ********************************************************************************************
// Inserts time and semaphore into the delta clock
int insertDC(deltaClock* c, int t, Semaphore* sem) {		SWAP;
	SEM_WAIT(dcMutex);										SWAP;
	int temp = t;											SWAP;
	int count = c->clock[0].count;							SWAP;
	// delta clock is empty
	if (count == 0) {										SWAP;
		dc_entry entry =  { .time = t, .sem = sem };		SWAP;
		c->clock[1].entry = entry;							SWAP;
		c->clock[0].count++;								SWAP;
		SEM_SIGNAL(dcMutex);								SWAP;
		return 1;
	}
	// delta clock is full
	else if (count == MAX_TASKS) {							SWAP;
		SEM_SIGNAL(dcMutex);								SWAP;
		return -1;
	}
	// otherwise
	else {													SWAP;
		for (int i = count + 1; i > 0; --i) {				SWAP;
			int delta = temp - c->clock[i - 1].entry.time;	SWAP;
			// printf("\ndelta = temp - c->clock[i - 1].entry.time: %d = %d - %d", delta, temp, c->clock[i - 1].entry.time);
			if (delta < 0 || i == 1) {// (temp < current value) => The new value goes in i
				dc_entry entry = { .time = temp, .sem = sem };SWAP;
				c->clock[i].entry = entry;					SWAP;
				c->clock[0].count++;						SWAP;
				if (i > 1) {								SWAP;
					c->clock[i - 1].entry.time = abs(delta);SWAP;
				}											SWAP;
				SEM_SIGNAL(dcMutex);						SWAP;
				return i;
			}
			else {// (temp >= curret value) => The i - 1 value goes in i
				c->clock[i].entry = c->clock[i - 1].entry;	SWAP;
				temp = delta;								SWAP;
			}
		}		
	}
}

// ********************************************************************************************
// decrements the top value of the delta clock
void decDC(deltaClock* c) {									SWAP;
	// printDC(c);									
	SEM_WAIT(dcMutex);										SWAP;
	int count = c->clock[0].count;							SWAP;

	if (count == 0) { SEM_SIGNAL(dcMutex); SWAP; return; }		
	c->clock[count].entry.time--;							SWAP;
	while (c->clock[count].entry.time == 0 && count > 0) {	SWAP;
		SEM_SIGNAL(c->clock[count].entry.sem);				SWAP;
		c->clock[0].count--;								SWAP;
		// SEM_SIGNAL(dcChange);
		count = c->clock[0].count;							SWAP;
	}														SWAP;
	SEM_SIGNAL(dcMutex);									SWAP;
	return;
}






