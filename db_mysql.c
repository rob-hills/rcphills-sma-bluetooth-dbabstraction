/* mysql database interface for smatool

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
#include <mysql/mysql.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

char mysql_server[255];
char mysql_user[255];
char mysql_password[255];
char mysql_database[255];

MYSQL *dbHandle = NULL;

const int MYSQL_OK = 0;
const int MYSQL_ERROR = 1;


struct mysql_row_handle {
 MYSQL_RES *result;
 MYSQL_ROW row;
};


int mysql_open( void )
{
  //already open?
  if( dbHandle ) return MYSQL_OK;
  dbHandle = mysql_init(NULL);
  if( NULL == mysql_real_connect( dbHandle, mysql_server, mysql_user, mysql_password, mysql_database, 0, NULL, 0))
  {
      fprintf(stderr, "Error opening mysql db %s:%s\n", mysql_database, mysql_error(dbHandle) );
      dbHandle = NULL;
  }
  return ( dbHandle == NULL ? MYSQL_ERROR : MYSQL_OK );
}




/* Configure database parameters. May or may not connect to the database at this time */
void db_init(char *server, char *user, char *password, char *database)
{
  strcpy( mysql_server, server );
  strcpy( mysql_user , user );
  strcpy( mysql_password, password );
  strcpy( mysql_database, database );
}


/* Release memory used to store results and close connection */  
void db_close()
{
  mysql_close( dbHandle );
  dbHandle = NULL;
}



int db_install_tables( void )
{
  if( mysql_open() != MYSQL_OK )
  {
    fprintf(stderr, "Cannot install tables\n" );
    return -1;
  }
  const char *query_almanac = "CREATE TABLE Almanac( \
    Date DATE PRIMARY KEY, \
    Sunrise TIME, \
    Sunset TIME, \
    Changetime DATETIME );";

  int result = mysql_query( dbHandle, query_almanac);
  if( result != MYSQL_OK )
  {
    fprintf(stderr, "%s\n", mysql_error(dbHandle) );
    return -1;
  }

  const char *query_daydata= "CREATE TABLE DayData( DateTime DATETIME NOT NULL, \
    Inverter varchar(10) NOT NULL, \
    Serial varchar(40) NOT NULL, \
    CurrentPower int NULL, \
    ETotalToday decimal(10,3) NULL, \
    PVOutput datetime NULL, \
    Changetime datetime NULL, \
    PRIMARY KEY( DateTime, Inverter, Serial) );";
  result = mysql_query( dbHandle, query_daydata );
  if( result != MYSQL_OK )
  {
    fprintf(stderr, "%s\n", mysql_error(dbHandle) );
    return -1;
  }

  const char *query_settings= "CREATE TABLE Settings( \
    Value varchar(128) NOT NULL PRIMARY KEY, \
    Data varchar(500) NOT NULL );";
  result = mysql_query( dbHandle, query_settings );
  if( result != MYSQL_OK )
  {
    fprintf(stderr, "%s\n", mysql_error(dbHandle) );
    return -1;
  }

  const char *set_schema= "INSERT INTO Settings(Value,Data) VALUES('Schema',2);";
  result = mysql_query( dbHandle, set_schema );
  if( result != MYSQL_OK )
  {
    fprintf(stderr, "%s\n", mysql_error( dbHandle) );
    return -1;
  }
  return 1;
}
/*
 * returns the integer value of the schema defined in the database
 */
int db_get_schema(){
  if( mysql_open() != MYSQL_OK )
  {
    fprintf(stderr, "db_get_schema error\n" );
    return -1;
  }
  
  int schema = 0;
  int result = mysql_query( dbHandle, "SELECT Data FROM Settings WHERE Value='Schema';" );
  if( result == MYSQL_OK )
  {
    MYSQL_RES *dbResult = mysql_store_result( dbHandle );
    MYSQL_ROW row = mysql_fetch_row( dbResult );
    if( row != NULL && row[0] != NULL ) 
    {
	schema = atoi(row[0]);
    }
    mysql_free_result( dbResult );
  }
  
  return schema;
}


/*
 * Fetch the sunrise and sunset values for date
 */
