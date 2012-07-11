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

#define _XOPEN_SOURCE /*for strptime, before the includes */

#include "pvlogger.h"
#include "logging.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <bluetooth/bluetooth.h>
#include <bluetooth/rfcomm.h>
#include <string.h>
#include <assert.h>
#include <sys/types.h>
#include <curl/curl.h>
#include "db_interface.h"
#include <math.h>

/*
 * u16 represents an unsigned 16-bit number.  Adjust the typedef for
 * your hardware.
 */
typedef u_int16_t u16;

#define PPPINITFCS16 0xffff /* Initial FCS value    */
#define PPPGOODFCS16 0xf0b8 /* Good final FCS value */
#define ASSERT(x) assert(x)
#define SCHEMA_VALUE 2      /* Current database schema */


typedef struct{
    char Inverter[20];              /*--inverter     -i     */
    char BTAddress[20];             /*--address      -a     */
    int  bt_timeout;                /*--timeout      -t     */
    char Password[20];              /*--password     -p     */
    char Config[80];                /*--config       -c     */
    char File[80];                  /*--file         -f     */
    float latitude_f;               /*--latitude     -la    */
    float longitude_f;              /*--longitude    -lo    */
    char MySqlHost[40];             /*--mysqlhost    -h     */
    char MySqlDatabase[80];         /*--mysqldb      -d     */
    char MySqlUser[80];             /*--mysqluser    -user  */
    char MySqlPwd[80];              /*--mysqlpwd     -pwd   */
    char PVOutputURL[80];           /*--pvouturl     -url   */
    char PVOutputKey[80];           /*--pvoutkey     -key   */
    char PVOutputSid[20];           /*--pvoutsid     -sid   */
    char Setting[80];               /* inverter model data  */
    unsigned char InverterCode[4];  /* Unknown code inverter specific*/
    unsigned int ArchiveCode;       /* Code for archive data */
} ConfType;

typedef struct{
    unsigned int    key1;
    unsigned int    key2;
    char            description[20];
    char            units[20];
    float           divisor;
} ReturnType;

char *accepted_strings[] = {
"$END",
"$ADDR",
"$TIME",
"$SER",
"$CRC",
"$POW",
"$DTOT",
"$ADD2",
"$CHAN",
"$ITIME",
"$TMMI",
"$TMPL",
"$TIMESTRING",
"$TIMEFROM1",
"$TIMETO1",
"$TIMEFROM2",
"$TIMETO2",
"$TESTDATA",
"$ARCHIVEDATA1",
"$PASSWORD",
"$SIGNAL",
"$UNKNOWN",
"$INVCODE",
"$ARCHCODE",
"$INVERTERDATA",
"$CNT",         /*Counter of sent packets*/
"$TIMEZONE",    /*Timezone seconds +1 from GMT*/
"$TIMESET"      /*Unknown string involved in time setting*/
};

int cc;
int skip_daylight_check = 0;
unsigned char fl[1024] = { 0 };

static u16 fcstab[256] = {
   0x0000, 0x1189, 0x2312, 0x329b, 0x4624, 0x57ad, 0x6536, 0x74bf,
   0x8c48, 0x9dc1, 0xaf5a, 0xbed3, 0xca6c, 0xdbe5, 0xe97e, 0xf8f7,
   0x1081, 0x0108, 0x3393, 0x221a, 0x56a5, 0x472c, 0x75b7, 0x643e,
   0x9cc9, 0x8d40, 0xbfdb, 0xae52, 0xdaed, 0xcb64, 0xf9ff, 0xe876,
   0x2102, 0x308b, 0x0210, 0x1399, 0x6726, 0x76af, 0x4434, 0x55bd,
   0xad4a, 0xbcc3, 0x8e58, 0x9fd1, 0xeb6e, 0xfae7, 0xc87c, 0xd9f5,
   0x3183, 0x200a, 0x1291, 0x0318, 0x77a7, 0x662e, 0x54b5, 0x453c,
   0xbdcb, 0xac42, 0x9ed9, 0x8f50, 0xfbef, 0xea66, 0xd8fd, 0xc974,
   0x4204, 0x538d, 0x6116, 0x709f, 0x0420, 0x15a9, 0x2732, 0x36bb,
   0xce4c, 0xdfc5, 0xed5e, 0xfcd7, 0x8868, 0x99e1, 0xab7a, 0xbaf3,
   0x5285, 0x430c, 0x7197, 0x601e, 0x14a1, 0x0528, 0x37b3, 0x263a,
   0xdecd, 0xcf44, 0xfddf, 0xec56, 0x98e9, 0x8960, 0xbbfb, 0xaa72,
   0x6306, 0x728f, 0x4014, 0x519d, 0x2522, 0x34ab, 0x0630, 0x17b9,
   0xef4e, 0xfec7, 0xcc5c, 0xddd5, 0xa96a, 0xb8e3, 0x8a78, 0x9bf1,
   0x7387, 0x620e, 0x5095, 0x411c, 0x35a3, 0x242a, 0x16b1, 0x0738,
   0xffcf, 0xee46, 0xdcdd, 0xcd54, 0xb9eb, 0xa862, 0x9af9, 0x8b70,
   0x8408, 0x9581, 0xa71a, 0xb693, 0xc22c, 0xd3a5, 0xe13e, 0xf0b7,
   0x0840, 0x19c9, 0x2b52, 0x3adb, 0x4e64, 0x5fed, 0x6d76, 0x7cff,
   0x9489, 0x8500, 0xb79b, 0xa612, 0xd2ad, 0xc324, 0xf1bf, 0xe036,
   0x18c1, 0x0948, 0x3bd3, 0x2a5a, 0x5ee5, 0x4f6c, 0x7df7, 0x6c7e,
   0xa50a, 0xb483, 0x8618, 0x9791, 0xe32e, 0xf2a7, 0xc03c, 0xd1b5,
   0x2942, 0x38cb, 0x0a50, 0x1bd9, 0x6f66, 0x7eef, 0x4c74, 0x5dfd,
   0xb58b, 0xa402, 0x9699, 0x8710, 0xf3af, 0xe226, 0xd0bd, 0xc134,
   0x39c3, 0x284a, 0x1ad1, 0x0b58, 0x7fe7, 0x6e6e, 0x5cf5, 0x4d7c,
   0xc60c, 0xd785, 0xe51e, 0xf497, 0x8028, 0x91a1, 0xa33a, 0xb2b3,
   0x4a44, 0x5bcd, 0x6956, 0x78df, 0x0c60, 0x1de9, 0x2f72, 0x3efb,
   0xd68d, 0xc704, 0xf59f, 0xe416, 0x90a9, 0x8120, 0xb3bb, 0xa232,
   0x5ac5, 0x4b4c, 0x79d7, 0x685e, 0x1ce1, 0x0d68, 0x3ff3, 0x2e7a,
   0xe70e, 0xf687, 0xc41c, 0xd595, 0xa12a, 0xb0a3, 0x8238, 0x93b1,
   0x6b46, 0x7acf, 0x4854, 0x59dd, 0x2d62, 0x3ceb, 0x0e70, 0x1ff9,
   0xf78f, 0xe606, 0xd49d, 0xc514, 0xb1ab, 0xa022, 0x92b9, 0x8330,
   0x7bc7, 0x6a4e, 0x58d5, 0x495c, 0x3de3, 0x2c6a, 0x1ef1, 0x0f78
};

/*
 * Calculate a new fcs given the current fcs and the new data.
 */
u16 pppfcs16(u16 fcs, void *_cp, int len)
{
    register unsigned char *cp = (unsigned char *)_cp;
    /* don't worry about the efficiency of these asserts here.  gcc will
     * recognise that the asserted expressions are constant and remove them.
     * Whether they are usefull is another question. 
     */

    ASSERT(sizeof (u16) == 2);
    ASSERT(((u16) -1) > 0);
    while (len--)
        fcs = (fcs >> 8) ^ fcstab[(fcs ^ *cp++) & 0xff];
    return (fcs);
}

/*
 * Strip escapes (7D) as they aren't includes in fcs
 */
void strip_escapes(unsigned char *cp, int *len)
{
    int i,j;

    for( i=0; i<(*len); i++ ) {
      if( cp[i] == 0x7d ) { /*Found escape character. Need to convert*/
        cp[i] = cp[i+1]^0x20;
        for( j=i+1; j<(*len)-1; j++ ) cp[j] = cp[j+1];
        (*len)--;
      }
   }
}

/*
 * Add escapes (7D) as they are required
 */
void add_escapes(unsigned char *cp, int *len)
{
    int i,j;

    for( i=19; i<(*len); i++ ) {
      switch( cp[i] ) {
        case 0x7d :
        case 0x7e :
        case 0x11 :
        case 0x12 :
        case 0x13 :
          for( j=(*len); j>i; j-- ) cp[j] = cp[j-1];
          cp[i+1] = cp[i]^0x20;
          cp[i]=0x7d;
          (*len)++;
          break;
      }
   }
}

/*
 * Recalculate and update length to correct for escapes
 */
