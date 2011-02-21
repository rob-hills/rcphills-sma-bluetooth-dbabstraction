#include "db_interface.h"
#include <stdio.h>
#include <time.h>
#include <assert.h>
#include <string.h>

#define DATE_STR_LENGTH 25

int main( int argc, char** argv )
{
  db_init(NULL,NULL,NULL,"./test.db");
  int result = db_install_tables();
  printf("db_install_tables: %i\n", result );
 
  assert( result == 1 );
  
  result = db_get_schema();
  printf("db_get_schema: %i\n", result );

  assert( result == 1 );
  
  struct tm date;
  time_t t;
  char sunrise[DATE_STR_LENGTH];
  char sunset[DATE_STR_LENGTH];
  char sunrise_get[DATE_STR_LENGTH];
  char sunset_get[DATE_STR_LENGTH];
  
  time( &t );
  t+=3600;
  date = *(localtime( &t ));
  strftime(sunset,DATE_STR_LENGTH,"%Y-%m-%d %H:%M:%S",&date);

  time( &t );
  t-=3600;
  date = *(localtime( &t ));
  strftime(sunrise,DATE_STR_LENGTH,"%Y-%m-%d %H:%M:%S",&date);
  
  result = db_update_almanac(&date, sunrise, sunset );
  printf("db_update_almanac: %i %s %s\n", result,sunrise, sunset );
  
  assert( result == 1 );

  
  sunrise_get[0] ='\0';
  sunset_get[0] = '\0';
  result = db_fetch_almanac( &date, sunrise_get, sunset_get );
  printf("db_fetch_almanac: %i %s %s\n", result,sunrise_get, sunset_get );
  
  assert( result == 1);
  assert( strcmp( sunrise, sunrise_get ) == 0 );
  assert( strcmp( sunset, sunset_get ) == 0 );
  
  //same day next year - should fail (no row found)
  sunrise_get[0] ='\0';
  sunset_get[0] = '\0';
  date.tm_year++;
  result = db_fetch_almanac( &date, sunrise_get, sunset_get );
  printf("db_fetch_almanac: %i %s %s\n", result,sunrise_get, sunset_get );
  assert( result == 0 );
  
  
  time( &t );
  date = *(localtime( &t ));
  struct tm curr_date = *(localtime( &t ));
  date.tm_min=5;
  date.tm_sec=0;
  strftime(sunrise,DATE_STR_LENGTH,"%Y-%m-%d %H:%M:%S",&date);
  result = db_set_interval_value( &date, "inv", "ser", 6.1, 13.2 );
  assert( result == 1 );
  printf("db_set_interval_value: %i %s\n", result, sunrise );
  
  date.tm_min=0;
  strftime(sunrise,DATE_STR_LENGTH,"%Y-%m-%d %H:%M:%S",&date);
  result = db_set_interval_value( &date, "inv", "ser", 3.14, 13.75 );
  assert( result == 1 );
  printf("db_set_interval_value: %i %s\n", result, sunrise );
  
  
  
  date.tm_hour = 0;
  struct tm last_date = db_get_last_recorded_interval_datetime( &date );
  strftime(sunrise,DATE_STR_LENGTH,"%Y-%m-%d %H:%M:%S",&last_date);
  printf("db_get_last_recorded_interval_datetime: %s\n", sunrise );
  date = curr_date;
  date.tm_min = 5;
  date.tm_sec = 0;
  assert( date.tm_yday == last_date.tm_yday );
  assert( date.tm_hour == last_date.tm_hour );
  assert( date.tm_min == last_date.tm_min );
  assert( date.tm_sec == last_date.tm_sec );
  
  db_close();

  return 0;
}