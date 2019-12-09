#include "TaskScheduler.h"
#include "DynamicMemory.h"
#include "Utility.h"
#include "FileSystem.h"

// static TASKTRIGGER triggers[MAX_NUM_SCHEDULED_TASK];
static TASKTRIGGER* headTrigger;
static int numScheduledTask;
static BOOL thisIsRepeatedTask;

void initScheduler()
{
    int i;
    int j;
    headTrigger = (TASKTRIGGER*)kAllocateMemory(sizeof(TASKTRIGGER));
    headTrigger->prev = headTrigger;
    headTrigger->next = headTrigger;

    _loadSchedule();

    if( kCreateTask( TASK_FLAGS_LOW | TASK_FLAGS_THREAD, 0, 0, ( QWORD ) _startScheduler ) == NULL )
    {
        kPrintf("Task Scheduelr Fail\n");
    }
}



void addTrigger(int taskType, int year, int month, int day, int hour, int minute, char* parameter)
{
    int j;
    TASKTRIGGER* prevTrigger = headTrigger;
    while(prevTrigger->next != prevTrigger){
         prevTrigger = prevTrigger->next;
    }

    TASKTRIGGER* newTrigger = (TASKTRIGGER*)kAllocateMemory(sizeof(TASKTRIGGER));
    newTrigger->prev = prevTrigger;
    prevTrigger->next = newTrigger;
    newTrigger->next = newTrigger;

    if((taskType) > 0)
    {
        newTrigger->taskType = taskType;
        newTrigger->year = year;
        newTrigger->month = month;
        newTrigger->day = day;
        newTrigger->hour = hour;
        newTrigger->minute = minute;
        newTrigger->lastMinute = 415325136;
        j = 0;
        while(1)
        {
            if(parameter[j] == '\0')
            {
                newTrigger->parameter[j] = parameter[j];
                break;
            }
            else
            {
                newTrigger->parameter[j] = parameter[j];
                j++;
            }
        }
    }

}

void showTriggers()
{
    int i = 0; // 0은 headTrigger, 1부터 정상 trigger
    TASKTRIGGER* trigger = headTrigger;
    if (trigger->next != trigger)
        kPrintf("[id] YYYY MM DD HH MM EVENT\n");
    while(trigger->next != trigger){
        trigger = trigger->next;
        kPrintf("[%2d] ", ++i);
        trigger->year  == TASK_REPEAT ? kPrintf("   %c ", '*') : kPrintf("%4d ", trigger->year);;
        trigger->month == TASK_REPEAT ? kPrintf(" %c ", '*') : kPrintf("%2d ", trigger->month);
        trigger->day   == TASK_REPEAT ? kPrintf(" %c ", '*') : kPrintf("%2d ", trigger->day);
        trigger->hour  == TASK_REPEAT ? kPrintf(" %c ", '*') : kPrintf("%2d ", trigger->hour);
        trigger->minute== TASK_REPEAT ? kPrintf(" %c ", '*') : kPrintf("%2d ", trigger->minute);
        kPrintf("%s\n", trigger->parameter);
    }
    kPrintf("Number of Scheduled Tasks: %d\n", i);
}

void deleteTrigger(int i)
{
    //resetTrigger(&(triggers[i]));
    TASKTRIGGER* trigger = _getTrigger(i);
    _deleteTrigger(trigger);
    kPrintf("ID %d is deleted.\n", i);
    numScheduledTask--;
}

static void _loadSchedule(){
    TASKTRIGGER* trigger = headTrigger;
    TASKTRIGGER load;
    FILE* fp = fopen(SCHEDULE_FILE, "r");
    if (fp!=NULL){
        while(fread(&load, sizeof(TASKTRIGGER), 1, fp) == 1){
            trigger->next = (TASKTRIGGER*)kAllocateMemory(sizeof(TASKTRIGGER));
            trigger->next->prev = trigger;
            trigger = trigger->next;
            trigger->next = trigger;

            trigger->taskType = load.taskType;
            trigger->year     =load.year;
            trigger->month    =load.month;
            trigger->day      =load.day;
            trigger->hour     =load.hour;
            trigger->minute   =load.minute;

            int i=-1;
            do{
                i++;
                trigger->parameter[i] = load.parameter[i];
            }while(load.parameter[i] != '\0');
            
        }
    }
    fclose(fp);
}