void fix_length_send(unsigned char *cp, int *len)
{
    int        delta=0;

    log_debug("sum [%x]", cp[1]+cp[3]);

    if(( cp[1] != (*len)+1 ))
    {
      delta = (*len)+1 - cp[1];
      log_debug("length change from [%x] to [%x]; diff [%x]",
                cp[1],(*len)+1,cp[1]+cp[3]);
      }
      cp[3] = (cp[1]+cp[3])-((*len)+1);
      cp[1] =(*len)+1;

      switch( cp[1] ) {
        case 0x3a: cp[3]=0x44; break;
        case 0x3b: cp[3]=0x43; break;
        case 0x3c: cp[3]=0x42; break;
        case 0x3d: cp[3]=0x41; break;
        case 0x3e: cp[3]=0x40; break;
        case 0x3f: cp[3]=0x41; break;
        case 0x40: cp[3]=0x3e; break;
        case 0x41: cp[3]=0x3f; break;
        case 0x42: cp[3]=0x3c; break;
        case 0x52: cp[3]=0x2c; break;
        case 0x53: cp[3]=0x2b; break;
        case 0x54: cp[3]=0x2a; break;
        case 0x55: cp[3]=0x29; break;
        case 0x56: cp[3]=0x28; break;
        case 0x57: cp[3]=0x27; break;
        case 0x58: cp[3]=0x26; break;
        case 0x59: cp[3]=0x25; break;
        case 0x5a: cp[3]=0x24; break;
        case 0x5b: cp[3]=0x23; break;
        case 0x5c: cp[3]=0x22; break;
        case 0x5d: cp[3]=0x23; break;
        case 0x5e: cp[3]=0x20; break;
        case 0x5f: cp[3]=0x21; break;
        case 0x60: cp[3]=0x1e; break;
        case 0x61: cp[3]=0x1f; break;
        case 0x62: cp[3]=0x1e; break;
        default:
                log_fatal("NO CONVERSION!");
                abort();
                break;
      }
      log_debug("new sum [%x]", cp[1]+cp[3]);
}

/*
 * How to use the fcs
 */
void tryfcs16(unsigned char *cp, int len)
{
    u16 trialfcs;
    unsigned char stripped[1024] = { 0 };

    memcpy( stripped, cp, len );
    /* add on output */
    hlog_trace("String to calculate FCS", cp, len, 0);
    trialfcs = pppfcs16( PPPINITFCS16, stripped, len );
    trialfcs ^= 0xffff;                 /* complement */
    fl[cc] = (trialfcs & 0x00ff);       /* least significant byte first */
    fl[cc+1] = ((trialfcs >> 8) & 0x00ff);
    cc+=2;
    log_trace("FCS = [%x%x] [%x]",
              (trialfcs & 0x00ff),((trialfcs >> 8) & 0x00ff), trialfcs);
}


unsigned char conv(char *nn){
    unsigned char tt=0,res=0;
    int i;   
    
    for(i=0;i<2;i++){
        switch(nn[i]){

        case 65: /*A*/
        case 97: /*a*/
        tt = 10;
        break;

        case 66: /*B*/
        case 98: /*b*/
        tt = 11;
        break;

        case 67: /*C*/
        case 99: /*c*/
        tt = 12;
        break;

        case 68: /*D*/
        case 100: /*d*/
        tt = 13;
        break;

        case 69: /*E*/
        case 101: /*e*/
        tt = 14;
        break;

        case 70: /*F*/
        case 102: /*f*/
        tt = 15;
        break;


        default:
        tt = nn[i] - 48;
        break;
        }
        res = res + (tt * pow(16,1-i));
        }
        return res;
}

