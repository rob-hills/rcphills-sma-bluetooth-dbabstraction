#ifndef _MINUNIT_H
#define _MINUNIT_H
/*
MinUnit unit testing "framework"
Copyright Jera Design
http://www.jera.com/techinfo/jtns/jtn002.html

Additional assertions Copyright Tony Brown 2011

License:
You may use the code in this tech note for any purpose, with the understanding that it comes with NO WARRANTY. 
*/

#include <stdio.h>
#include <string.h>

char *t_name;
char *t_test;
char t_expected[1000];
#define STR(x) #x
#define mu_assert(message, test) do { t_test=STR(Expected test); if (!(test)) return message; } while (0)
#define mu_assert_equal_int(expected, result) do { t_test=mu_priv_assert_equal_l(expected,result); if(t_test) return t_test; } while (0)
#define mu_assert_equal_string(expected, result) do { t_test=mu_priv_assert_equal_s(expected,result); if(t_test) return t_test; } while (0)
#define mu_run_test(test) do { t_name=STR(test); char *message = test(); tests_run++; \
                                if (message) return message; } while (0)
extern int tests_run;

char *mu_priv_assert_equal_l( const long expected, const long result )
{
  if (expected != result )
  {
    sprintf( t_expected, "Expected \"%ld\", got \"%ld\"  ", expected, result );
    return t_expected;
  }
  return 0;
}

char *mu_priv_assert_equal_s(const char *expected,const char *result )
{
  if (strcmp(expected,result) != 0 )
  {
    sprintf( t_expected, "Expected \"%s\", got \"%s\"  ", expected, result );
    return t_expected;
  }
  return 0;
}


#endif