static void _saveSchedule(){
    BOOL success = FALSE;
    while(!success){
        TASKTRIGGER* trigger = headTrigger;

        FILE* fp = fopen(SCHEDULE_FILE, "w");
        if (fp!=NULL){
            while(trigger->next != trigger){
                trigger = trigger->next;
                if(fwrite(trigger, sizeof(TASKTRIGGER), 1, fp) == sizeof(TASKTRIGGER))
                    break;
            }
            if(trigger == trigger->next) break;
        }
        fclose(fp);
    }
}

static void _startScheduler()
{
    int i;
    int j = 0;
    TASKTRIGGER *trigger;
    while(1)
    {
        i=0;
        trigger = headTrigger;
        if(kGetProcessorLoad() < 50){
            while (trigger->next != trigger){
                i++;
                trigger = trigger->next;
                if(_cmpPresentDate(trigger) < 1){
                    if(trigger->taskType == 1){ // Notification
                        kPrintf("[Notification]%s\n", trigger->parameter);
                    }else if(trigger->taskType == 2){
                        kExecuteCommand(trigger->parameter);
                    }else if(trigger->taskType == 3){
                        kExecuteCommand(trigger->parameter);
                    }
                    if(thisIsRepeatedTask == FALSE){
                        _deleteTrigger(trigger);
                        numScheduledTask--;
                    }
                }
            }
        }
        while(j++ > 30){
            _saveSchedule();
            j=0;
        }
        kSleep(1000);
    }
    kExitTask();
}

static int _cmpPresentDate(TASKTRIGGER *trigger)
{
    BYTE bSecond, bMinute, bHour;
    BYTE bDayOfWeek, bDayOfMonth, bMonth;
    WORD wYear;

    kReadRTCTime( &bHour, &bMinute, &bSecond );
    kReadRTCDate( &wYear, &bMonth, &bDayOfMonth, &bDayOfWeek );

    if(trigger->year    == TASK_REPEAT ||
        trigger->month  == TASK_REPEAT ||
        trigger->day    == TASK_REPEAT ||
        trigger->hour   == TASK_REPEAT ||
        trigger->minute == TASK_REPEAT)
        thisIsRepeatedTask = TRUE;

    if(trigger->lastMinute == bMinute){
        return 1;
    }else
    {
        trigger->lastMinute = bMinute;
    }
    

    if(trigger->year == wYear || trigger->year == TASK_REPEAT)
    {
        if(trigger->month == bMonth || trigger->month == TASK_REPEAT)
        {
            if(trigger->day == bDayOfMonth || trigger->day == TASK_REPEAT)
            {
                if(trigger->hour == bHour || trigger->hour == TASK_REPEAT)
                {
                    if(trigger->minute == bMinute || trigger->minute == TASK_REPEAT)
                    {
                        return 0;
                    }
                    else if(trigger->minute > bMinute || trigger->minute == TASK_REPEAT)
                    {
                        return 1;
                    }
                    else
                    {
                        return -1;
                    }
                }
                else if(trigger->hour > bHour || trigger->hour == TASK_REPEAT)
                {
                    return 1;
                }
                else
                {
                    return -1;
                }
            }
            else if(trigger->day > bDayOfMonth || trigger->day == TASK_REPEAT)
            {
                return 1;
            }
            else
            {
                return -1;
            }
        }
        else if(trigger->month > bMonth || trigger->month == TASK_REPEAT)
        {
            return 1;
        }
        else
        {
            return -1;
        }
    }
    else if(trigger->year > wYear || trigger->year == '*')
    {
        return 1;
    }
    else
    {
        return -1;
    }
}

static void resetTrigger(TASKTRIGGER *trigger)
{
    trigger->taskType = 0;
    trigger->year = 0;
    trigger->month = 0;
    trigger->day = 0;
    trigger->hour = 0;
    trigger->minute = 0;
}static TASKTRIGGER* _getTrigger(int i){
    TASKTRIGGER* trigger = headTrigger;
    for(i=i<0?0:i; i!=0; i--){
        trigger = trigger->next;
    }
    if(trigger == headTrigger) return NULL;
    return trigger;
}

static void _deleteTrigger(TASKTRIGGER* trigger){
    if(trigger == NULL) return; // no trigger
    if(trigger->prev == trigger) return; // this is head trigger;
    trigger->next->prev = trigger->prev;

    if(trigger->next == trigger)
        trigger->prev->next = trigger->prev;
    else
        trigger->prev->next = trigger->next;
    kFreeMemory(trigger);

}
