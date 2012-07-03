#include "logging.h"

#include <stdio.h>

/* Print in hex format.
   This prints the address, the byte in hex and also the printable
   chars in ASCII.
*/
#define HEXDUMP_LINE_LEN 16
/* Limit plus some additional. */
unsigned long const HEXDUMP_MAX_LINE_LEN = HEXDUMP_LINE_LEN*4+10+5;

static char const *
logging_hex_one_line(char * buf,
                 char const * const data,
                 unsigned long const line_start_addr,
                 char const * const data_cur,
                 char const * const data_start,
                 char const * const data_end)
{
        char const * const line_end = data_cur + HEXDUMP_LINE_LEN;
        char const * dcur = data_cur;
        size_t size = 0;

        /* Print address. */
        size += snprintf(buf + size, HEXDUMP_MAX_LINE_LEN - size,
                        "%08lx: ", line_start_addr);

        /* Print hex part. */
        for(unsigned long line_offset = 0 ; dcur<line_end;
                        ++dcur, ++line_offset) {
      if(data_start<=dcur && dcur<data_end) {
          size += snprintf(buf + size, HEXDUMP_MAX_LINE_LEN - size,
                           "%02x", (unsigned char)*dcur);
      } else {
          size += snprintf(buf + size, HEXDUMP_MAX_LINE_LEN - size, "  ");
      }
      if(line_offset%2==1)
          size += snprintf(buf + size, HEXDUMP_MAX_LINE_LEN - size, " ");
   }

   size += snprintf(buf + size, HEXDUMP_MAX_LINE_LEN - size, " ");

   /* Print ASCII part. */
   dcur = data_cur;
   for(unsigned long line_offset = 0 ; dcur<line_end;
                   ++dcur, ++line_offset)
   {
           if(data_start<=dcur && dcur<data_end) {
               unsigned char const udata = (unsigned char)*dcur;
               if(32<=udata && udata<127)
                   size += snprintf(buf + size, HEXDUMP_MAX_LINE_LEN - size,
                                        "%c", udata);
               else
                   size += snprintf(buf + size, HEXDUMP_MAX_LINE_LEN - size, ".");
           } else {
               size += snprintf(buf + size, HEXDUMP_MAX_LINE_LEN - size, " ");
           }
   }
   return dcur;
}

static void
logging_hex_with_offset(logging_p self, loglevel_t level,
                char const * const desc, char const * const data,
                unsigned long line_start_addr,
                char const * const odata,
                char const * const data_start,
                char const * const data_end)
{
        char const * data_cur = odata;
        char buf[HEXDUMP_MAX_LINE_LEN + 1];
        do
        {
                data_cur = logging_hex_one_line(buf, data, line_start_addr,
                                data_cur, data_start, data_end);
                line_start_addr += HEXDUMP_LINE_LEN;
                logging_generic(self, level, "%s %s", buf, desc);
        } while(data_cur < data_end);
}

void logging_hex(logging_p self, loglevel_t level, char const * const desc,
                void const * const vdata, unsigned long const len,
                unsigned long const offset)
{
        if(self->loglevel>level) return;

        char const * const data = (char const * const)vdata;
        unsigned long line_start_addr
           = offset / HEXDUMP_LINE_LEN * HEXDUMP_LINE_LEN;
    char const * const data_end = data + len;
    unsigned long const offset_neg
       = (offset % HEXDUMP_LINE_LEN == 0 )
         ? 0
         : offset % HEXDUMP_LINE_LEN;

    logging_hex_with_offset(self, level, desc, data, line_start_addr,
                        data-offset_neg, data, data_end);
}

#if 0
int main()
{
   char const t1[] = "Hello World! This is a buffer which is longer than 16.";
   char const t2[] = "Shorter than 16";
   char const t3[] = "Exactly 16 bytes";
   for(int i=0; i<20; ++i) {
      printf("+++ Offset %d +++\n", i);
      hexdump(t1, strlen(t1), i);
      hexdump(t2, strlen(t2), i);
      hexdump(t3, strlen(t3), i);
   }
}
#endif

