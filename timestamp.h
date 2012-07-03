/* tool to read power production data for SMA solar power convertors
   Copyright flonatel GmbH & Co. KG, 2012

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

#ifndef TIMESTAMP_H
#define TIMESTAMP_H

#include "pvlogger.h"
#include <sys/time.h>

struct timestamp_struct
{
        struct timeval tv;
};

typedef struct timestamp_struct timestamp_t;
typedef timestamp_t * timestamp_p;

/* Constructs a new timestamp and sets it to the current time.*/
timestamp_p time_stamp_constructor();
/* Destroys a timestamp. */
void timestamp_destuctor(timestamp_p self);
/* Sets the given timestamp to the current time.*/
void timestamp_set_current_time(timestamp_p self);
/* Computes the difference of the given timestamp to the current
 * time and sets the given timestamp appropriate.
 */
void timestamp_duration_since(timestamp_p ts);
/* Returns the timestamp as a double (as used to be in python). */
double timestamp_as_double(timestamp_p self);

#endif