int
check_send_error( ConfType * conf, int *s, int *rr, unsigned char *received, int cc, unsigned char *last_sent, int *terminated, int *already_read )
{
    int bytes_read,i;
    unsigned char buf[1024]; /*read buffer*/
    unsigned char header[3]; /*read buffer*/
    struct timeval tv;
    fd_set readfds;

    tv.tv_sec = 0; // set timeout of reading
    tv.tv_usec = 5000;
    memset(buf,0,1024);

    FD_ZERO(&readfds);
    FD_SET((*s), &readfds);
                
    select((*s)+1, &readfds, NULL, NULL, &tv);
                
    (*terminated) = 0; // Tag to tell if string has 7e termination
    // first read the header to get the record length
    if (FD_ISSET((*s), &readfds)){    // did we receive anything within 5 seconds
        bytes_read = recv((*s), header, sizeof(header), 0); //Get length of string
    (*rr) = 0;
        for( i=0; i<sizeof(header); i++ ) {
            received[(*rr)] = header[i];
            log_debug("%02x ", received[(*rr)]);
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
    if (FD_ISSET((*s), &readfds)){    // did we receive anything within 5 seconds
        bytes_read = recv((*s), buf, header[1]-3, 0); //Read the length specified by header
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
           abort;
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
        log_debug("%02x ", received[(*rr)]);
        (*rr)++;
    }
    fix_length_received( received, rr );
    hlog_debug("received", received, *rr, 0);

    (*already_read)=1;
    }    
    return 0;
}

int select_str(char *s)
{
    int i;
    for (i=0; i < sizeof(accepted_strings)/sizeof(*accepted_strings);i++)
    {
       //printf( "\ni=%d accepted=%s string=%s", i, accepted_strings[i], s );
       if (!strcmp(s, accepted_strings[i])) return i;
    }
    return -1;
}

unsigned char *  get_timezone_in_seconds( unsigned char *tzhex )
{
    time_t curtime;
    struct tm *loctime;
    struct tm *utctime;
    int day,month,year,hour,minute,isdst;
    char *returntime;

    float localOffset;
    int     tzsecs;

    returntime = (char *)malloc(6*sizeof(char));
    curtime = time(NULL);  //get time in seconds since epoch (1/1/1970)    
    loctime = localtime(&curtime);
    day = loctime->tm_mday;
    month = loctime->tm_mon +1;
    year = loctime->tm_year + 1900;
    hour = loctime->tm_hour;
    minute = loctime->tm_min; 
    isdst  = loctime->tm_isdst;
    utctime = gmtime(&curtime);
   

    log_debug("utc=%04d-%02d-%02d %02d:%02d local=%04d-%02d-%02d %02d:%02d diff %d hours",
               utctime->tm_year+1900, utctime->tm_mon+1,utctime->tm_mday,utctime->tm_hour,utctime->tm_min, 
               year, month, day, hour, minute, hour-utctime->tm_hour );
    localOffset=(hour-utctime->tm_hour)+(minute-utctime->tm_min)/60;
    log_debug("localOffset=%f", localOffset);
    if(( year > utctime->tm_year+1900 )||( month > utctime->tm_mon+1 )||( day > utctime->tm_mday ))
        localOffset+=24;
    if(( year < utctime->tm_year+1900 )||( month < utctime->tm_mon+1 )||( day < utctime->tm_mday ))
        localOffset-=24;
    log_debug("localOffset=%f isdst=%d", localOffset, isdst );
    if( isdst > 0 ) 
        localOffset=localOffset-1;
    tzsecs = (localOffset) * 3600 + 1;
    if( tzsecs < 0 )
        tzsecs=65536+tzsecs;
    log_debug("tzsecs=%x %d", tzsecs, tzsecs);
    tzhex[1] = tzsecs/256;
    tzhex[0] = tzsecs -(tzsecs/256)*256;
    log_debug("tzsecs=%02x %02x", tzhex[1], tzhex[0]);

    return tzhex;
}

int auto_set_dates( int * daterange, int mysql, char * datefrom, char * dateto )
/*  If there are no dates set - get last updated date and go from there to NOW */
{
    time_t      curtime;
    struct tm     loctime;
    curtime = time(NULL);  //get time in seconds since epoch (1/1/1970)    
    loctime = *(localtime(&curtime));
    if( mysql == 1 )
    {
      struct tm last;
      last = db_get_last_recorded_interval_datetime(&loctime);
      if( last.tm_year > 100 ) //ie, after year 2000
      {
        strftime(datefrom, 25, "%Y-%m-%d %H:%M:%S", &last );
      }
    }
    if( strlen( datefrom ) == 0 )
        strcpy( datefrom, "2000-01-01 00:00:00" );

    //curtime = time(NULL);  //get time in seconds since epoch (1/1/1970)    
    loctime = *(localtime(&curtime));
    loctime.tm_sec = 0;
    strftime(dateto, 25, "%Y-%m-%d %H:%M:%S", &loctime );
    (*daterange)=1;
    log_verbose("Auto set dates from [%s] to [%s]", datefrom, dateto);
    return 1;
}

int is_light( )
/* Check if all data done and past sunset or before sunrise 
 * Returns true if:
 *   - time is between sunrise and sunset
 *   - skip_daylight_check is true
 */
{
    char sunrise[25];
    char sunset[25];
    char timestring[25];
    time_t timenow = time(NULL);
    struct tm now = *(localtime( &timenow ) );
    if (skip_daylight_check != 0) {
        log_debug("Force option specified, skipping Daylight check.");
        return 1;
    }
    if(! db_fetch_almanac( &now,  sunrise,  sunset ) ) {
        return 1; //can't tell - no sunrise/set in db
    }
    strftime(timestring,25,"%H:%M", &now);
    log_verbose( "now: %s", timestring );
    if( strcmp( timestring, sunrise ) >= 0 && strcmp( timestring, sunset ) <= 0) 
        return 1; //now is between sunrise and sunset
    return 0;
}

//Convert a recieved string to a value
long ConvertStreamtoLong( unsigned char * stream, int length, long unsigned int * value )
{
   int    i, nullvalue;
   
   (*value) = 0;
   nullvalue = 1;

   for( i=0; i < length; i++ ) 
   {
      if( stream[i] != 0xff ) //check if all ffs which is a null value 
        nullvalue = 0;
      (*value) = (*value) + stream[i]*pow(256,i);
   }
   if( nullvalue == 1 )
      (*value) = 0; //Asigning null to 0 at this stage unless it breaks something
   return (*value);
}

//Convert a recieved string to a value
float ConvertStreamtoFloat( unsigned char * stream, int length, float * value )
{
   int    i, nullvalue;
   
   (*value) = 0;
   nullvalue = 1;

   for( i=0; i < length; i++ ) 
   {
      if( stream[i] != 0xff ) //check if all ffs which is a null value 
        nullvalue = 0;
      (*value) = (*value) + stream[i]*pow(256,i);
   }
   if( nullvalue == 1 )
      (*value) = 0; //Asigning null to 0 at this stage unless it breaks something
   return (*value);
}

//read return value data from init file
ReturnType * 
InitReturnKeys( ConfType * conf, ReturnType * returnkeylist, int * num_return_keys )
{
   FILE        *fp;
   char        line[400];
   ReturnType   tmp;
   int        i, j, reading, data_follows;

   data_follows = 0;

   fp=fopen(conf->File,"r");

   while (!feof(fp)){    
    if (fgets(line,400,fp) != NULL){                //read line from smatool.conf
            if( line[0] != '#' ) 
            {
                if( strncmp( line, ":unit conversions", 17 ) == 0 )
                    data_follows = 1;
                if( strncmp( line, ":end unit conversions", 21 ) == 0 )
                    data_follows = 0;
                if( data_follows == 1 ) {
                    tmp.key1=0x0;
                    tmp.key2=0x0;
                    strcpy( tmp.description, "" ); //Null out value
                    strcpy( tmp.units, "" ); //Null out value
                    tmp.divisor=0;
                    reading=0;
                    if( sscanf( line, "%x %x", &tmp.key1, &tmp.key2  ) == 2 ) {
                        j=0;
                        for( i=6; line[i]!='\0'; i++ ) {
                            if(( line[i] == '"' )&&(reading==1)) {
                                tmp.description[j]='\0';
                                break;
                            }
                            if( reading == 1 )
                            {
                                tmp.description[j] = line[i];
                                j++;
                            }
                             
                            if(( line[i] == '"' )&&(reading==0))
                                reading = 1;
                        }
                        if( sscanf( line+i+1, "%s %f", tmp.units, &tmp.divisor ) == 2 ) {
                              
                            if( (*num_return_keys) == 0 )
                                returnkeylist=(ReturnType *)malloc(sizeof(ReturnType));
                            else
                                returnkeylist=(ReturnType *)realloc(returnkeylist,sizeof(ReturnType)*((*num_return_keys)+1));
                            (returnkeylist+(*num_return_keys))->key1=tmp.key1;
                            (returnkeylist+(*num_return_keys))->key2=tmp.key2;
                            strcpy( (returnkeylist+(*num_return_keys))->description, tmp.description );
                            strcpy( (returnkeylist+(*num_return_keys))->units, tmp.units );
                            (returnkeylist+(*num_return_keys))->divisor = tmp.divisor;
                            (*num_return_keys)++;
                        }
                    }
                }
            }
        }
    }
    if(fp) {
        /* This is a hack:
         * It is not clean when the file is really used...
         */
        fclose(fp);
    }
   
    return returnkeylist;
}

//Convert a recieved string to a value
int ConvertStreamtoInt( unsigned char * stream, int length, int * value )
{
   int    i, nullvalue;
   
   (*value) = 0;
   nullvalue = 1;

   for( i=0; i < length; i++ ) 
   {
      if( stream[i] != 0xff ) //check if all ffs which is a null value 
        nullvalue = 0;
      (*value) = (*value) + stream[i]*pow(256,i);
   }
   if( nullvalue == 1 )
      (*value) = 0; //Asigning null to 0 at this stage unless it breaks something
   return (*value);
}

//Convert a recieved string to a value
time_t ConvertStreamtoTime( unsigned char * stream, int length, time_t * value )
{
   int    i, nullvalue;
   
   (*value) = 0;
   nullvalue = 1;

   for( i=0; i < length; i++ ) 
   {
      if( stream[i] != 0xff ) //check if all ffs which is a null value 
        nullvalue = 0;
      (*value) = (*value) + stream[i]*pow(256,i);
   }
   if( nullvalue == 1 )
      (*value) = 0; //Asigning null to 0 at this stage unless it breaks something
   return (*value);
}

// Set switches to save lots of strcmps
void  SetSwitches( ConfType *conf, char * datefrom, char * dateto, int *location, int *mysql, int *post, int *file, int *daterange, int *test )  
{
    //Check if all location variables are set
    if(( conf->latitude_f <= 180 )&&( conf->longitude_f <= 180 ))
        (*location)=1;
    else
        (*location)=0;
    //Check if all Mysql variables are set
    if(( strlen(conf->MySqlUser) > 0 )
     &&( strlen(conf->MySqlPwd) > 0 )
     &&( strlen(conf->MySqlHost) > 0 )
     &&( strlen(conf->MySqlDatabase) > 0 )
     &&( (*test)==0 ))
        (*mysql)=1;
    else
        (*mysql)=0;
    //Check if all File variables are set
    if( strlen(conf->File) > 0 )
        (*file)=1;
    else
        (*file)=0;
    //Check if all PVOutput variables are set
    if(( strlen(conf->PVOutputURL) > 0 )
     &&( strlen(conf->PVOutputKey) > 0 )
     &&( strlen(conf->PVOutputSid) > 0 ))
        (*post)=1;
    else
        (*post)=0;
    if(( strlen(datefrom) > 0 )
     &&( strlen(dateto) > 0 ))
        (*daterange)=1;
    else
        (*daterange)=0;
}

unsigned char *
ReadStream( ConfType * conf, int * s, unsigned char * stream, int * streamlen, unsigned char * datalist, int * datalen, unsigned char * last_sent, int cc, int * terminated, int * togo )
{
   int    finished;
   int    finished_record;
   int  i, j=0;

   (*togo)=ConvertStreamtoInt( stream+43, 2, togo );
   log_debug("togo=%d", (*togo));
   i=59; //Initial position of data stream
   (*datalen)=0;
   datalist=(unsigned char *)malloc(sizeof(char));
   finished=0;
   finished_record=0;
   while( finished != 1 ) {
     datalist=(unsigned char *)realloc(datalist,sizeof(char)*((*datalen)+(*streamlen)-i));
     while( finished_record != 1 ) {
        if( i> 500 ) break; //Somthing has gone wrong
        
        if(( i < (*streamlen) )&&(( (*terminated) != 1)||(i+3 < (*streamlen) ))) 
    {
           datalist[j]=stream[i];
           j++;
           (*datalen)=j;
           i++;
        }
        else
           finished_record = 1;
           
     }
     finished_record = 0;
     if( (*terminated) == 0 )
     {
         read_bluetooth( conf->bt_timeout, *s, streamlen, 
                            stream, cc, last_sent, terminated );
         i=18;
     }
     else
         finished = 1;
   }
   hlog_debug("Datalist", datalist, *datalen, 0);
   return datalist;
}

/* Init Config to default values */
void InitConfig( ConfType *conf, char * datefrom, char * dateto )
{
    log_trace ("Starting InitConfig");
    strcpy( conf->Config,"./smatool.conf");
    strcpy( conf->Setting,"./invcode.in");
    strcpy( conf->Inverter, "" );  
    strcpy( conf->BTAddress, "" );  
    conf->bt_timeout = 30;  
    strcpy( conf->Password, "0000" );  
    strcpy( conf->File, "sma.in.new" );  
    conf->latitude_f = 999 ;  
    conf->longitude_f = 999 ;  
    strcpy( conf->MySqlHost, "localhost" );  
    strcpy( conf->MySqlDatabase, "smatool" );  
    strcpy( conf->MySqlUser, "" );  
    strcpy( conf->MySqlPwd, "" );  
    strcpy( conf->PVOutputURL, "http://pvoutput.org/service/r2/addstatus.jsp" );  
    strcpy( conf->PVOutputKey, "" );  
    strcpy( conf->PVOutputSid, "" );
    conf->InverterCode[0]=0;
    conf->InverterCode[1]=0;
    conf->InverterCode[2]=0;
    conf->InverterCode[3]=0;
    conf->ArchiveCode=0;
    strcpy( datefrom, "" );  
    strcpy( dateto, "" );  
    log_trace ("Finished InitConfig");
}

/* read Config from file */
int GetConfig( ConfType *conf )
{
    FILE     *fp;
    char    line[400];
    char    variable[400];
    char    value[400];

    if (strlen(conf->Config) > 0 )
    {
        if(( fp=fopen(conf->Config,"r")) == (FILE *)NULL )
        {
           log_fatal("Error! Could not open file %s", conf->Config);
           return( -1 ); //Could not open file
        }
    }
    else
    {
        if(( fp=fopen("./smatool.conf","r")) == (FILE *)NULL )
        {
           log_fatal("Error! Could not open file ./smatool.conf");
           return( -1 ); //Could not open file
        }
    }
    while (!feof(fp)){    
    if (fgets(line,400,fp) != NULL){                //read line from smatool.conf
            if( line[0] != '#' ) 
            {
                strcpy( value, "" ); //Null out value
                sscanf( line, "%s %s", variable, value );
                log_debug("variable [%s] value [%s]", variable, value );
                if( value[0] != '\0' )
                {
                    if( strcmp( variable, "Inverter" ) == 0 )
                       strcpy( conf->Inverter, value );  
                    if( strcmp( variable, "BTAddress" ) == 0 )
                       strcpy( conf->BTAddress, value );  
                    if( strcmp( variable, "BTTimeout" ) == 0 )
                       conf->bt_timeout =  atoi(value);  
                    if( strcmp( variable, "Password" ) == 0 )
                       strcpy( conf->Password, value );  
                    if( strcmp( variable, "File" ) == 0 )
                       strcpy( conf->File, value );  
                    if( strcmp( variable, "Latitude" ) == 0 )
                       conf->latitude_f = atof(value) ;  
                    if( strcmp( variable, "Longitude" ) == 0 )
                       conf->longitude_f = atof(value) ;  
                    if( strcmp( variable, "MySqlHost" ) == 0 )
                       strcpy( conf->MySqlHost, value );  
                    if( strcmp( variable, "MySqlDatabase" ) == 0 )
                       strcpy( conf->MySqlDatabase, value );  
                    if( strcmp( variable, "MySqlUser" ) == 0 )
                       strcpy( conf->MySqlUser, value );  
                    if( strcmp( variable, "MySqlPwd" ) == 0 )
                       strcpy( conf->MySqlPwd, value );  
                    if( strcmp( variable, "PVOutputURL" ) == 0 )
                       strcpy( conf->PVOutputURL, value );  
                    if( strcmp( variable, "PVOutputKey" ) == 0 )
                       strcpy( conf->PVOutputKey, value );  
                    if( strcmp( variable, "PVOutputSid" ) == 0 )
                       strcpy( conf->PVOutputSid, value );  
                }
            }
        }
    }
    fclose( fp );
    return( 0 );
}

/* read  Inverter Settings from file */
int GetInverterSetting( ConfType *conf )
{
    FILE        *fp;
    char        line[400];
    char        variable[400];
    char        value[400];
    /* This variable flags that the current scan process
     * is inside the section for the sought inverter.
     */
    int         found_inverter=0;
    /* This variable flags that the inverter was found
     * in the configuration file.
     */
    int     inverter_in_configuration_file = 0;

    if (strlen(conf->Setting) > 0 )
    {
        if(( fp=fopen(conf->Setting,"r")) == (FILE *)NULL )
        {
           log_fatal( "Error! Could not open file %s", conf->Setting );
           return( -1 ); //Could not open file
        }
    }
    else
    {
        if(( fp=fopen("./invcode.in","r")) == (FILE *)NULL )
        {
           log_fatal( "Error! Could not open file ./invcode.in" );
           return( -1 ); //Could not open file
        }
    }
    while (!feof(fp)) {
        if (fgets(line,400,fp) != NULL){                                //read line from invcode.in
            if( line[0] != '#' ) 
            {
                strcpy( value, "" ); //Null out value
                sscanf( line, "%s %s", variable, value );
                log_debug( "variable=%s value=%s", variable, value );
                if( value[0] != '\0' )
                {
                    if( strcmp( variable, "Inverter" ) == 0 )
                    {
                        if ( found_inverter )
                            break; // Already found our inverter previously, this is a new inverter so no need to process further
                        if( strcmp( value, conf->Inverter ) == 0 ) 
                        {
                            found_inverter = 1;
                            inverter_in_configuration_file = 1;
                            log_debug( "Found inverter: %s", conf->Inverter );
                        } else
                            found_inverter = 0;
                    }
                    if(( strcmp( variable, "Code1" ) == 0 )&& found_inverter )
                    {
                       sscanf( value, "%X", &conf->InverterCode[0] );
                    }
                    if(( strcmp( variable, "Code2" ) == 0 )&& found_inverter )
                       sscanf( value, "%X", &conf->InverterCode[1] );
                    if(( strcmp( variable, "Code3" ) == 0 )&& found_inverter )
                       sscanf( value, "%X", &conf->InverterCode[2] );
                    if(( strcmp( variable, "Code4" ) == 0 )&& found_inverter )
                       sscanf( value, "%X", &conf->InverterCode[3] );
                    if(( strcmp( variable, "InvCode" ) == 0 )&& found_inverter )
                       sscanf( value, "%X", &conf->ArchiveCode );
                }
            }
        }
    }

    fclose( fp );

    if ( !inverter_in_configuration_file ) {
        log_error ( "The inverter [%s] was not found in the invcode.in "
                    "inverter configuration file.  Please check the "
                    "configuration.", conf->Inverter );
        return -1;
    }

    if(( conf->InverterCode[0] == 0 ) ||
       ( conf->InverterCode[1] == 0 ) ||
       ( conf->InverterCode[2] == 0 ) ||
       ( conf->InverterCode[3] == 0 ) ||
       ( conf->ArchiveCode == 0 ))
    {
       log_error( " Error ! not all codes set" );
       log_error( " Code [0]: %d, Code[1]: %d, Code[2]: %d, Code[3]: %d, ArchiveCode: %d", 
                    conf->InverterCode[0], conf->InverterCode[1], conf->InverterCode[2], conf->InverterCode[3], conf->ArchiveCode );
       return( -1 );
    }
    return( 0 );
}


/* Print a help message */
void PrintHelp()
{
    printf( "Usage: smatool [OPTION]\n" );
    printf( "  -v,  --verbose                           Give more verbose output\n" );
    printf( "  -d,  --debug                             Show debug\n" );
    printf( "       --trace                             Show trace\n" );
    printf( "  -f,  --force                             Force inverter query, even if not daytime\n" );
    printf( "  -c,  --config CONFIGFILE                 Set config file default smatool.conf\n" );
    printf( "       --test                              Run in test mode - don't update data\n" );
    printf( "\n" );
    printf( "Dates are no longer required - defaults to last update if using mysql\n" );
    printf( "or 2000 to now if not using mysql\n" );
    printf( "  -from  --datefrom YYYY-DD-MM HH:MM:00    Date range from date\n" );
    printf( "  -to  --dateto YYYY-DD-MM HH:MM:00        Date range to date\n" );
    printf( "\n" );
    printf( "The following options are in config file but may be overridden\n" );
    printf( "  -i,  --inverter INVERTER_MODEL           inverter model\n" );
    printf( "  -a,  --address INVERTER_ADDRESS          inverter BT address\n" );
    printf( "  -t,  --timeout TIMEOUT                   bluetooth timeout (secs) default 5\n" );
    printf( "  -p,  --password PASSWORD                 inverter user password default 0000\n" );
    printf( "  -f,  --file FILENAME                     command file default sma.in.new\n" );
    printf( "Location Information to calculate sunset and sunrise so inverter is not\n" );
    printf( "queried in the dark\n" );
    printf( "  -lat,  --latitude LATITUDE               location latitude -180 to 180 deg\n" );
    printf( "  -lon,  --longitude LONGITUDE             location longitude -90 to 90 deg\n" );
    printf( "Mysql database information\n" );
    printf( "  -H,  --mysqlhost MYSQLHOST               mysql host default localhost\n");
    printf( "  -D,  --mysqldb MYSQLDATBASE              mysql database default smatool\n");
    printf( "  -U,  --mysqluser MYSQLUSER               mysql user\n");
    printf( "  -P,  --mysqlpwd MYSQLPASSWORD            mysql password\n");
    printf( "Mysql tables can be installed using INSTALL you may have to use a higher \n" );
    printf( "privelege user to allow the creation of databases and tables, use command line \n" );
    printf( "       --INSTALL                           install mysql data tables\n");
    printf( "       --UPDATE                            update mysql data tables\n");
    printf( "PVOutput.org (A free solar information system) Configs\n" );
    printf( "  -url,  --pvouturl PVOUTURL               pvoutput.org live url\n");
    printf( "  -key,  --pvoutkey PVOUTKEY               pvoutput.org key\n");
    printf( "  -sid,  --pvoutsid PVOUTSID               pvoutput.org sid\n");
    printf( "  -repost                                  verify and repost data if different\n");
    printf( "\n\n" );
}

/* Init Config to default values */
int ReadCommandConfig( ConfType *conf, int argc, char **argv, char *datefrom, 
                        char *dateto, loglevel_t *loglevel, int *skip_daylight_check, 
                        int *repost, int *test, int *install, int *update )
{
    int    i;

    // these need validation checking at some stage TODO
    for (i=1;i<argc;i++)            //Read through passed arguments
    {
        if(strcmp(argv[i],"-v")==0 || strcmp(argv[i],"--verbose")==0) {
            if(*loglevel>ll_verbose) *loglevel = ll_verbose;
        } else if ((strcmp(argv[i],"-d")==0)||(strcmp(argv[i],"--debug")==0)) {
            if(*loglevel>ll_debug) *loglevel = ll_debug;
        } else if( strcmp(argv[i],"--trace")==0) {
            if(*loglevel>ll_trace) *loglevel = ll_trace;
        } else if ((strcmp(argv[i],"-f")==0)||(strcmp(argv[i],"--force")==0)) (*skip_daylight_check) = 1;
        else if ((strcmp(argv[i],"-c")==0)||(strcmp(argv[i],"--config")==0)) {
            i++;
            if(i<argc){
                strcpy(conf->Config,argv[i]);
            }
        }
        else if (strcmp(argv[i],"--test")==0) (*test)=1;
        else if ((strcmp(argv[i],"-from")==0)||(strcmp(argv[i],"--datefrom")==0))   {
            i++;
            if(i<argc){
                strcpy(datefrom,argv[i]);
            }
        }
        else if ((strcmp(argv[i],"-to")==0)||(strcmp(argv[i],"--dateto")==0)) {
            i++;
            if(i<argc){
                strcpy(dateto,argv[i]);
            }
        }
        else if (strcmp(argv[i],"-repost")==0) {
            i++;
            (*repost)=1;
        }
        else if ((strcmp(argv[i],"-i")==0)||(strcmp(argv[i],"--inverter")==0)) {
            i++;
            if (i<argc){
                strcpy(conf->Inverter,argv[i]);
            }
        }
        else if ((strcmp(argv[i],"-a")==0)||(strcmp(argv[i],"--address")==0)) {
            i++;
            if (i<argc){
                strcpy(conf->BTAddress,argv[i]);
            }
        }
        else if ((strcmp(argv[i],"-t")==0)||(strcmp(argv[i],"--timeout")==0)) {
            i++;
            if (i<argc){
                conf->bt_timeout = atoi(argv[i]);
            }
        }
        else if ((strcmp(argv[i],"-p")==0)||(strcmp(argv[i],"--password")==0)) {
            i++;
            if (i<argc){
                strcpy(conf->Password,argv[i]);
            }
        }
        else if ((strcmp(argv[i],"-f")==0)||(strcmp(argv[i],"--file")==0)) {
            i++;
            if (i<argc){
                strcpy(conf->File,argv[i]);
            }
        }
        else if ((strcmp(argv[i],"-lat")==0)||(strcmp(argv[i],"--latitude")==0)) {
            i++;
            if(i<argc){
                conf->latitude_f=atof(argv[i]);
            }
        }
        else if ((strcmp(argv[i],"-long")==0)||(strcmp(argv[i],"--longitude")==0)) {
            i++;
            if(i<argc){
                conf->longitude_f=atof(argv[i]);
            }
        }
        else if ((strcmp(argv[i],"-H")==0)||(strcmp(argv[i],"--mysqlhost")==0)) {
            i++;
            if (i<argc){
                strcpy(conf->MySqlHost,argv[i]);
            }
        }
        else if ((strcmp(argv[i],"-D")==0)||(strcmp(argv[i],"--mysqlcwdb")==0)) {
            i++;
            if (i<argc){
                strcpy(conf->MySqlDatabase,argv[i]);
            }
        }
        else if ((strcmp(argv[i],"-U")==0)||(strcmp(argv[i],"--mysqluser")==0)) {
            i++;
            if (i<argc){
                strcpy(conf->MySqlUser,argv[i]);
            }
        }
        else if ((strcmp(argv[i],"-P")==0)||(strcmp(argv[i],"--mysqlpwd")==0)) {
            i++;
            if (i<argc){
                strcpy(conf->MySqlPwd,argv[i]);
            }
        }                
        else if ((strcmp(argv[i],"-url")==0)||(strcmp(argv[i],"--pvouturl")==0)) {
            i++;
            if(i<argc){
                strcpy(conf->PVOutputURL,argv[i]);
            }
        }
        else if ((strcmp(argv[i],"-key")==0)||(strcmp(argv[i],"--pvoutkey")==0)) {
            i++;
            if(i<argc){
                strcpy(conf->PVOutputKey,argv[i]);
            }
        }
        else if ((strcmp(argv[i],"-sid")==0)||(strcmp(argv[i],"--pvoutsid")==0)) {
            i++;
            if(i<argc){
                strcpy(conf->PVOutputSid,argv[i]);
            }
        }
        else if ((strcmp(argv[i],"-h")==0) || (strcmp(argv[i],"--help") == 0 )) {
            PrintHelp();
            return( -1 );
        }
        else if (strcmp(argv[i],"--INSTALL")==0) (*install)=1;
        else if (strcmp(argv[i],"--UPDATE")==0) (*update)=1;
        else {
            printf("Bad Syntax\n\n" );
            for( i=0; i< argc; i++ )
                printf( "%s ", argv[i] );
            printf( "\n\n" );

            PrintHelp();
            return( -1 );
        }
    }

    log_info ( "Log Level set to [%s]", level2type( *loglevel ) );

    return( 0 );
}

size_t write_data(void *ptr, size_t size, size_t nmemb, void *stream) 
{
    size_t written;

    written = fwrite(ptr, size, nmemb, stream);
    return written;
}

int curl_post_this_query( char *compurl, char *pvOutputKey, char *pvOutputSid, loglevel_t loglevel)
{
  CURL *curl;
  CURLcode result;
  char *curlErrorText = (char*)malloc(CURL_ERROR_SIZE);
  char header[255];

  curlErrorText[0] = '\0';
  curl = curl_easy_init();
  if (curl){
    log_debug( "url = %s",compurl );
    if (loglevel >= ll_debug){
        curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, curlErrorText);
    }

    struct curl_slist *slist=NULL;

    sprintf(header, "X-Pvoutput-Apikey: %s",pvOutputKey );
    slist = curl_slist_append(slist, header);
    sprintf(header, "X-Pvoutput-SystemId: %s",pvOutputSid );
    slist = curl_slist_append(slist, header);

    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, slist);
    curl_easy_setopt(curl, CURLOPT_URL, compurl);

    result = curl_easy_perform(curl);
    log_debug( "result = %d",result );
    log_debug( "Error message = %s",curlErrorText );
    if (result != 0){
        log_error( "Unable to post data to PVOutput.  CURL result = %d",result );
        log_error( "Error message = %s",curlErrorText );
    }
    curl_slist_free_all(slist);
    curl_easy_cleanup(curl);
    free(curlErrorText);
    return result;
  }
  return -1;
}

