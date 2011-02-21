#ifndef __DB_INTERFACE_H__
#define __DB_INTERFACE_H__


//definition of struct tm
#include <time.h>

typedef void* row_handle;


/* Configure database parameters. May or may not connect to the database at this time */
void db_init(char *server, char *user, char *password, char *database);


/* Release memory used to store results and close connection */  
void db_close();



/*  called from --initial to setup database schema. Returns 0 if db setup this call, 1 if db already existed */
//int install_mysql_tables( ConfType * conf );
int db_install_tables( void );


/*  Check if using the correct database schema - returns 1 if database matches constant SCHEMA */
//int check_schema( ConfType * conf );
/*
 * returns the integer value of the schema defined in the database
 */
int db_get_schema();


/*  Check if sunset and sunrise have been set today - returns 1 if row for today is found */
//could possibly return sunrise/sunset values from this function??
//int todays_almanac( ConfType *conf );
int db_fetch_almanac(struct tm *date, char * sunrise, char * sunset );


/* inserts the sunrise/set values for today's date */
//void update_almanac( ConfType *conf, char * sunrise, char * sunset );
int db_update_almanac(struct tm *date, const char * sunrise, const char * sunset );



/*  If there are no dates set - get last updated date and go from there to NOW */
// Maybe just getLastRecordedDate function required to replace some of this function??
//int auto_set_dates( ConfType * conf, int * daterange, int mysql, char * datefrom, char * dateto );

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
*/
int db_set_interval_value( struct tm *date, char *inverter, char *serial, float current_power, float total_energy );
/*
in main()
//insert/update day values
            sprintf(SQLQUERY,"INSERT INTO DayData ( DateTime, Inverter, Serial, CurrentPower, EtotalToday ) VALUES ( FROM_UNIXTIME(%ld),\'%s\',%ld,%0.f, %.3f ) ON DUPLICATE KEY UPDATE DateTime=Datetime, Inverter=VALUES(Inverter), Serial=VALUES(Serial), CurrentPower=VALUES(CurrentPower), EtotalToday=VALUES(EtotalToday)",(archdatalist+i)->date, (archdatalist+i)->inverter, (archdatalist+i)->serial, (archdatalist+i)->current_value, (archdatalist+i)->accum_value );
            if (debug == 1) printf("%s\n",SQLQUERY);
            DoQuery(SQLQUERY);
*/

/*
 * Get the start of day ETotalEnergy value for the specified day
 * Returns 0.0f if there is no data.
 */
float db_get_start_of_day_energy_value( struct tm *day );
/*
//posting
    //get start of day values
    sprintf(SQLQUERY,"SELECT EtotalToday FROM DayData WHERE DateTime=DATE_FORMAT( NOW(), \"%%Y%%m%%d000000\" ) " );
*/

/*
 * Return 1 if the interval specified by interval_datetime has been uploaded to PVOutput
 */
int db_is_interval_uploaded( struct tm interval_datetime );
/*
    sprintf(SQLQUERY,"SELECT PVOutput FROM DayData WHERE DateTime=\"%i%02i%02i%02i%02i%02i\"  and PVOutput IS NOT NULL", year, month, day, hour, minute, second );
*/

/*
 * Set the upload date/time to NOW on the interval identified by interval_datetime
 * Return 1 for success, 0 for failure
 */
int db_set_interval_upload_now( struct tm interval_datetime );
/*
    sprintf(SQLQUERY,"UPDATE DayData  set PVOutput=NOW() WHERE DateTime=\"%i%02i%02i%02i%02i%02i\"  ", year, month, day, hour, minute, second );

*/

/*
 * Return an opaque row handle pointer that can be iterated over to get the values for the specified day
 * Column ID 0 = interval datetime
 * Column ID 1 = ETotalToday
 * Rows are in interval datetime order, ascending
 * Call db_row_get_column_value() and db_row_next() to get values and iterate, respectively.
 * Call db_row_handle_free() when done.
 */
row_handle * db_get_day_data( struct tm day );

void db_row_get_column_value( row_handle *row, int column_id, char **value );

/*
 * move to next row.
 * returns 1 on success, 0 on failure (no more rows)
 */
int db_row_next( row_handle *row );

/*
 * frees the row_handle
 */
void db_row_handle_free( row_handle *row );


/*
//re-posting
    sprintf(SQLQUERY,"SELECT DATE_FORMAT( DateTime, \"%%Y%%m%%d\" ), ETotalToday FROM DayData WHERE DateTime LIKE \"%%-%%-%% 23:55:00\" ORDER BY DateTime ASC" );
...
    sprintf(SQLQUERY,"UPDATE DayData set PVOutput=NOW() WHERE DateTime=\"%s235500\"  ", row[0] );
    //the DoQuery() call for the above query is commentted out.

*/


 
#endif