int db_fetch_almanac(struct tm *date, char * sunrise, char * sunset )
{
  int retval = -1;
  if( mysql_open() != MYSQL_OK )
  {
    fprintf(stderr, "db_fetch_almanac error\n" );
    return retval;
  }
  const char *stmtText = "SELECT Sunrise, Sunset FROM Almanac WHERE Date='%s'";
  char query[100];

  char chardate[25];
  strftime(chardate, 25, "%Y-%m-%d", date );

  sprintf( query, stmtText, chardate );
#ifdef DEBUG
  puts(query);
#endif
  int result = mysql_query( dbHandle, query );
  if( result != MYSQL_OK )
  {
    fprintf(stderr, "db_fetch_almanac error: %s\n", mysql_error( dbHandle) );
    return retval;
  }

  retval = 0;
  MYSQL_RES *dbResult = mysql_store_result( dbHandle );
  MYSQL_ROW row = mysql_fetch_row( dbResult );
  if( row != NULL )
  {
    strcpy( sunrise, row[0] );
    strcpy( sunset, row[1] );
    retval = 1;
  }
  mysql_free_result( dbResult );
  
  return retval;
  
}


/* inserts the sunrise/set values for today's date */
int db_update_almanac(struct tm *date, const char * sunrise, const char * sunset )
{
  if( mysql_open() != MYSQL_OK )
  {
    fprintf(stderr, "db_update_almanac error\n" );
    return 0;
  }
  const char *stmtText = "REPLACE INTO Almanac(Date,Sunrise,Sunset) VALUES('%s','%s','%s')";
  char query[200];
  char chardate[25];
  strftime(chardate,25,"%Y-%m-%d", date);

  sprintf(query, stmtText, chardate, sunrise, sunset );
#ifdef DEBUG
  puts(query);
#endif

  int result = mysql_query( dbHandle, query );
  if( result != MYSQL_OK )
  {
    fprintf(stderr, "db_update_almanac error: %s\n", mysql_error( dbHandle) );
    return 0;
  }
  
  return 1;  
} 

/*
 * get the last recorded interval datetime for the specified date
 */
struct tm db_get_last_recorded_interval_datetime(struct tm *date)
{
  struct tm last_time;
  time_t gmt_zero = 0;
  last_time = *(gmtime( &gmt_zero  ));

  if( mysql_open() != MYSQL_OK )
  {
    fprintf(stderr, "db_get_last_recorded_interval_datetime error\n" );
    return last_time;
  }
	
  const char *stmtText = "SELECT MAX(DateTime) FROM DayData";
  char query[200];

  sprintf( query, stmtText);

#ifdef DEBUG
  puts(query);
#endif

  int result = mysql_query( dbHandle, query );
  if( result != MYSQL_OK )
  {
    fprintf(stderr, "db_get_last_recorded_interval_datetime error: %s\n", mysql_error( dbHandle) );
    return last_time;
  }

  MYSQL_RES *dbResult = mysql_store_result( dbHandle );
  MYSQL_ROW row = mysql_fetch_row( dbResult );
  if( row != NULL && row[0] != NULL )
  {
    strptime( row[0], "%Y-%m-%d %H:%M:%S", &last_time );
  }
  mysql_free_result( dbResult );

  return last_time;  
}

/* insert or update a single row in the database 
  Return 1 on success, 0 on failure
*/
int db_set_interval_value( struct tm *date, char *inverter, long unsigned int serial, long current_power, long total_energy )
{
  if( mysql_open() != MYSQL_OK )
  {
    fprintf(stderr, "db_set_interval_value error\n" );
    return 0;
  }
  const char *stmtText= "REPLACE INTO DayData(DateTime, Inverter, Serial, CurrentPower, ETotalToday, Changetime) VALUES( '%s', '%s', '%lu', %ld, %d.%03d, NOW() )";
  char query[200];
  char interval_datetime[25];
  strftime(interval_datetime,25,"%Y-%m-%d %H:%M:%S", date);

  sprintf( query, stmtText, interval_datetime, inverter, serial, current_power , total_energy / 1000, total_energy % 1000 );
#ifdef DEBUG
  puts(query);
#endif
  int result = mysql_query( dbHandle, query);
  if( result != MYSQL_OK )
  {
    fprintf(stderr, "db_set_interval_value error: %s\n", mysql_error( dbHandle) );
    return 0;
  }

  return 1;  
  
}


/*
 * Get the start of day ETotalEnergy value for the specified day
 * Returns 0.0f if there is no data.
 */
