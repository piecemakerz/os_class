#include "TaskScheduler.h"

static TASKTRIGGER triggers[MAX_NUM_SCHEDULED_TASK];
static int numScheduledTask;

void initTriggers()
{
    int i;
    int j;
    for(i = 0; i < MAX_NUM_SCHEDULED_TASK; i++)
    {
        triggers[i].taskType = 0;
        triggers[i].date = 0;
        triggers[i].once = 1;
        for(j = 0; j < MAX_NUM_SCHEDULER_PARAMETER; j++)
        {
            triggers[i].parameter[j] = '\0';
        }
    }
    numScheduledTask = 0;

    //startScheduler(); //for debug
    if( kCreateTask( TASK_FLAGS_LOW | TASK_FLAGS_THREAD, 0, 0, ( QWORD ) startScheduler ) == NULL )
    {
        kPrintf("Task Scheduelr Fail\n");
    }
}

static void startScheduler()
{
    int i;
    //triggers[0].taskType = 1; //for debug
    //triggers[0].parameter[0] = 'H'; //for debug
    //triggers[0].parameter[1] = '\0'; //for debug
    while(1)
    {
        for(i = 0; i < MAX_NUM_SCHEDULED_TASK; i++){
            if(triggers[i].taskType)
            {
                //if(triggers[i].date == presentDate)
                {
                    if(triggers[i].taskType == 1)
                    {
                        kPrintf( "====================================================\n" );
                        kPrintf( "%s\n", triggers[i].parameter );
                        kPrintf( "====================================================\n" );
                        checkOnce(i);
                    }
                    else if(triggers[i].taskType == 2)
                    {
                        //
                        checkOnce(i);
                    }
                }
            }
        }
        //break; //for debug
        kSchedule();
    }
    kExitTask();
}

void checkOnce(int i)
{
    int j;
    if(triggers[i].once)
    {
        triggers[i].taskType = 0;
        triggers[i].date = 0;
        triggers[i].once = 1;
        for(j = 0; j < MAX_NUM_SCHEDULER_PARAMETER; j++)
        {
            triggers[i].parameter[j] = '\0';
        }
    }
    else
    {
        //triggers[i].date = presentDate;
    }
}
