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
#include "pvlogger.h"
#include "logging.h"

#include <time.h>
#include <math.h>
#include <stdlib.h>
#include <stdio.h>

char *  sunrise( float latitude, float longitude )
{
   //adapted from http://williams.best.vwh.net/sunrise_sunset_algorithm.htm
   time_t curtime;
   struct tm *loctime;
   struct tm *utctime;
   int day,month,year,hour,minute;
   char *returntime;

   double t,M,L,T,RA,Lquadrant,RAquadrant,sinDec,cosDec;
   double cosH, H, UT, localT,lngHour;
   float localOffset,zenith=91;
   double pi=M_PI;

   returntime = (char *)malloc(6*sizeof(char));
   curtime = time(NULL);  //get time in seconds since epoch (1/1/1970)
   loctime = localtime(&curtime);
   day = loctime->tm_mday;
   month = loctime->tm_mon +1;
   year = loctime->tm_year + 1900;
   hour = loctime->tm_hour;
   minute = loctime->tm_min;
   utctime = gmtime(&curtime);


   log_debug("utc=%04d-%02d-%02d %02d:%02d local=%04d-%02d-%02d %02d:%02d diff %d hours", utctime->tm_year+1900, utctime->tm_mon+1,utctime->tm_mday,utctime->tm_hour,utctime->tm_min, year, month, day, hour, minute, hour-utctime->tm_hour );
   localOffset=(hour-utctime->tm_hour)+(minute-utctime->tm_min)/60;
   log_debug("localOffset=%f", localOffset );
   if(( year > utctime->tm_year+1900 )||( month > utctime->tm_mon+1 )||( day > utctime->tm_mday ))
      localOffset+=24;
   if(( year < utctime->tm_year+1900 )||( month < utctime->tm_mon+1 )||( day < utctime->tm_mday ))
      localOffset-=24;
   log_debug("localOffset=%f\n", localOffset );
   lngHour = longitude / 15;
   t = loctime->tm_yday + ((6 - lngHour) / 24);
   //Calculate the Sun's mean anomaly
   M = (0.9856 * t) - 3.289;
   //Calculate the Sun's tru longitude
   L = M + (1.916 * sin((pi/180)*M)) + (0.020 * sin(2 * (pi/180)*M)) + 282.634;
   if( L > 360 ) L=L-360;
   if( L < 0 ) L=L+360;
   //calculate the Sun's right ascension
   RA = (180/pi)*atan(0.91764 * tan((pi/180)*L));
   //right ascension value needs to be in the same quadrant as L
   Lquadrant  = (floor( L/90)) * 90;
   RAquadrant = (floor(RA/90)) * 90;

   RA = RA + (Lquadrant - RAquadrant);
   //right ascension value needs to be converted into hours
   RA = RA / 15;
   //calculate the Sun's declination
   sinDec = 0.39782 * sin((pi/180)*L);
   cosDec = cos(asin(sinDec));
   //calculate the Sun's local hour angle
   cosH = (cos((pi/180)*zenith) - (sinDec * sin((pi/180)*latitude))) / (cosDec * cos((pi/180)*latitude));

   if (cosH >  1)
      printf( "Sun never rises here!\n" );
          //the sun never rises on this location (on the specified date)
   if (cosH < -1)
      printf( "Sun never sets here!\n" );
          //the sun never sets on this location (on the specified date)
   //finish calculating H and convert into hours
   H = 360 -(180/pi)*acos(cosH);
   H = H/15;
   //calculate local mean time of rising/setting
   T = H + RA - (0.06571 * t) - 6.622;
   //adjust back to UTC
   UT = T - lngHour;
   if( UT < 0 ) UT=UT+24;
   if( UT > 24 ) UT=UT-24;
   day = loctime->tm_mday;
   month = loctime->tm_mon +1;
   year = loctime->tm_year + 1900;
   hour = loctime->tm_hour;
   minute = loctime->tm_min;
   //convert UT value to local time zone of latitude/longitude
   localT = UT + localOffset;
   if( localT < 0 ) localT=localT+24;
   if( localT > 24 ) localT=localT-24;
   sprintf( returntime, "%02.0f:%02.0f",floor(localT),floor((localT-floor(localT))*60) );
   return returntime;
}

char * sunset( float latitude, float longitude )
{
   //adapted from http://williams.best.vwh.net/sunrise_sunset_algorithm.htm
   time_t curtime;
   struct tm *loctime;
   struct tm *utctime;
   int day,month,year,hour,minute;
   char *returntime;

   double t,M,L,T,RA,Lquadrant,RAquadrant,sinDec,cosDec;
   double cosH, H, UT, localT,lngHour;
   float localOffset,zenith=91;
   double pi=M_PI;

   returntime = (char *)malloc(6*sizeof(char));

   curtime = time(NULL);  //get time in seconds since epoch (1/1/1970)
   loctime = localtime(&curtime);
   day = loctime->tm_mday;
   month = loctime->tm_mon +1;
   year = loctime->tm_year + 1900;
   hour = loctime->tm_hour;
   minute = loctime->tm_min;
   utctime = gmtime(&curtime);


   localOffset=(hour-utctime->tm_hour)+(minute-utctime->tm_min)/60;
   if(( year > utctime->tm_year+1900 )||( month > utctime->tm_mon+1 )||( day > utctime->tm_mday ))
      localOffset+=24;
   if(( year < utctime->tm_year+1900 )||( month < utctime->tm_mon+1 )||( day < utctime->tm_mday ))
      localOffset-=24;

   lngHour = longitude / 15;
   t = loctime->tm_yday + ((18 - lngHour) / 24);
   //Calculate the Sun's mean anomaly
   M = (0.9856 * t) - 3.289;
   //Calculate the Sun's tru longitude
   L = M + (1.916 * sin((pi/180)*M)) + (0.020 * sin(2 * (pi/180)*M)) + 282.634;
   if( L > 360 ) L=L-360;
   if( L < 0 ) L=L+360;
   //calculate the Sun's right ascension
   RA = (180/pi)*atan(0.91764 * tan((pi/180)*L));
   //right ascension value needs to be in the same quadrant as L
   Lquadrant  = (floor( L/90)) * 90;
   RAquadrant = (floor(RA/90)) * 90;

   RA = RA + (Lquadrant - RAquadrant);
   //right ascension value needs to be converted into hours
   RA = RA / 15;
   //calculate the Sun's declination
   sinDec = 0.39782 * sin((pi/180)*L);
   cosDec = cos(asin(sinDec));
   //calculate the Sun's local hour angle
   cosH = (cos((pi/180)*zenith) - (sinDec * sin((pi/180)*latitude))) / (cosDec * cos((pi/180)*latitude));

   if (cosH >  1);
          //the sun never rises on this location (on the specified date)
   if (cosH < -1);
          //the sun never sets on this location (on the specified date)
   //finish calculating H and convert into hours
   H = (180/pi)*acos(cosH);
   H = H/15;
   //calculate local mean time of rising/setting
   T = H + RA - (0.06571 * t) - 6.622;
   //adjust back to UTC
   UT = T - lngHour;
   if( UT > 24 ) UT=UT-24;
   if( UT < 0 ) UT=UT+24;
   //convert UT value to local time zone of latitude/longitude
   localT = UT + localOffset;
   if( localT < 0 ) localT=localT+24;
   if( localT > 24 ) localT=localT-24;
   sprintf( returntime, "%02.0f:%02.0f",floor(localT),floor((localT-floor(localT))*60) );
   return returntime;
}


