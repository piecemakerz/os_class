#ifndef __TASKSCHEDULER_H__
#define __TASKSCHEDULER_H__

#include "Types.h"
#include "Task.h"
#include "Console.h"
#include "ConsoleShell.h"

#define MAX_NUM_SCHEDULED_TASK 10
#define MAX_NUM_SCHEDULER_PARAMETER 100

typedef struct taskTriggerStruct
{
    int taskType; // 0 is nothing
    int date;
    BOOL once;
    char parameter[MAX_NUM_SCHEDULER_PARAMETER];
} TASKTRIGGER;

void initTriggers();
static void startScheduler();
void checkOnce(int i);
void addTrigger(int taskType, int date, BOOL once, char* parameter);

#endif /*__TASKSCHEDULER_H__*/
