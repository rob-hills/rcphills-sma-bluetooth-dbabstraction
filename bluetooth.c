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
#include <sys/time.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <stdlib.h>

/*
 * Recalculate and update length to correct for escapes
 */
void
fix_length_received(unsigned char *received, int *len)
{
    int     sum;

    if( received[1] != (*len) )
    {
      sum = received[1]+received[3];
      log_debug("sum [%x]", sum );
      log_debug("length change from [%x] to [%x]", received[1], (*len) );
      if(( received[3] != 0x13 )&&( received[3] != 0x14 )) {
        received[1] = (*len);
        switch( received[1] ) {
          case 0x52: received[3]=0x2c; break;
          case 0x5a: received[3]=0x24; break;
          case 0x66: received[3]=0x1a; break;
          case 0x6a: received[3]=0x14; break;
          default:  received[3]=sum-received[1]; break;
        }
      }
    }
}

int
read_bluetooth(time_t const bt_timeout, int const sfd, int *rr, unsigned char *received, int cc, unsigned char *last_sent, int *terminated )
{
    int bytes_read,i;
    unsigned char buf[1024]; /*read buffer*/
    unsigned char header[3]; /*read buffer*/
    struct timeval tv;
    fd_set readfds;

    tv.tv_sec = bt_timeout; // set timeout of reading
    tv.tv_usec = 0;
    memset(buf,0,1024);

    FD_ZERO(&readfds);
    FD_SET((sfd), &readfds);

    select((sfd)+1, &readfds, NULL, NULL, &tv);

    (*terminated) = 0; // Tag to tell if string has 7e termination
    // first read the header to get the record length
    if (FD_ISSET((sfd), &readfds)){     // did we receive anything within 5 seconds
        bytes_read = recv((sfd), header, sizeof(header), 0); //Get length of string
        (*rr) = 0;
        for( i=0; i<sizeof(header); i++ ) {
            received[(*rr)] = header[i];
//            log_trace("%02x ", received[i]);
            (*rr)++;
        }
    }
    else
    {
       log_warning("Timeout reading bluetooth socket");
       (*rr) = 0;
       memset(received,0,1024);
       return -1;
    }
    if (FD_ISSET((sfd), &readfds)){     // did we receive anything within 5 seconds
        bytes_read = recv((sfd), buf, header[1]-3, 0); //Read the length specified by header
    }
    else
    {
       log_warning("Timeout reading bluetooth socket");
       (*rr) = 0;
       memset(received,0,1024);
       return -1;
    }
    if ( bytes_read > 0){
        hlog_debug("Receiving - header", header, sizeof(header), 12);
        hlog_debug("Receiving - body  ", buf, bytes_read, 0);

        if ((cc==bytes_read)&&(memcmp(received,last_sent,cc) == 0)){
           log_error( "ERROR received what we sent!" );
           abort();
           //Need to do something
        }
        if( buf[ bytes_read-1 ] == 0x7e )
           (*terminated) = 1;
        else
           (*terminated) = 0;
        for (i=0;i<bytes_read;i++){ //start copy the rec buffer in to received
            if (buf[i] == 0x7d){ //did we receive the escape char
                switch (buf[i+1]){   // act depending on the char after the escape char

                    case 0x5e :
                        received[(*rr)] = 0x7e;
                        break;

                    case 0x5d :
                        received[(*rr)] = 0x7d;
                        break;

                    default :
                        received[(*rr)] = buf[i+1] ^ 0x20;
                        break;
                }
                    i++;
            }
            else {
               received[(*rr)] = buf[i];
            }
//            log_trace("%02x ", received[(*rr)]);
            (*rr)++;
        }
        fix_length_received( received, rr );
        log_trace("received", received, *rr, 0);
    }
    return 0;
}