long db_get_start_of_day_energy_value( struct tm *day )
{
  if( mysql_open() != MYSQL_OK )
  {
    fprintf(stderr, "db_get_start_of_day_energy_value error\n" );
    return 0;
  }
  long start_day_e = 0;
  const char *stmtText = "SELECT ETotalToday*1000 FROM DayData WHERE DateTime >= '%s' AND DateTime < ADDDATE('%s',1) ORDER BY DateTime ASC";
  char query[200];
  char date[25];
  strftime(date,25,"%Y-%m-%d", day);

  sprintf( query, stmtText, date, date );
#ifdef DEBUG
  puts(query);
#endif
  
  int result = mysql_query( dbHandle, query);
  if( result != MYSQL_OK )
  {
    fprintf(stderr, "db_get_start_of_day_energy_value error: %s\n", mysql_error( dbHandle) );
    return 0;
  }
  
  MYSQL_RES *dbResult = mysql_store_result( dbHandle );
  MYSQL_ROW row = mysql_fetch_row( dbResult );
  if( row != NULL && row[0] != NULL)
  {
    start_day_e = atol( row[0]  );
  }
  mysql_free_result( dbResult );
  
  return start_day_e;  
}


/*
 * Set the upload date/time to NOW on the intervals between from_datetime and to_datetime inclusive
 * Return 1 for success, 0 for failure
 */
int db_set_data_posted(struct tm *from_datetime, struct tm *to_datetime )
{
  if( mysql_open() != MYSQL_OK )
  {
    fprintf(stderr, "db_set_data_posted error\n" );
    return 0;
  }
  
  const char *stmtText = "UPDATE DayData SET PVOutput=NOW() WHERE DateTime >= '%s' AND DateTime <= '%s' ";
  char query[200];
  char charfromdate[25];
  char chartodate[25];
  strftime(charfromdate,25,"%Y-%m-%d %H:%M:%S", from_datetime);
  strftime(chartodate,25,"%Y-%m-%d %H:%M:%S", to_datetime);

  sprintf( query, stmtText, charfromdate, chartodate );
#ifdef DEBUG
  puts(query);
#endif
  
  int result = mysql_query( dbHandle, query);
  if( result != MYSQL_OK )
  {
    fprintf(stderr, "db_set_data_posted error: %s\n", mysql_error( dbHandle) );
    return 0;
  }

  return 1;  
  
}



/*
 * Return an opaque row handle pointer that can be iterated over to get the values for the specified day
 * Column ID 0 = interval datetime
 * Column ID 1 = ETotalToday
 * Rows are in interval datetime order, ascending
 * Call db_row_get_column_value() and db_row_next() to get values and iterate, respectively.
 * Call db_row_handle_free() when done.
 */
row_handle* db_get_unposted_data( struct tm *from_datetime )
{
  if( mysql_open() != MYSQL_OK )
  {
    fprintf(stderr, "db_get_unposted_data error\n" );
    return NULL;
  }

  const char *stmtText = "SELECT Datetime, ETotalToday FROM DayData WHERE DateTime >= '%s' AND PVOutput IS NULL ORDER BY Datetime ASC ";
  char query[200];
  char charfromdate[25];
  strftime(charfromdate,25,"%Y-%m-%d %H:%M:%S", from_datetime);

  sprintf( query, stmtText, charfromdate );
#ifdef DEBUG
  puts(query);
#endif

  int result = mysql_query( dbHandle, query );
  if( result != MYSQL_OK )
  {
    fprintf(stderr, "db_get_unposted_data error: %s\n", mysql_error( dbHandle) );
    return NULL;
  }
  

  if( result == MYSQL_OK )
  {
    struct mysql_row_handle *handle = malloc( sizeof( struct mysql_row_handle ));
    handle->result = mysql_store_result( dbHandle );
    handle->row = mysql_fetch_row( handle->result );
    if( handle->row == NULL )
    {
	mysql_free_result( handle->result );
	free( handle );
	return NULL;
    }
    return ((row_handle*) handle);
  }
  return NULL;
}

char* db_row_string_data( row_handle *row, int column_id )
{
  return ((struct mysql_row_handle*) row)->row[column_id];
}

long db_row_int_data( row_handle *row, int column_id )
{
  return ((struct mysql_row_handle*) row)->row[column_id];
}

struct tm db_row_datetime_data( row_handle *row, int column_id )
{
  char *stringdate;
  stringdate = db_row_string_data( row, column_id );
  struct tm date;
  strptime( stringdate, "%Y-%m-%d %H:%M:%S", &date );
  return date;
}
/*
 * move to next row.
 * returns 1 on success, 0 on no more rows
 * returns a negative number representing an error code on failure.
 */
int db_row_next( row_handle *row )
{
  struct mysql_row_handle* handle = (struct mysql_row_handle*)row;

  handle->row =  mysql_fetch_row( handle->result );
  return (handle->row == NULL) ? 0 : 1;
}

/*
 * frees the row_handle
 */
void db_row_handle_free( row_handle *row )
{
  struct mysql_row_handle* handle = (struct mysql_row_handle*) row;
  mysql_free_result( handle->result );
  free( handle );
}
