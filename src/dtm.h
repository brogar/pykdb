#ifndef DTM_H
#define DTM_H

int normalize_datetime(int *year, int *month, int *day, int *hour, int *minute, int *second, int *microsecond) ;
int check_time_args(int h, int m, int s, int us) ;
void normalize_pair(int *hi, int *lo, int factor) ;
int normalize_date(int *year, int *month, int *day) ;


#endif

