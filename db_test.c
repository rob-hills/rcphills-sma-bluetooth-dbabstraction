/* database interface test program for smatool

   Copyright Tony Brown 2011 

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>. */

#define _XOPEN_SOURCE

#include "db_interface.h"
#include <stdio.h>
#include <time.h>
#include <assert.h>
#include <string.h>

#define DATE_STR_LENGTH 25

int main( int argc, char** argv )
{
  if( argc < 5 )
  {
    puts("db_test server user password database\nFor sqlite3, database is filename of data file. Other params ignored\n");
    return 1;
  }
  
  db_init(argv[1],argv[2],argv[3],argv[4]);
  int result = db_install_tables();
  printf("db_install_tables: %i\n", result );
 
  //assert( result == 1 );
  
  result = db_get_schema();
  printf("db_get_schema: %i\n", result );

  assert( result == 2 );
  
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
  
  date.tm_hour = 0;
  row_handle *row = db_get_unposted_data( &date );
  assert( row != NULL );
  
  struct tm from_date, to_date;
  
  puts( db_row_string_data( row, 0 ) ) ;
  strptime(  db_row_string_data( row, 0 ) ,"%Y-%m-%d %H:%M:%S", &from_date);
  
  puts( db_row_string_data( row, 1 ) ) ;
  assert( db_row_next( row ) == 1 );
  puts( db_row_string_data( row, 0 ) ) ;

  strptime(  db_row_string_data( row, 0 ) ,"%Y-%m-%d %H:%M:%S", &to_date);

  puts( db_row_string_data( row, 1 ) ) ;
  assert( db_row_next( row ) == 0 );
  db_row_handle_free( row );
  
  
  result = db_set_data_posted( &from_date, &to_date );
  assert( result == 1 );
  
  row = db_get_unposted_data( &date );
  assert( row == NULL );
  if( row != NULL ) db_row_handle_free( row );
  
  
  
  db_close();

  return 0;
}