void post_interval_data(char *pvOutputUrl, char *pvOutputKey, char *pvOutputSid, int repost, char *datefrom, char *dateto, loglevel_t loglevel)
{
  time_t prior = time(NULL) - ( 60 * 60 * 24 * 14 ); //up to 14 days before now (r2 service)
  struct tm from_datetime = *(localtime( &prior ) );
  if (repost == 0) {
      from_datetime.tm_hour = 0;
      from_datetime.tm_min = 0;
      from_datetime.tm_sec = 0;
  } else {
      /*
       **************** TODO ********************
       * Need to be able to post data between 2 dates which means modifying the db_get_unposted_data function.
       */
      strptime( datefrom, "%Y-%m-%d %H:%M:%S", &from_datetime );  // reposting, use datefrom
      log_debug( "checking DB for unposted data after from_datetime = %04d-%02d-%02d %02d:%02d", 
                 from_datetime.tm_year+1900, from_datetime.tm_mon+1, from_datetime.tm_mday, 
                 from_datetime.tm_hour, from_datetime.tm_min );
  }

  row_handle *row = db_get_unposted_data( &from_datetime );
  if( row == NULL )
  {
    log_debug ( "No data posted because db_get_unposted_data returned NULL " );
    log_debug ( "for from_datetime = %04d-%02d-%02d %02d:%02d", from_datetime.tm_year+1900, 
                 from_datetime.tm_mon+1, from_datetime.tm_mday, from_datetime.tm_hour, from_datetime.tm_min );
    return; //nothing to post, db_get_unposted_data returns NULL if no results
  }

  int rows_processed = 0;
  struct tm start_datetime, this_datetime;
  int more_rows = 1;
  int string_end = 0;
  char posturl[2048];
  long startOfDayWh = 0;
  int curlResult = -1;
  while( more_rows )
  {
    if( 0 == rows_processed )
    {
        start_datetime = db_row_datetime_data( row, 0  );
        // r2 service - key and sid are sent as headers, not in the url
        string_end = sprintf(posturl,"%s?data=",pvOutputUrl);
        startOfDayWh = db_get_start_of_day_energy_value(&start_datetime);
    }
    this_datetime = db_row_datetime_data( row, 0 );
    if( start_datetime.tm_mday != this_datetime.tm_mday )
    {
      startOfDayWh = db_get_start_of_day_energy_value(&this_datetime);
    }
    string_end += sprintf( posturl + string_end ,"%s,%s,%ld,%s;", db_row_string_data(row,1), db_row_string_data(row,2), db_row_int_data(row,3) - startOfDayWh, db_row_string_data(row,4)  );
    rows_processed++;

    more_rows = db_row_next( row ); //db_next_row returns 0 if we cannot move to next row in result set, 1 otherwise
    //r2 service - can process upto 30 rows at a time
    if( 30 == rows_processed || 0 == more_rows  )
    {
      //if post requires last ; to be stripped... posturl[string_end] = '\0';
      curlResult = curl_post_this_query(posturl, pvOutputKey, pvOutputSid, loglevel);
      if ( curlResult == 0 )
      {
        db_set_data_posted(&start_datetime, &this_datetime );  //date range covering possibly 1, but at most 30, values
        rows_processed = 0;
        sleep(2); //pvoutput api says we can't post more than once a second.
      } else {
          log_error( "CURL post failed, CURL result was %d",curlResult );
      }
    }
  }

  db_row_handle_free( row ); 
}



