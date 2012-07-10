#include <mysql/mysql.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


MYSQL *conn = 0;


void OpenMySqlDatabase (char *server, char *user, char *password, char *database)
{
	if(conn!=0) {
	    log_fatal("Trying to open database connection but is already open.");
	    abort();
	}

	conn = mysql_init(NULL);
   /* Connect to database */
   if (!mysql_real_connect(conn, server,
         user, password, database, 0, NULL, 0)) {
      log_fatal("%s", mysql_error(conn));
      exit(1);
   }
   log_debug("Successfully connected to database.");
}

void CloseMySqlDatabase()
{
   log_debug("Closing database connection.");
   mysql_close(conn);
   conn = 0;
}

MYSQL_RES * DoQuery (char const * const query){
	/* execute query */
	if (mysql_real_query(conn, query, strlen(query))) {
	    log_error("Error in query [%s]", mysql_error(conn));
	    exit(1);
	}
	MYSQL_RES * res = mysql_store_result(conn);
	if (res==0) {
	    log_error("Error in get result [%s]", mysql_error(conn));
	    exit(1);
	}
	return res;
}

void DoQueryNoRes (char const * const query) {
    MYSQL_RES * res = DoQuery(query);
    mysql_free_result(res);
}

