#ifndef __TASKSCHEDULER_H__
#define __TASKSCHEDULER_H__

#include "Types.h"
#include "Task.h"
#include "Console.h"
#include "ConsoleShell.h"
#include "RTC.h"

#define MAX_NUM_SCHEDULED_TASK 10
#define MAX_LENGTH_SCHEDULER_PARAMETER 100

typedef struct taskTriggerStruct
{
    int taskType; // 0 is nothing
    int year, month, day, hour, minute;
    char parameter[MAX_LENGTH_SCHEDULER_PARAMETER];
} TASKTRIGGER;

void initScheduler();
static void startScheduler();
void addTrigger(int taskType, int year, int month, int day, int hour, int minute, char* parameter);
int cmpPresentDate(TASKTRIGGER *trigger);
void resetTrigger(TASKTRIGGER *trigger);
void showTriggers();
void deleteTrigger(int i);

#endif /*__TASKSCHEDULER_H__*/