int main(int argc, char **argv)
{
    FILE *fp = 0;
    unsigned char * last_sent;
    ConfType conf;
    ReturnType *returnkeylist = NULL;
    int num_return_keys=0;
    struct sockaddr_rc addr = { 0 };
    unsigned char received[1024];
    unsigned char datarecord[1024];
    unsigned char * data;
    unsigned char send_count = 0x0;
    int return_key;
    int gap=1;
    int datalen = 0;
    int archdatalen=0;
    int failedbluetooth=0;
    int terminated=0;
    int s,i,j,status,mysql=0,post=0,repost=0,test=0,file=0,daterange=0;
    int install=0, update=0, already_read=0;
    int location=0, error=0;
    int found,crc_at_end=0, finished=0;
    int togo=0;
    int initstarted=0,setupstarted=0,rangedatastarted=0;
    long returnpos;
    int  returnline;
    char datefrom[100];
    char dateto[100];
    int  pass_i;
    char line[400];
    unsigned char address[6] = { 0 };
    unsigned char address2[6] = { 0 };
    unsigned char timestr[25] = { 0 };
    unsigned char serial[4] = { 0 };
    unsigned char tzhex[2] = { 0 };
    unsigned char timeset[4] = { 0x30,0xfe,0x7e,0x00 };
    int  invcode;
    char *lineread;
    time_t curtime;
    time_t reporttime;
    time_t fromtime;
    time_t totime;
    time_t idate;
    time_t prev_idate;
    struct tm *loctime;
    struct tm tm;
    int day,month,year,hour,minute,second;
    char tt[10] = {48,48,48,48,48,48,48,48,48,48}; 
    char ti[3];    
    char chan[1];
    float currentpower_total;
    int   rr;
    int linenum = 0;
    float dtotal;
    float gtotal;
    float ptotal;
    float strength;
    struct archdata_type
    {
        time_t date;
        char   inverter[20];
        long unsigned int serial;
        long  accum_value;
        long  current_value;
    } *archdatalist;

    char sunrise_time[6],sunset_time[6];
    loglevel_t loglevel = ll_info;
   
    log_init();
    log_info("Starting pvlogger");

    memset(received,0,1024);
    last_sent = (unsigned  char *)malloc( sizeof( unsigned char ));
    /* get the report time - used in various places */
    reporttime = time(NULL);  //get time in seconds since epoch (1/1/1970)    
    
    // set config to defaults
    InitConfig( &conf, datefrom, dateto );
    // read command arguments needed so can get config
    if( ReadCommandConfig( &conf, argc, argv, datefrom, dateto, &loglevel, 
            &skip_daylight_check, &repost, &test, &install, &update) < 0 )
        exit(0);
    // read Config file
    if( GetConfig( &conf ) < 0 )
        exit(-1);
    // read command arguments  again - they overide config
    if( ReadCommandConfig( &conf, argc, argv, datefrom, dateto, &loglevel, 
            &skip_daylight_check, &repost, &test, &install, &update) < 0 )
        exit(0);
    // Log level may have been reset by command line.
    logging_set_loglevel(logger, loglevel);

    // read Inverter Setting file
    if( GetInverterSetting( &conf ) < 0 )
        exit(-1);
    // set switches used through the program
    SetSwitches( &conf, datefrom, dateto, &location, &mysql, &post, &file, &daterange, &test );  
    
    if(( install==1 )&&( mysql==1 )) {
        db_init( conf.MySqlHost, conf.MySqlUser, conf.MySqlPwd, conf.MySqlDatabase );
        int result = db_install_tables();
        db_close();
        exit(result);
    }
    if(( update==1 )&&( mysql==1 )) {
        db_init( conf.MySqlHost, conf.MySqlUser, conf.MySqlPwd, conf.MySqlDatabase );
        int result = 0; //db_update_schema( SCHEMA_VALUE ); //TODO implement this
        db_close();
        exit(result);
    }

    db_init( conf.MySqlHost, conf.MySqlUser, conf.MySqlPwd, conf.MySqlDatabase );

    if( mysql==1 ) {
       if( db_get_schema() != SCHEMA_VALUE ) {
            log_fatal( "Please Update database schema. Use --UPDATE" );
            db_close();
            exit(-1);
       }
    }
    // Set value for inverter type
    // SetInverterType( &conf );
    // Get Return Value lookup from file
    returnkeylist = InitReturnKeys( &conf, returnkeylist, &num_return_keys );
    // Get Local Timezone offset in seconds
    get_timezone_in_seconds( tzhex );
    // Location based information to avoid quering Inverter in the dark
    if((location==1)&&(mysql==1)) {
        loctime = localtime( &curtime );
        if( !db_fetch_almanac( loctime , sunrise_time, sunset_time ) ) {
            sprintf( sunrise_time, "%s", sunrise(conf.latitude_f,conf.longitude_f ));
            sprintf( sunset_time, "%s", sunset(conf.latitude_f, conf.longitude_f ));
            db_update_almanac( loctime, sunrise_time, sunset_time );
        }
        log_verbose( "sunrise=%s sunset=%s", sunrise_time, sunset_time );
           
    }
    if(daterange==0 ) //auto set the dates
        auto_set_dates( &daterange, mysql, datefrom, dateto );
    else
        log_verbose( "QUERY RANGE    from %s to %s", datefrom, dateto ); 
    
    int isLight = is_light();
    log_verbose("is_light() =  %u",isLight );
    
    if(( daterange==1 )&&((location==0)||(mysql==0)||is_light( ))) {
        log_verbose( "Address %s",conf.BTAddress );

        if (file ==1)
            fp=fopen(conf.File,"r");
        else
            fp=fopen("/etc/sma.in","r");
        for( i=1; i<20; i++ ){
            // allocate a socket
            s = socket(AF_BLUETOOTH, SOCK_STREAM, BTPROTO_RFCOMM);

            // set the connection parameters (who to connect to)
            addr.rc_family = AF_BLUETOOTH;
            addr.rc_channel = (uint8_t) 1;
            str2ba( conf.BTAddress, &addr.rc_bdaddr );

            // connect to server
            log_debug( "datefrom=%s dateto=%s", datefrom, dateto );
            status = connect(s, (struct sockaddr *)&addr, sizeof(addr));

            if (status <0) {
                log_error( "Error connecting to %s. Errno=%i. %s",conf.BTAddress, errno, strerror( errno ) );
                close( s );
            }
            else
               break;
        }
        if (status < 0 ) {
            return( -1 );
        }

       // convert address
       address[5] = conv(strtok(conf.BTAddress,":"));
       address[4] = conv(strtok(NULL,":"));
       address[3] = conv(strtok(NULL,":"));
       address[2] = conv(strtok(NULL,":"));
       address[1] = conv(strtok(NULL,":"));
       address[0] = conv(strtok(NULL,":"));
    
        while (!feof(fp)){    
            start:
        if (fgets(line,400,fp) != NULL){                //read line from sma.in
            linenum++;
            lineread = strtok(line," ;");
            if(!strcmp(lineread,"R")){        //See if line is something we need to receive
                log_debug( "[%d] Waiting for string",linenum );
                cc = 0;
                do{
                    lineread = strtok(NULL," ;");
                    log_trace( "Read command [%s]", lineread );
                    switch(select_str(lineread)) {
        
                    case 0: // $END
                    //do nothing
                    break;            

                    case 1: // $ADDR
                    for (i=0;i<6;i++){
                        fl[cc] = address[i];
                        cc++;
                    }
                    break;    

                    case 3: // $SER
                    for (i=0;i<4;i++){
                        fl[cc] = serial[i];
                        cc++;
                    }
                    break;    
                    
                    case 7: // $ADD2
                    for (i=0;i<6;i++){
                        fl[cc] = address2[i];
                        cc++;
                    }
                    break;    

                    case 8: // $CHAN
                    fl[cc] = chan[0];
                    cc++;
                    break;

                    default :
                    fl[cc] = conv(lineread);
                    cc++;
                    break;
                    }

                } while (strcmp(lineread,"$END"));
                {
                    char buf[128];
                    snprintf(buf, 127, "[%d] waiting for", linenum);
                    hlog_debug(buf, fl, cc, 0);
                }
                log_debug("[%d] Waiting for data on rfcomm", linenum);

                found = 0;
                do {
                    if( already_read == 0 )
                        rr=0;
                    if(( already_read == 0 )&&( read_bluetooth( conf.bt_timeout, s, &rr, received, cc, last_sent, &terminated ) != 0 ))
                    {
                        already_read=0;
                        fseek( fp, returnpos, 0 );
                        linenum = returnline;
                        found=0;
                        if( archdatalen > 0 )
                           free( archdatalist );
                        archdatalen=0;
                        strcpy( lineread, "" );
                        sleep(10);
                        failedbluetooth++;
                        if( failedbluetooth > 60 )
                            exit(-1);
                        goto start;
                    }
                    else {
                      already_read=0;
                      {
                          char buf[128];
                          snprintf(buf, 127, "[%d] looking for", linenum);
                          hlog_debug(buf, fl, cc, 0);
                          snprintf(buf, 127, "[%d] received   ", linenum);
                          hlog_debug(buf, received, rr, 0);
                      }
                    }
                               
                    if (memcmp(fl+4,received+4,cc-4) == 0){
                        found = 1;
                        log_debug("[%d] Found string we are waiting for",
                                    linenum);
                    } else {
                        log_debug("[%d] Did not find string", linenum);
                    }
                } while (found == 0);
                hlog_trace("data", fl, cc, 0);
            }
            if(!strcmp(lineread,"S")){        //See if line is something we need to send
                log_debug("[%d] Sending", linenum);
                cc = 0;
                do{
                    lineread = strtok(NULL," ;");
                    switch(select_str(lineread)) {
        
                    case 0: // $END
                    //do nothing
                    break;            

                    case 1: // $ADDR
                    for (i=0;i<6;i++){
                        fl[cc] = address[i];
                        cc++;
                    }
                    break;

                    case 3: // $SER
                    for (i=0;i<4;i++){
                        fl[cc] = serial[i];
                        cc++;
                    }
                    break;    
                    

                    case 7: // $ADD2
                    for (i=0;i<6;i++){
                        fl[cc] = address2[i];
                        cc++;
                    }
                    break;

                    case 2: // $TIME    
                    // get report time and convert
                    sprintf(tt,"%x",(int)reporttime); //convert to a hex in a string
                    for (i=7;i>0;i=i-2){ //change order and convert to integer
                        ti[1] = tt[i];
                        ti[0] = tt[i-1];    
                                            ti[2] = '\0';
                        fl[cc] = conv(ti);
                        cc++;        
                    }
                    break;

                    case 11: // $TMPLUS    
                    // get report time and convert
                    sprintf(tt,"%x",(int)reporttime+1); //convert to a hex in a string
                    for (i=7;i>0;i=i-2){ //change order and convert to integer
                        ti[1] = tt[i];
                        ti[0] = tt[i-1];    
                                            ti[2] = '\0';
                        fl[cc] = conv(ti);
                        cc++;        
                    }
                    break;


                    case 10: // $TMMINUS
                    // get report time and convert
                    sprintf(tt,"%x",(int)reporttime-1); //convert to a hex in a string
                    for (i=7;i>0;i=i-2){ //change order and convert to integer
                        ti[1] = tt[i];
                        ti[0] = tt[i-1];    
                                            ti[2] = '\0';
                        fl[cc] = conv(ti);
                        cc++;        
                    }
                    break;

                    case 4: //$crc
                    tryfcs16(fl+19, cc -19);
                                    add_escapes(fl,&cc);
                                    fix_length_send(fl,&cc);
                    break;

                    case 8: // $CHAN
                    fl[cc] = chan[0];
                    cc++;
                    break;

                    case 12: // $TIMESTRING
                    for (i=0;i<25;i++){
                        fl[cc] = timestr[i];
                        cc++;
                    }
                    break;

                    case 13: // $TIMEFROM1    
                    // get report time and convert
                                    if( daterange == 1 ) {
                                        if( strptime( datefrom, "%Y-%m-%d %H:%M:%S", &tm) == 0 ) 
                                        {
                                            log_debug("datefrom %s", datefrom );
                                            log_fatal("Time Coversion Error");
                                            error=1;
                                            exit(-1);
                                        }
                                        tm.tm_isdst=-1;
                                        fromtime=mktime(&tm);
                                        if( fromtime == -1 ) {
                                        // Error we need to do something about it
                                            printf( "%03x",(int)fromtime ); getchar();
                                            printf( "\n%03lx", fromtime ); getchar();
                                            fromtime=0;
                                            printf( "bad from" ); getchar();
                                        }
                                    }
                                    else
                                    {
                                      printf( "no from" ); getchar();
                                      fromtime=0;
                                    }
                    sprintf(tt,"%03x",(int)fromtime-300); //convert to a hex in a string and start 5 mins before for dummy read.
                    for (i=7;i>0;i=i-2){ //change order and convert to integer
                        ti[1] = tt[i];
                        ti[0] = tt[i-1];    
                                            ti[2] = '\0';
                        fl[cc] = conv(ti);
                        cc++;        
                    }
                    break;

                    case 14: // $TIMETO1    
                                    if( daterange == 1 ) {
                                        if( strptime( dateto, "%Y-%m-%d %H:%M:%S", &tm) == 0 ) 
                                        {
                                            log_debug("dateto [%s]", dateto);
                                            log_fatal("Time Coversion Error");
                                            error=1;
                                            exit(-1);
                                        }
                                        tm.tm_isdst=-1;
                                        totime=mktime(&tm);
                                        if( totime == -1 ) {
                                        // Error we need to do something about it
                                            printf( "%03x",(int)totime ); getchar();
                                            printf( "\n%03lx", totime ); getchar();
                                            totime=0;
                                            printf( "bad to" ); getchar();
                                        }
                                    }
                                    else
                                      totime=0;
                    sprintf(tt,"%03x",(int)totime); //convert to a hex in a string
                    // get report time and convert
                    for (i=7;i>0;i=i-2){ //change order and convert to integer
                        ti[1] = tt[i];
                        ti[0] = tt[i-1];    
                                            ti[2] = '\0';
                        fl[cc] = conv(ti);
                        cc++;        
                    }
                    break;

                    case 15: // $TIMEFROM2    
                                    if( daterange == 1 ) {
                                        strptime( datefrom, "%Y-%m-%d %H:%M:%S", &tm);
                                        tm.tm_isdst=-1;
                                        fromtime=mktime(&tm)-86400;
                                        if( fromtime == -1 ) {
                                        // Error we need to do something about it
                                            printf( "%03x",(int)fromtime ); getchar();
                                            printf( "\n%03lx", fromtime ); getchar();
                                            fromtime=0;
                                            printf( "bad from" ); getchar();
                                        }
                                    }
                                    else
                                    {
                                      printf( "no from" ); getchar();
                                      fromtime=0;
                                    }
                    sprintf(tt,"%03x",(int)fromtime); //convert to a hex in a string
                    for (i=7;i>0;i=i-2){ //change order and convert to integer
                        ti[1] = tt[i];
                        ti[0] = tt[i-1];    
                                            ti[2] = '\0';
                        fl[cc] = conv(ti);
                        cc++;        
                    }
                    break;

                    case 16: // $TIMETO2    
                                    if( daterange == 1 ) {
                                        strptime( dateto, "%Y-%m-%d %H:%M:%S", &tm);

                                        tm.tm_isdst=-1;
                                        totime=mktime(&tm)-86400;
                                        if( totime == -1 ) {
                                        // Error we need to do something about it
                                            printf( "%03x",(int)totime ); getchar();
                                            printf( "\n%03lx", totime ); getchar();
                                            fromtime=0;
                                            printf( "bad from" ); getchar();
                                        }
                                    }
                                    else
                                      totime=0;
                    sprintf(tt,"%03x",(int)totime); //convert to a hex in a string
                    for (i=7;i>0;i=i-2){ //change order and convert to integer
                        ti[1] = tt[i];
                        ti[0] = tt[i-1];    
                                            ti[2] = '\0';
                        fl[cc] = conv(ti);
                        cc++;        
                    }
                    break;
                    
                    case 19: // $PASSWORD
                                  
                                    j=0;
                    for(i=0;i<12;i++){
                        if( conf.Password[j] == '\0' )
                                          fl[cc] = 0x88;
                                        else {
                                            pass_i = conf.Password[j];
                                            fl[cc] = (( pass_i+0x88 )%0xff);
                                            j++;
                                        }
                                        cc++;
                    }
                    break;    

                    case 21: // $UNKNOWN
                    for (i=0;i<4;i++){
                            fl[cc] = conf.InverterCode[i];
                        cc++;
                    }
                                    break;

                    case 22: // $INVCODE
                            fl[cc] = invcode;
                        cc++;
                                    break;
                    case 23: // $ARCHCODE
                            fl[cc] = conf.ArchiveCode;
                        cc++;
                                    break;
                    case 25: // $CNT send counter
                                        send_count++;
                            fl[cc] = send_count;
                        cc++;
                                    break;
                    case 26: // $TIMEZONE timezone in seconds
                            fl[cc] = tzhex[1];
                            fl[cc+1] = tzhex[0];
                        cc+=2;
                                    break;
                    case 27: // $TIMESET unknown setting
                                        for( i=0; i<4; i++ ) {
                                fl[cc] = timeset[i];
                            cc++;
                                        }
                                    break;

                    default :
                    fl[cc] = conv(lineread);
                    cc++;
                    break;
                    }

                } while (strcmp(lineread,"$END"));
                {
                    char buf[128];
                    snprintf(buf, 127, "[%d] sending", linenum);
                    hlog_debug(buf, fl, cc, 12);
                }
                last_sent = (unsigned  char *)realloc( last_sent, sizeof( unsigned char )*(cc));
                memcpy(last_sent,fl,cc);
                write(s,fl,cc);
                            already_read=0;
                            //check_send_error( &conf, &s, &rr, received, cc, last_sent, &terminated, &already_read ); 
            }


            if(!strcmp(lineread,"E")){        //See if line is something we need to extract
                log_debug("[%d] Extracting", linenum);
                cc = 0;
                do{
                    lineread = strtok(NULL," ;");
                    switch(select_str(lineread)) {

                        case 3: // Extract Serial of Inverter
                            data = ReadStream( &conf, &s, received, &rr, data, &datalen, last_sent, cc, &terminated, &togo );
                            /*
                            printf( "1.len=%d data=", datalen );
                            for( i=0; i< datalen; i++ )
                              printf( "%02x ", data[i] );
                            printf( "\n" );
                            */
                            serial[3]=data[19];
                            serial[2]=data[18];
                            serial[1]=data[17];
                            serial[0]=data[16];
                            log_verbose( "serial=%02x:%02x:%02x:%02x\n",
                                            serial[3]&0xff,serial[2]&0xff,
                                            serial[1]&0xff,serial[0]&0xff ); 
                            free( data );
                            break;
                                    
                        case 9: // extract Time from Inverter
                            idate = (received[66] * 16777216 ) + (received[65] *65536 )+ (received[64] * 256) + received[63];
                            loctime = localtime(&idate);
                            day = loctime->tm_mday;
                            month = loctime->tm_mon +1;
                            year = loctime->tm_year + 1900;
                            hour = loctime->tm_hour;
                            minute = loctime->tm_min; 
                            second = loctime->tm_sec; 
                            log_info( "Date power = %d/%d/%4d %02d:%02d:%02d",day, month, year, hour, minute,second);
#if 0
                    /* This was commented out. */
                    currentpower = (received[72] * 256) + received[71];
                    printf("Current power = %i Watt\n",currentpower);
#endif
                            break;

                        case 5: // extract current power $POW
                            data = ReadStream( &conf, &s, received, &rr, data, &datalen, last_sent, cc, &terminated, &togo );
                            if( (data+3)[0] == 0x08 )
                                gap = 40; 
                            if( (data+3)[0] == 0x10 )
                                gap = 40; 
                            if( (data+3)[0] == 0x40 )
                                gap = 28;
                            if( (data+3)[0] == 0x00 )
                                gap = 28;
                            for ( i = 0; i<datalen; i+=gap ) 
                            {
                               idate=ConvertStreamtoTime( data+i+4, 4, &idate );
                               loctime = localtime(&idate);
                               day = loctime->tm_mday;
                               month = loctime->tm_mon +1;
                               year = loctime->tm_year + 1900;
                               hour = loctime->tm_hour;
                               minute = loctime->tm_min; 
                               second = loctime->tm_sec; 
                               ConvertStreamtoFloat( data+i+8, 3, &currentpower_total );
                               return_key=-1;
                               for( j=0; j<num_return_keys; j++ )
                               {
                                  if(( (data+i+1)[0] == returnkeylist[j].key1 )&&((data+i+2)[0] == returnkeylist[j].key2)) {
                                      return_key=j;
                                      break;
                                  }
                               }
                               if( return_key >= 0 )
                                   log_info("%d-%02d-%02d %02d:%02d:%02d %-20s = %.0f %-20s", year, month, day, hour, minute, second,
                                             returnkeylist[return_key].description, currentpower_total/returnkeylist[return_key].divisor, 
                                             returnkeylist[return_key].units );
                               else
                                   log_info("%d-%02d-%02d %02d:%02d:%02d NO DATA for %02x %02x = %.0f NO UNITS", year, month, day, hour,
                                             minute, second, (data+i+1)[0], (data+i+1)[1], currentpower_total );
                            }
                            free( data );
                            break;

                        case 6: // extract total energy collected today
                            gtotal = (received[69] * 65536) + (received[68] * 256) + received[67];
                            gtotal = gtotal / 1000;
                            log_info("G total so far = %.2f kWh",gtotal);

                            dtotal = (received[84] * 256) + received[83];
                            dtotal = dtotal / 1000;
                            log_info("E total today = %.2f kWh",dtotal);
                            break;        

                        case 7: // extract 2nd address
                            memcpy(address2,received+26,6);
                            log_debug("address 2");
                            break;
                    
                        case 8: // extract bluetooth channel
                            memcpy(chan,received+22,1);
                            log_debug("Bluetooth channel [%i]", chan[0]);
                            break;

                        case 12: // extract time strings $TIMESTRING
                            if(( received[60] == 0x6d )&&( received[61] == 0x23 ))
                            {
                                memcpy(timestr,received+63,24);
                                log_debug("extracting timestring");
                                memcpy(timeset,received+79,4);
                                idate=ConvertStreamtoTime( received+63,4, &idate );
                                /* Allow delay for inverter to be slow */
                                if( reporttime > idate ) {
                                   log_debug("delay [5 seconds]");
                                   sleep( 5 );
                                }
                            }
                            else
                            {
                                memcpy(timestr,received+63,24);
                                log_debug("bad extracting timestring");
                                already_read=0;
                                fseek( fp, returnpos, 0 );
                                linenum = returnline;
                                found=0;
                                if( archdatalen > 0 )
                                   free( archdatalist );
                                archdatalen=0;
                                strcpy( lineread, "" );
                                failedbluetooth++;
                                if( failedbluetooth > 10 )
                                    exit(-1);
                                goto start;
                                //exit(-1);
                            }
                                    
                            break;

                    case 17: // Test data
                            data = ReadStream( &conf, &s, received, &rr, data, &datalen, last_sent, cc, &terminated, &togo );
                            // printf( "\n" );
                      
                            free( data );
                            break;
                    
                    case 18: // $ARCHIVEDATA1
                        finished=0;
                        ptotal=0;
                        idate=0;
                        // printf( "\n" );
                        while( finished != 1 ) {
                            data = ReadStream( &conf, &s, received, &rr, data, &datalen, last_sent, cc, &terminated, &togo );

                            j=0;
                            for( i=0; i<datalen; i++ )
                            {
                               datarecord[j]=data[i];
                               j++;
                               if( j > 11 ) {
                                 if( idate > 0 ) prev_idate=idate;
                                 else prev_idate=0;
                                 idate=ConvertStreamtoTime( datarecord, 4, &idate );
                                 if( prev_idate == 0 )
                                    prev_idate = idate-300;

                                 loctime = localtime(&idate);
                                 day = loctime->tm_mday;
                                 month = loctime->tm_mon +1;
                                 year = loctime->tm_year + 1900;
                                 hour = loctime->tm_hour;
                                 minute = loctime->tm_min; 
                                 second = loctime->tm_sec; 
                                 ConvertStreamtoFloat( datarecord+4, 8, &gtotal );
                                 if(archdatalen == 0 )
                                    ptotal = gtotal;
                                 log_info("%d/%d/%4d %02d:%02d:%02d  total=%.3f Kwh current=%.0f Watts togo=%d i=%d crc=%d", day,
                                          month, year, hour, minute,second, gtotal/1000, (gtotal-ptotal)*12, togo, i, crc_at_end);
                                 if( idate != prev_idate+300 ) {
                                    log_error("Date Error! prev=%d current=%d\n", (int)prev_idate, (int)idate );
                                    error=1;
                                    break;
                                 }
                                 if( archdatalen == 0 )
                                    archdatalist = (struct archdata_type *)malloc( sizeof( struct archdata_type ) );
                                 else
                                    archdatalist = (struct archdata_type *)realloc( archdatalist, sizeof( struct archdata_type )*(archdatalen+1));
                                 (archdatalist+archdatalen)->date=idate;
                                 strcpy((archdatalist+archdatalen)->inverter,conf.Inverter);
                                 ConvertStreamtoLong( serial, 4, &(archdatalist+archdatalen)->serial);
                                 (archdatalist+archdatalen)->accum_value=gtotal;
                                 (archdatalist+archdatalen)->current_value=(gtotal-ptotal)*12;
                                 archdatalen++;
                                 ptotal=gtotal;
                                 j=0; //get ready for another record
                              }
                           }
                           if( togo == 0 ) 
                              finished=1;
                           else
                              if( read_bluetooth( conf.bt_timeout, s, &rr, received, cc, last_sent, &terminated ) != 0 )
                              {
                                 fseek( fp, returnpos, 0 );
                                 linenum = returnline;
                                 found=0;
                                 if( archdatalen > 0 )
                                    free( archdatalist );
                                 archdatalen=0;
                                 strcpy( lineread, "" );
                                 sleep(10);
                                 failedbluetooth++;
                                 if( failedbluetooth > 3 )
                                   exit(-1);
                                 goto start;
                              }
                        }
                        free( data );
                        // printf( "\n" );
                                  
                        break;
                    case 20: // SIGNAL signal strength
                        strength  = (received[22] * 100.0)/0xff;
                        log_verbose("bluetooth signal [%.0f%%]",strength);
                        break;        

                    case 22: // extract time strings $INVCODE
                        invcode=received[22];
                        log_debug("extracting invcode [%02x]", invcode);
                        break;

                    case 24: // Inverter data $INVERTERDATA
                            data = ReadStream( &conf, &s, received, &rr, data, &datalen, last_sent, cc, &terminated, &togo );
                            log_debug( "data=%02x",(data+3)[0] );
                            if( (data+3)[0] == 0x08 )
                                gap = 40; 
                            if( (data+3)[0] == 0x10 )
                                gap = 40; 
                            if( (data+3)[0] == 0x40 )
                                gap = 28;
                            if( (data+3)[0] == 0x00 )
                                gap = 28;
                            for ( i = 0; i<datalen; i+=gap ) 
                            {
                               idate=ConvertStreamtoTime( data+i+4, 4, &idate );
                               loctime = localtime(&idate);
                               day = loctime->tm_mday;
                               month = loctime->tm_mon +1;
                               year = loctime->tm_year + 1900;
                               hour = loctime->tm_hour;
                               minute = loctime->tm_min; 
                               second = loctime->tm_sec; 
                               ConvertStreamtoFloat( data+i+8, 3, &currentpower_total );
                               return_key=-1;
                               for( j=0; j<num_return_keys; j++ )
                               {
                                  if(( (data+i+1)[0] == returnkeylist[j].key1 )&&((data+i+2)[0] == returnkeylist[j].key2)) {
                                      return_key=j;
                                      break;
                                  }
                               }
                               if( return_key >= 0 ) {
                                   if( i==0 )
                                       log_info("%d-%02d-%02d  %02d:%02d:%02d %s", year, month, day, hour, minute, second, (data+i+8) );
                                    log_info("%d-%02d-%02d %02d:%02d:%02d %-20s = %.0f %-20s", year, month, day, hour, minute, second,
                                             returnkeylist[return_key].description, currentpower_total/returnkeylist[return_key].divisor, 
                                             returnkeylist[return_key].units );
                               }
                               else
                                   log_info("%d-%02d-%02d %02d:%02d:%02d NO DATA for %02x %02x = %.0f NO UNITS", 
                                             year, month, day, hour, minute, second, (data+i+1)[0], (data+i+1)[0], currentpower_total );
                            }
                            free( data );
                    break;
                    }                
                }
                    
                while (strcmp(lineread,"$END"));
            } 
            if(!strcmp(lineread,":init")){        //See if line is something we need to extract
                       initstarted=1;
                       returnpos=ftell(fp);
               returnline = linenum;
                    }
            if(!strcmp(lineread,":setup")){        //See if line is something we need to extract
                       setupstarted=1;
                       returnpos=ftell(fp);
               returnline = linenum;
                    }
            if(!strcmp(lineread,":startsetup")){        //See if line is something we need to extract
                       sleep(1);
                    }
            if(!strcmp(lineread,":setinverter1")){        //See if line is something we need to extract
                       setupstarted=1;
                       returnpos=ftell(fp);
               returnline = linenum;
                    }
            if(!strcmp(lineread,":getrangedata")){        //See if line is something we need to extract
                       rangedatastarted=1;
                       returnpos=ftell(fp);
               returnline = linenum;
            }
        }
    }

    if ((mysql ==1)&&(error==0)){
    for( i=1; i<archdatalen; i++ ) //Start at 1 as the first record is a dummy
        {
      struct tm *interval;
      interval = localtime( &((archdatalist+i)->date) );
      db_set_interval_value( interval, (archdatalist+i)->inverter, (archdatalist+i)->serial, (archdatalist+i)->current_value, (archdatalist+i)->accum_value );
        }
    }
 
    close(s);
    if( archdatalen > 0 )
    free( archdatalist );
    archdatalen=0;
    free(last_sent);
    if ((post ==1)&&(mysql==1)&&(error==0)){
      post_interval_data( conf.PVOutputURL, conf.PVOutputKey, conf.PVOutputSid, repost, datefrom, dateto, loglevel);
    }

}
  db_close();
  /* Clean up memory alloc. */
  free(returnkeylist);

  return 0;
}
