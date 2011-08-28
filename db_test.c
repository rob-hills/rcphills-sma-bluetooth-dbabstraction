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
#include "minunit.h"

#define DATE_STR_LENGTH 25

int tests_run = 0;
char *server = NULL;
char *user = NULL;
char *password = NULL;
char *database = NULL;
int do_install = 0;

const char *tst_sunrise="2011-02-22 06:35:40";
const char *tst_sunset="2011-02-22 19:27:28";
const char *tst_date="2011-02-22 15:01:39";
const char *tst_format="%Y-%m-%d %H:%M:%S";


const int power1 = 3456; //kW
const int energy1 = 13003; // scaled * 1000 kWh; 13.003kWh
const int power2 = 3000; //kW
const int energy2 = 13600; // scaled * 1000 kWh; 13.6kWh


static char * test_db_install_tables() {
    mu_assert_equal_int(1, db_install_tables() );
    return 0;
}
 
static char * test_db_get_schema() {
    mu_assert_equal_int(2, db_get_schema());
    return 0;
}

static char * test_db_update_almanac() {
  struct tm date;
  strptime( tst_date,tst_format,&date); 
  mu_assert_equal_int(1, db_update_almanac(&date, tst_sunrise, tst_sunset ));
  return 0;
}

static char * test_db_fetch_almanac() {

  char sunrise_get[DATE_STR_LENGTH];
  char sunset_get[DATE_STR_LENGTH];
  struct tm date;
  strptime( tst_date,tst_format,&date); 
  date.tm_hour  = 0;
  mu_assert_equal_int(1, db_fetch_almanac( &date, sunrise_get, sunset_get ) );
  mu_assert_equal_string(tst_sunrise, sunrise_get );
  mu_assert_equal_string(tst_sunset, sunset_get );
  
  return 0;
}


static char * test_db_fetch_almanac_not_found() {

  char sunrise_get[DATE_STR_LENGTH];
  char sunset_get[DATE_STR_LENGTH];
  sunrise_get[0] = '\0';
  sunset_get[0] = '\0';
  struct tm date;
  strptime( tst_date,tst_format,&date); 
  date.tm_year++;
  mu_assert_equal_int(0, db_fetch_almanac( &date, sunrise_get, sunset_get ) );
  mu_assert_equal_string("", sunrise_get );
  mu_assert_equal_string("", sunset_get );
  
  return 0;
}


static char * test_db_set_interval_value() {
  struct tm date;
  strptime( tst_date,tst_format,&date); 
  //later time first
  date.tm_sec = 0;
  date.tm_min = 15;
  mu_assert_equal_int( 1, db_set_interval_value( &date, "inv", 1234567890, power2, energy2 ));
  date.tm_min = 10;
  mu_assert_equal_int( 1, db_set_interval_value( &date, "inv", 1234567890, power1, energy1 ));
  return 0; 
}

static char * test_db_get_last_recorded_interval_datetime() {

  struct tm date;
  strptime( tst_date,tst_format,&date); 
  struct tm last_date = db_get_last_recorded_interval_datetime( &date );
  date.tm_min = 15;
  date.tm_sec = 0;
  
  char exp[DATE_STR_LENGTH];
  char res[DATE_STR_LENGTH];
  strftime(exp,DATE_STR_LENGTH, tst_format ,&date);
  strftime(res,DATE_STR_LENGTH, tst_format ,&last_date);
  
  mu_assert_equal_string( res, exp );

  return 0;
}

static char * test_db_get_last_recorded_interval_datetime_not_found() {

  struct tm date;
  strptime( tst_date,tst_format,&date); 
  date.tm_year++;
  struct tm last_date = db_get_last_recorded_interval_datetime( &date );
  //date should be 1970-01-01 if no record found
  mu_assert_equal_int( 70, last_date.tm_year );
  mu_assert_equal_int( 0, last_date.tm_mon );
  mu_assert_equal_int( 1, last_date.tm_mday );

  return 0;
}

static char * test_db_get_unposted_data()
{
  struct tm date;
  strptime( tst_date,tst_format,&date); 
  row_handle *row = db_get_unposted_data( &date );
  mu_assert("No rows found", row != NULL );
  
  date.tm_sec = 0;
  date.tm_min = 10;
  char exp[DATE_STR_LENGTH];
 
  strftime(exp,DATE_STR_LENGTH, tst_format ,&date);

  char *got = db_row_string_data( row, 0 );
  mu_assert_equal_string( exp, got );
  sprintf(exp,"%d.%03d", energy1 / 1000 , energy1 % 1000 );
  got = db_row_string_data( row, 1 );
  mu_assert_equal_string( exp, got );
  //next row
  mu_assert_equal_int(1, db_row_next( row )  );
  
  
  date.tm_min = 15;
  strftime(exp,DATE_STR_LENGTH, tst_format ,&date);

  got = db_row_string_data( row, 0 );
  mu_assert_equal_string( exp, got );
  sprintf(exp,"%d.%03d", energy2 / 1000 , energy2 % 1000 );
  got = db_row_string_data( row, 1 );
  //code to ensure fetched number has 3 decimal places
  char *dotPos = strchr(got,'.');
  if( dotPos != NULL )
  {    
    strcat(got,"000");
    *(dotPos+4)='\0'; 
  }
  //end ensure trailing zeros
  mu_assert_equal_string( exp, got );
  
  db_row_handle_free( row );
  return 0;
}

static char * test_db_set_data_posted(){

  struct tm from_date, to_date;
  strptime( tst_date,tst_format,&from_date);
  from_date.tm_sec = 0;
  from_date.tm_min = 10;

  to_date = from_date;
  to_date.tm_min = 15;

  int result = db_set_data_posted( &from_date, &to_date );
  mu_assert_equal_int(1, result );

  //assert there should be no rows
  row_handle *row = db_get_unposted_data( &from_date );
  mu_assert("should be no unposted data", row == NULL );

  return 0;
}

static char * all_tests() {
  if( do_install ) mu_run_test(test_db_install_tables);
  mu_run_test(test_db_get_schema);
  mu_run_test(test_db_update_almanac);
  mu_run_test(test_db_fetch_almanac);
  mu_run_test(test_db_fetch_almanac_not_found);
  mu_run_test(test_db_set_interval_value);
  mu_run_test(test_db_get_last_recorded_interval_datetime);
  mu_run_test(test_db_get_last_recorded_interval_datetime_not_found);
  mu_run_test(test_db_get_unposted_data);
  mu_run_test(test_db_set_data_posted);
  return 0;
}
 
int main(int argc, char **argv) {
  if( argc < 5 )
  {
    puts("db_test server user password database [do_install]");
    puts("For sqlite3, \"database\" is filename of data file and all other params are ignored");
    return 1;
  }
  server = argv[1];
  user = argv[2];
  password = argv[3];
  database = argv[4];
  if( argv[5] ) do_install = 1;
  
  db_init(server, user, password, database);

  char *result = all_tests();
  if (result != 0) {
      printf("Test %s failed. %s.\n", t_name, result);
  }
  else {
      printf("ALL TESTS PASSED\n");
  }
  printf("Tests run: %d\n", tests_run);
  db_close();

  return result != 0;
}

