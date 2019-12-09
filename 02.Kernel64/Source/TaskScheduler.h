#ifndef __TASKSCHEDULER_H__
#define __TASKSCHEDULER_H__

#include "Types.h"
#include "Task.h"
#include "Console.h"
#include "ConsoleShell.h"
#include "RTC.h"

#define MAX_NUM_SCHEDULED_TASK 10
#define MAX_LENGTH_SCHEDULER_PARAMETER 100
#define TASK_REPEAT 84515

typedef struct TaskTrigger TASKTRIGGER;

struct TaskTrigger{
    TASKTRIGGER* prev;
    TASKTRIGGER* next;
    int taskType; // 0 is nothing
    int year, month, day, hour, minute, lastMinute;
    char parameter[MAX_LENGTH_SCHEDULER_PARAMETER];
};

extern void initScheduler();
extern void addTrigger(int taskType, int year, int month, int day, int hour, int minute, char* parameter);
extern void deleteTrigger(int i);
extern void showTriggers();

static void _startScheduler();
static int _cmpPresentDate(TASKTRIGGER *trigger);
static void resetTrigger(TASKTRIGGER *trigger);
static TASKTRIGGER* _getTrigger(int i);
static void _deleteTrigger(TASKTRIGGER* trigger);

#endif /*__TASKSCHEDULER_H__*/
