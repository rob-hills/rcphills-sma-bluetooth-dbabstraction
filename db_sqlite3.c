/* sqlite3 database interface for smatool

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
#include <sqlite3.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>


char sqlite_dbfile[255];

sqlite3 *dbHandle = NULL;

int sqlite_open( void )
{
  //already open?
  if( dbHandle ) return SQLITE_OK;
  
  int result = sqlite3_open_v2(sqlite_dbfile, &dbHandle, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, NULL );
  if( result  != SQLITE_OK )
  {
      fprintf(stderr, "Error opening sqlite3 db %s:%s\n", sqlite_dbfile, sqlite3_errmsg(dbHandle) );
      dbHandle = NULL;
  }
  return result;
}




/* Configure database parameters. May or may not connect to the database at this time */
void db_init(char *server, char *user, char *password, char *database)
{
  strcpy( sqlite_dbfile, database );
}


/* Release memory used to store results and close connection */  
void db_close()
{
  int result = sqlite3_close( dbHandle );
  if( result  != SQLITE_OK  ) 
  {
    fprintf(stderr, "Error closing sqlite3 db %s:%s\n", sqlite_dbfile, sqlite3_errmsg(dbHandle) );
  }
  else dbHandle = NULL;
}



int db_install_tables( void )
{
  if( sqlite_open() != SQLITE_OK )
  {
    fprintf(stderr, "Cannot install tables\n" );
    return -1;
  }
  const char *query_almanac = "CREATE TABLE Almanac( \
    Date DATE PRIMARY KEY, \
    Sunrise DATETIME, \
    Sunset DATETIME, \
    Changetime DATETIME );";
  char *error = NULL;
  int result = sqlite3_exec( dbHandle, query_almanac, NULL, NULL, &error );
  if( error )
  {
    fprintf(stderr, "%s\n", error );
    sqlite3_free( error );
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
  result = sqlite3_exec( dbHandle, query_daydata, NULL, NULL, &error );
  if( error )
  {
    fprintf(stderr, "%s\n", error );
    sqlite3_free( error );
    return -1;
  }

  const char *query_settings= "CREATE TABLE Settings( \
    Value varchar(128) NOT NULL PRIMARY KEY, \
    Data varchar(500) NOT NULL );";
  result = sqlite3_exec( dbHandle, query_settings, NULL, NULL, &error );
  if( error )
  {
    fprintf(stderr, "%s\n", error );
    sqlite3_free( error );
    return -1;
  }

  const char *set_schema= "INSERT INTO Settings(Value,Data) VALUES('Schema',2);";
  result = sqlite3_exec( dbHandle, set_schema, NULL, NULL, &error );
  if( error )
  {
    fprintf(stderr, "%s\n", error );
    sqlite3_free( error );
    return -1;
  }
  return 1;
}
/*
 * returns the integer value of the schema defined in the database
 */
int db_get_schema(){
  if( sqlite_open() != SQLITE_OK )
  {
    fprintf(stderr, "db_get_schema error\n" );
    return -1;
  }
  
  sqlite3_stmt *pStmt = NULL;
  int schema = 0;
  int result = sqlite3_prepare_v2( dbHandle, "SELECT Data FROM Settings WHERE Value='Schema';", -1, &pStmt, NULL );
  if( pStmt != NULL )
  {
    result = sqlite3_step( pStmt );
    if( result == SQLITE_ROW )
    {
      schema = sqlite3_column_int( pStmt, 0 );
    }
    sqlite3_finalize( pStmt );
  }
  
  return schema;
}


/*
 * Fetch the sunrise and sunset values for date
 */
int db_fetch_almanac(struct tm *date, char * sunrise, char * sunset )
{
  int retval = -1;
  if( sqlite_open() != SQLITE_OK )
  {
    fprintf(stderr, "db_fetch_almanac error\n" );
    return retval;
  }
  
  sqlite3_stmt *pStmt = NULL;
  int result = sqlite3_prepare_v2( dbHandle, "SELECT Sunrise, Sunset FROM Almanac WHERE Date=?;", -1, &pStmt, NULL );
  if( NULL == pStmt )
  {
    fprintf(stderr, "db_fetch_almanac error: %s\n", sqlite3_errmsg( dbHandle) );
    return retval;
  }
  char chardate[25];
  strftime(chardate,25,"%Y-%m-%d", date);
  sqlite3_bind_text( pStmt, 1, chardate, -1, SQLITE_STATIC );
  result = sqlite3_step( pStmt );
  retval = 0;
  if( result == SQLITE_ROW )
  {
    strcpy( sunrise, (char*)sqlite3_column_text( pStmt, 0 ));
    strcpy( sunset, (char*)sqlite3_column_text( pStmt, 1 ));
    retval = 1;
  }
  sqlite3_finalize( pStmt );
  
  return retval;
  
}


/* inserts the sunrise/set values for today's date */
int db_update_almanac(struct tm *date, const char * sunrise, const char * sunset )
{
  if( sqlite_open() != SQLITE_OK )
  {
    fprintf(stderr, "db_update_almanac error\n" );
    return 0;
  }
  
  sqlite3_stmt *pStmt = NULL;
  int result = sqlite3_prepare_v2( dbHandle, "REPLACE INTO Almanac(Date,Sunrise,Sunset) VALUES(?,?,?);", -1, &pStmt, NULL );
  if( NULL == pStmt )
  {
    fprintf(stderr, "db_update_almanac error: %s\n", sqlite3_errmsg( dbHandle) );
    return 0;
  }
  
  char chardate[25];
  strftime(chardate,25,"%Y-%m-%d", date);
  sqlite3_bind_text( pStmt, 1, chardate, -1, SQLITE_STATIC );
  sqlite3_bind_text( pStmt, 2, sunrise , -1, SQLITE_STATIC );
  sqlite3_bind_text( pStmt, 3, sunset  , -1, SQLITE_STATIC);
  result = sqlite3_step( pStmt );
  if( result == SQLITE_DONE )
  {
    sqlite3_finalize( pStmt );
    return 1;
  }

  fprintf(stderr, "db_update_almanac error: %s\n", sqlite3_errmsg( dbHandle) );
  sqlite3_finalize( pStmt );
  return 0;  
} 

/*
 * get the last recorded interval datetime for the specified date
 */
struct tm db_get_last_recorded_interval_datetime(struct tm *date)
{
  struct tm last_time;
  time_t gmt_zero = 0;
  last_time = *(gmtime( &gmt_zero  ));

  if( sqlite_open() != SQLITE_OK )
  {
    fprintf(stderr, "db_get_last_recorded_interval_datetime error\n" );
    return last_time;
  }
  
  sqlite3_stmt *pStmt = NULL;
  int result = sqlite3_prepare_v2( dbHandle, "SELECT MAX(DateTime) FROM DayData WHERE DateTime < date(?,'1 day') ;", -1, &pStmt, NULL );
  if( NULL == pStmt )
  {
    fprintf(stderr, "db_get_last_recorded_interval_datetime error: %s\n", sqlite3_errmsg( dbHandle) );
    return last_time;
  }
  char chardate[25];
  strftime(chardate,25,"%Y-%m-%d", date);
  sqlite3_bind_text( pStmt, 1, chardate, -1, SQLITE_STATIC );
 // sqlite3_bind_text( pStmt, 2, chardate, -1, SQLITE_STATIC );

  result = sqlite3_step( pStmt );
  if( result == SQLITE_ROW )
  {
    char *val = (char*) sqlite3_column_text(pStmt, 0);
    if( val == NULL )
    {
      last_time = *date;
    }
    else
    {
      strptime(val , "%Y-%m-%d %H:%M:%S", &last_time );
    }
  }
   
  sqlite3_finalize( pStmt );
  return last_time;  
}

/* insert or update a single row in the database 
  Return 1 on success, 0 on failure
*/
int db_set_interval_value( struct tm *date, char *inverter, long unsigned int serial, long current_power, long total_energy )
{
  if( sqlite_open() != SQLITE_OK )
  {
    fprintf(stderr, "db_set_interval_value error\n" );
    return 0;
  }
  
  sqlite3_stmt *pStmt = NULL;
  int result = sqlite3_prepare_v2( dbHandle, "REPLACE INTO DayData(DateTime, Inverter, Serial, CurrentPower, ETotalToday, Changetime) VALUES( ?, ?, ?, ?, ? /1000.0, datetime('now','localtime') );", -1, &pStmt, NULL );
  if( NULL == pStmt )
  {
    fprintf(stderr, "db_set_interval_value error: %s\n", sqlite3_errmsg( dbHandle) );
    return 0;
  }
  char interval_datetime[25];
  strftime(interval_datetime,25,"%Y-%m-%d %H:%M:%S", date);
  sqlite3_bind_text( pStmt, 1, interval_datetime, -1, SQLITE_STATIC );
  sqlite3_bind_text( pStmt, 2, inverter , -1, SQLITE_STATIC);
  sqlite3_bind_int( pStmt, 3, serial);
  sqlite3_bind_int( pStmt, 4, current_power );
  sqlite3_bind_int( pStmt, 5, total_energy );
  result = sqlite3_step( pStmt );
  if( result == SQLITE_DONE )
  {
    sqlite3_finalize( pStmt );
    return 1;
  }

  fprintf(stderr, "db_set_interval_value error: %s\n", sqlite3_errmsg( dbHandle) );
  sqlite3_finalize( pStmt );
  return 0;  
  
}


/*
 * Get the start of day ETotalEnergy value for the specified day
 * Returns 0.0f if there is no data.
 */
long db_get_start_of_day_energy_value( struct tm *day )
{
  if( sqlite_open() != SQLITE_OK )
  {
    fprintf(stderr, "db_get_start_of_day_energy_value error\n" );
    return 0;
  }
  long start_day_e = 0;
  
  sqlite3_stmt *pStmt = NULL;
  int result = sqlite3_prepare_v2( dbHandle, "SELECT ETotalToday*1000 FROM DayData WHERE DateTime >= ? AND DateTime < date(?,'1 day') ORDER BY DateTime ASC;", -1, &pStmt, NULL );
  if( NULL == pStmt )
  {
    fprintf(stderr, "db_get_start_of_day_energy_value error: %s\n", sqlite3_errmsg( dbHandle) );
    return 0;
  }
  
  char charfromdate[25];
  strftime(charfromdate,25,"%Y-%m-%d", day);
  sqlite3_bind_text( pStmt, 1, charfromdate, -1, SQLITE_STATIC );
  sqlite3_bind_text( pStmt, 2, charfromdate, -1, SQLITE_STATIC );

  result = sqlite3_step( pStmt );
  if( result == SQLITE_ROW )
  {
    start_day_e = sqlite3_column_int( pStmt, 0 );
  }
  sqlite3_finalize( pStmt );
  return start_day_e;  
}


/*
 * Set the upload date/time to NOW on the intervals between from_datetime and to_datetime inclusive
 * Return 1 for success, 0 for failure
 */
int db_set_data_posted(struct tm *from_datetime, struct tm *to_datetime )
{
  int retval = 0;
  if( sqlite_open() != SQLITE_OK )
  {
    fprintf(stderr, "db_set_data_posted error\n" );
    return 0;
  }
  
  sqlite3_stmt *pStmt = NULL;
  int result = sqlite3_prepare_v2( dbHandle, "UPDATE DayData SET PVOutput=datetime('now','localtime') WHERE DateTime >= ? AND DateTime <= ? ;", -1, &pStmt, NULL );
  if( NULL == pStmt )
  {
    fprintf(stderr, "db_set_data_posted error: %s\n", sqlite3_errmsg( dbHandle) );
    return 0;
  }
  char charfromdate[25];
  char chartodate[25];
  strftime(charfromdate,25,"%Y-%m-%d %H:%M:%S", from_datetime);
  strftime(chartodate,25,"%Y-%m-%d %H:%M:%S", to_datetime);
  sqlite3_bind_text( pStmt, 1, charfromdate, -1, SQLITE_STATIC );
  sqlite3_bind_text( pStmt, 2, chartodate, -1, SQLITE_STATIC );

  result = sqlite3_step( pStmt );
  if( result == SQLITE_DONE )
  {
    retval = 1;
  }
   
  sqlite3_finalize( pStmt );
  return retval;  
  
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
  if( sqlite_open() != SQLITE_OK )
  {
    fprintf(stderr, "db_get_unposted_data error\n" );
    return NULL;
  }
  
  sqlite3_stmt *pStmt = NULL;
  int result = sqlite3_prepare_v2( dbHandle, "SELECT Datetime, strftime('%Y%m%d',Datetime),strftime('%H:%M',Datetime), ETotalToday*1000, CurrentPower FROM DayData WHERE DateTime >= ? AND PVOutput IS NULL AND CurrentPower > 0 ORDER BY Datetime ASC ;", -1, &pStmt, NULL );
  if( NULL == pStmt )
  {
    fprintf(stderr, "db_get_unposted_data error: %s\n", sqlite3_errmsg( dbHandle) );
    return NULL;
  }
  char charfromdate[25];
  strftime(charfromdate,25,"%Y-%m-%d %H:%M:%S", from_datetime);
  sqlite3_bind_text( pStmt, 1, charfromdate, -1, SQLITE_STATIC );
  
  result = sqlite3_step( pStmt );

  if( result == SQLITE_ROW )
  {
    return (row_handle*) pStmt;
  }
  sqlite3_finalize( pStmt );
  return NULL;
}

/*
 *************** TODO ******************
 * NEED A NEW db_get_data function based on the above that gets data between a from_datetime and a to_datetime irrespective of the
 * PVOutput field status (for reposting old data).
 */


char* db_row_string_data( row_handle *row, int column_id )
{
  return (char*) sqlite3_column_text( (sqlite3_stmt*)row, column_id);
}

struct tm db_row_datetime_data( row_handle *row, int column_id )
{
  char *stringdate;
  stringdate = db_row_string_data( row, column_id );
// printf("\nstring date: %s\n",stringdate );
  struct tm date;
  strptime( stringdate, "%Y-%m-%d %H:%M:%S", &date );
  return date;
}
long db_row_int_data( row_handle *row, int column_id )
{
  return sqlite3_column_int( (sqlite3_stmt*)row, column_id);
}
/*
 * move to next row.
 * returns 1 on success, 0 on no more rows
 * returns a negative number representing an error code on failure.
 */
int db_row_next( row_handle *row )
{
  int result =  sqlite3_step( (sqlite3_stmt*)row );
  if( result == SQLITE_ROW ) 
  {
    return 1;
  }
  else if( result == SQLITE_DONE )
  {
    return 0;
  }
  return -result;
}

/*
 * frees the row_handle
 */
void db_row_handle_free( row_handle *row )
{
  sqlite3_finalize( (sqlite3_stmt*)row );
}
