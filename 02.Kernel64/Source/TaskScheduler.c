#include "TaskScheduler.h"

static TASKTRIGGER triggers[MAX_NUM_SCHEDULED_TASK];
static int numScheduledTask;

void initScheduler()
{
    int i;
    int j;
    for(i = 0; i < MAX_NUM_SCHEDULED_TASK; i++)
    {
        resetTrigger(&(triggers[i]));
        for(j = 0; j < MAX_LENGTH_SCHEDULER_PARAMETER; j++)
        {
            triggers[i].parameter[j] = '\0';
        }
    }
    numScheduledTask = 0;

    if( kCreateTask( TASK_FLAGS_LOW | TASK_FLAGS_THREAD, 0, 0, ( QWORD ) startScheduler ) == NULL )
    {
        kPrintf("Task Scheduelr Fail\n");
    }
}

static void startScheduler()
{
    int i;
    while(1)
    {
        for(i = 0; i < MAX_NUM_SCHEDULED_TASK; i++){
            if(triggers[i].taskType)
            {
                if(cmpPresentDate(&(triggers[i])) < 1)
                {
                    if(triggers[i].taskType == 1)
                    {
                        kPrintf( "Notification Message~!!\n" );
                        kPrintf( "%s\n", triggers[i].parameter );
                    }
                    else if(triggers[i].taskType == 2)
                    {
                        kExecuteCommand( triggers[i].parameter );
                    }
                    resetTrigger(&(triggers[i]));
                    numScheduledTask--;
                }
            }
        }
        kSchedule();
    }
    kExitTask();
}

void addTrigger(int taskType, int year, int month, int day, int hour, int minute, char* parameter)
{
    int i;
    int j;
    if(numScheduledTask == MAX_NUM_SCHEDULED_TASK)
    {
        kPrintf("Task list is full.\n");
    }
    else{
        for(i = 0; i < MAX_NUM_SCHEDULED_TASK; i++){
            if(!(triggers[i].taskType))
            {
                triggers[i].taskType = taskType;
                triggers[i].year = year;
                triggers[i].month = month;
                triggers[i].day = day;
                triggers[i].hour = hour;
                triggers[i].minute = minute;
                j = 0;
                while(1)
                {
                    if(parameter[j] == '\0')
                    {
                        triggers[i].parameter[j] = parameter[j];
                        break;
                    }
                    else
                    {
                        triggers[i].parameter[j] = parameter[j];
                        j++;
                    }
                }
                numScheduledTask++;
                break;
            }
        }
    }
}

int cmpPresentDate(TASKTRIGGER *trigger)
{
    BYTE bSecond, bMinute, bHour;
    BYTE bDayOfWeek, bDayOfMonth, bMonth;
    WORD wYear;

    kReadRTCTime( &bHour, &bMinute, &bSecond );
    kReadRTCDate( &wYear, &bMonth, &bDayOfMonth, &bDayOfWeek );

    if(trigger->year == wYear)
    {
        if(trigger->month == bMonth)
        {
            if(trigger->day == bDayOfMonth)
            {
                if(trigger->hour == bHour)
                {
                    if(trigger->minute == bMinute)
                    {
                        return 0;
                    }
                    else if(trigger->minute > bMinute)
                    {
                        return 1;
                    }
                    else
                    {
                        return -1;
                    }
                }
                else if(trigger->hour > bHour)
                {
                    return 1;
                }
                else
                {
                    return -1;
                }
            }
            else if(trigger->day > bDayOfMonth)
            {
                return 1;
            }
            else
            {
                return -1;
            }
        }
        else if(trigger->month > bMonth)
        {
            return 1;
        }
        else
        {
            return -1;
        }
    }
    else if(trigger->year > wYear)
    {
        return 1;
    }
    else
    {
        return -1;
    }
}

void resetTrigger(TASKTRIGGER *trigger)
{
    trigger->taskType = 0;
    trigger->year = 0;
    trigger->month = 0;
    trigger->day = 0;
    trigger->hour = 0;
    trigger->minute = 0;
}

void showTriggers()
{
    int i;
    kPrintf("Number of Scheduled Tasks: %d\n", numScheduledTask);
    for(i = 0; i < MAX_NUM_SCHEDULED_TASK; i++)
    {
        if(triggers[i].taskType)
        {
            kPrintf("Scheduler ID: %d\n", i);
        }
    }
}

void deleteTrigger(int i)
{
    resetTrigger(&(triggers[i]));
    kPrintf("ID %d is deleted.\n", i);
    numScheduledTask--;
}
