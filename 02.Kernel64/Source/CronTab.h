#ifndef CRONTAB_H
#define CRONTAB_H
#include "Types.h"

typedef struct Wildcard_{
	char minutes[2];
	char hours;
	char mday;		// 일 
	char wday;		// 요일
	char month;		// 달
} Wildcard;

typedef struct Cron_{
	Wildcard wildcard;
	char *command;
} Cron;

typedef struct Crontab_{
	char* user;
	time_t mtime;	// 수정 시간
	int flags;
	cron* list;
}Crontab;

#endif