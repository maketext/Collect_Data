#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <sys/select.h>
#include <unistd.h>
#include "sqlite3.h"
#define MAX_THREAD 10
#define MAX_MAT 8

typedef struct mat_xy {
	int y, x, ha, hs;
}mat_;
typedef struct command_parcel {
	int index;
	char syscommand[32];
	mat_ mat[MAX_MAT];
	int mat_size;
	char filename[16];
	char filename2[16];
}PARCEL;
typedef struct parse_datetime {
	char year[5], mon[3], mday[3], date[11], hour[3], min[3], time[6];
}DT;

sqlite3 *db;
char sql[128];

void datetime_parse(DT*);
void itoa(int, char*, int);
void dtos(time_t *, char *);
int mat_match(int, int, mat_ *, int);
int main()
{
	int rc;
	sqlite3_stmt *stmt;

	// DB Open
	rc = sqlite3_open_v2("db", &db, SQLITE_OPEN_READWRITE, NULL);
	if(rc != SQLITE_OK)
	{
		rc = sqlite3_open_v2("db", &db, SQLITE_OPEN_READWRITE|SQLITE_OPEN_CREATE, NULL);
		if(rc != SQLITE_OK)
		{
			sqlite3_close_v2(db);
			printf("Err: DB open failed.\n");
			return 1;
		}
		strcpy(sql, "CREATE TABLE TB_VAL(PK INTEGER PRIMARY KEY, TM TEXT, COMM TEXT, HA INTEGER,  HS INTEGER, VAL TEXT);");
		rc = sqlite3_prepare_v2(db, sql, sizeof(sql), &stmt, NULL);
		if(rc != SQLITE_OK)
		{
			sqlite3_finalize(stmt);
			sqlite3_close_v2(db);
			printf("Err: DB create1 failed.\n");
			return 1;
		}
		rc = sqlite3_step(stmt);
		if(rc != SQLITE_DONE)
		{
			sqlite3_finalize(stmt);
			sqlite3_close_v2(db);
			printf("Err: DB create2 failed.\n");
			return 1;
		}
		sqlite3_finalize(stmt);
		printf("DB Table Created.\n");
	}
	sqlite3_close_v2(db);
	printf("DB initalized.\n");

	rc = sqlite3_open_v2("db", &db, SQLITE_OPEN_READWRITE, NULL);
	if(rc != SQLITE_OK)
	{
		printf("DB init error.\n");
		return 1;
	}
	pthread_t thread_id[MAX_THREAD];
	PARCEL parcel[10] = {
				{1, "sar -u 1 5", {{8, 1, 1, 1}, {8, 2, 1, 2}, {8, 4, 1, 4}}, 3, "t_sar_u.txt", "sar_u"},
				{2, "vmstat 1 5", {{6, 3, 4, 1}, {6, 4, 8, 1}, {6, 18, 16, 1}}, 3, "t_vmstat.txt", "vmstat"},
				{3, "sar -mcgpq 1 5", {{33, 1, 32, 1}, {33, 2, 64, 1}, {34, 0, 128, 1}, {34, 1, 256, 1}, {34, 2, 256, 2}, {35, 1, 512, 1}, {36, 2, 512, 2}, {37, 0, 1024, 1}}, 8, "t_sar.txt", "sar"},
				{4, "zpool list | grep rpool", {{0, 1, 2048, 1}, {0, 2, 2048, 2}, {0, 3, 2048, 4}}, 3, "t_zpool.txt", "zpool"},
				{5, "df -k | grep /dev/dsk", {{0, 0, 4096, 8}, {0, 2, 4096, 1}, {0, 3, 4096, 2}, {0, 4, 4096, 4}, {0, 5, 4096, 16}}, 5, "t_df.txt", "df"},
		};

		int mat_index, i, j;
		char str[64], str_datetime[17];
		FILE *fp;
		time_t timer;
		struct tm *t;
		struct timeval tv = {.tv_sec = 0, .tv_usec = 500 * 1000};
		char *s, ch;
		int c, row, col;
		while(1)
		{
			select(0, NULL, NULL, NULL, &tv);
			timer = time(NULL);
			t = localtime(&timer);
			if(t->tm_sec != 0) continue;

			for(j = 0; j < 5; j++)
			{
				// parse
				if(!(fp = popen(parcel[j].syscommand, "r")))
				{
					fprintf(stderr, "popen: could not read %s file!\n", parcel[j].filename);
					continue;
				}

				// date-time parse
				DT datetime;
				datetime_parse(&datetime);
				row = 0;
				col = -1;
				while(1)
				{
					s = str;
					while(1)
					{
						c = fgetc(fp);
						ch = (char)c;
						if(c == EOF)
							break;
						if(ch == '\n')
							break;
						if(ch == ' ')
							break;
						*s++ = c;
					}
					if(c == EOF) break;
					*s = '\0';
					if(strlen(str) == 0)
					{
						if(ch == '\n')
						{
							row++;
							col = -1;
						}
						continue;
					}
					col++;

					if((mat_index = mat_match(col, row, parcel[j].mat, parcel[j].mat_size)) != -1)
					{
						strcpy(sql, "INSERT INTO TB_VAL (TM, COMM, HA, HS, VAL) VALUES(datetime(?), ?, ?, ?, ?);");
						rc = sqlite3_prepare_v2(db, sql, sizeof(sql), &stmt, NULL);
						if(rc != SQLITE_OK)
						{
							fprintf(stderr, "err\n");
							sqlite3_reset(stmt);
							continue;
						}
						snprintf(str_datetime, 17, "%s %s", datetime.date, datetime.time);
						sqlite3_bind_text(stmt, 1, str_datetime, -1, NULL);
//						fprintf(stderr, "%s\n", str);
						sqlite3_bind_text(stmt, 2, parcel[j].syscommand, -1, NULL);
						sqlite3_bind_int(stmt, 3, parcel[j].mat[mat_index].ha);
						sqlite3_bind_int(stmt, 4, parcel[j].mat[mat_index].hs);
						sqlite3_bind_text(stmt, 5, str, -1, NULL);
						rc = sqlite3_step(stmt);
						if(rc != SQLITE_DONE)
						{
							fprintf(stderr, "err\n");
							sqlite3_reset(stmt);
							continue;
						}
						sqlite3_reset(stmt);
//						fprintf(stderr, "(%s) (%s)\n", datetime.date, parcel[j].syscommand);
					}
					if(ch == '\n')
					{
						row++;
						col = -1;
					}
				}
				pclose(fp);
			}
		}
		sqlite3_finalize(stmt);
		sqlite3_close_v2(db);
}
int mat_match(int x, int y, mat_ *arr, int size)
{
	int i;
	for(i = 0; i < size; i++)
		if(x == (arr + i)->x && y == (arr + i)->y)
			return i;
	return -1;
}
void datetime_parse(DT *dt)
{
			time_t timer;
		    timer = time(NULL);
		    struct tm *t;
		    t = localtime(&timer);
		    itoa(t->tm_year + 1900, dt->year, 4);
		    itoa(t->tm_mon + 1, dt->mon, 2);
			itoa(t->tm_mday, dt->mday, 2);
			itoa(t->tm_hour, dt->hour, 2);
			itoa(t->tm_min, dt->min, 2);
			snprintf(dt->date, 11, "%s-%s-%s", dt->year, dt->mon, dt->mday);
			snprintf(dt->time, 6, "%s:%s", dt->hour, dt->min);
}
void itoa(int val, char *des, int size){
    if(val < 10)
    		snprintf(des, size + 1, "0%d", val);
    else
		snprintf(des, size + 1, "%d", val);
}





