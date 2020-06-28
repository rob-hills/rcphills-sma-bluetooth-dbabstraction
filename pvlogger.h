/* tool to read power production data for SMA solar power convertors
   Copyright Wim Hofman 2010
   Copyright Stephen Collier 2010,2011
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

#ifndef PVLOGGER_HH
#define PVLOGGER_HH

#define _XOPEN_SOURCE
/* #define _BSD_SOURCE */
#define _DEFAULT_SOURCE
#include <time.h>

char * sunset( float latitude, float longitude );
char * sunrise( float latitude, float longitude );

/* bluetooth.c */

int read_bluetooth(time_t const bt_timeout, int const sfd, int *rr,
                unsigned char *received, int cc, unsigned char *last_sent,
                int *terminated );
void fix_length_received(unsigned char *received, int *len);

#endif

