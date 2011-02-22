#ifndef __DB_INTERFACE_H__
#define __DB_INTERFACE_H__
/* database interface for smatool

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


//definition of struct tm
#include <time.h>

/* The opaque row handle object */
typedef void* row_handle;


/* Configure database parameters. May or may not connect to the database at this time */
void db_init(char *server, char *user, char *password, char *database);


/* Release memory used to store results and close connection */  
void db_close();



/*  called from --initial to setup database schema. Returns 0 if db setup this call, 1 if db already existed */
int db_install_tables( void );


/*
 * returns the integer value of the schema defined in the database
 */
int db_get_schema();


/*  Get the sunrise and sunset times for the specified day
 * Returns 1 on success, 0 on failure ( no matching row )
 */
int db_fetch_almanac(struct tm *date, char * sunrise, char * sunset );


/* inserts the sunrise/set values for today's date */
int db_update_almanac(struct tm *date, const char * sunrise, const char * sunset );


/*
 * get the last recorded interval datetime for the specified date
 */
struct tm db_get_last_recorded_interval_datetime(struct tm *date);


//int is_light( ConfType * conf );
/*  Check if all data done and past sunset or before sunrise */
//logic in here is a bit hard to understand...
// if current datetime is before sunrise, exit(0)
// otherwise, if there is a recorded value after sunset today, exit(0)
// else exit(1)
// ...effectively - return 1 if there's data to collect


/* insert or update a single row in the database 
  Return 1 on success, 0 on failure
  TODO: current_power and total_energy should be scaled integer values (decimal(10,3) type )
*/
int db_set_interval_value( struct tm *date, char *inverter, char *serial, float current_power, float total_energy );

/*
 * Get the start of day ETotalEnergy value for the specified day
 * Returns 0.0f if there is no data.
 */
float db_get_start_of_day_energy_value( struct tm *day );

/*
 * Set the upload date/time to NOW on the intervals between from_datetime and to_datetime inclusive
 * Return 1 for success, 0 for failure
 */
int db_set_data_posted(struct tm *from_datetime, struct tm *to_datetime );


/*
 * Return an opaque row handle pointer that can be iterated over to get unposted values from the specified datetime
 * Column ID 0 = interval datetime
 * Column ID 1 = ETotalToday
 * Rows are in interval datetime order, ascending
 * Call db_row_string_data() and db_row_next() to get values and iterate, respectively.
 * Call db_row_handle_free() when done.
 * Returns NULL if there are no rows returned
 */
row_handle* db_get_unposted_data( struct tm *from_datetime );

/*
 * Get a row's column value as a char* value
 * TODO: do we need other data types? ( struct tm, int ? )
 */
char* db_row_string_data( row_handle *row, int column_id );

/*
 * move to next row.
 * returns 1 on success, 0 on failure (no more rows)
 */
int db_row_next( row_handle *row );

/*
 * frees the row_handle
 */
void db_row_handle_free( row_handle *row );


 
#endif