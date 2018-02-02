/* -----------------------------------------------------------------------------
 * inttypes.i
 *
 * SWIG library file  for ISO C99 types: 7.8 Format conversion of integer types <inttypes.h>
 * ----------------------------------------------------------------------------- */

%{
#include <inttypes.h>
%}

%include <stdint.i>
%include <wchar.i>

#ifdef __cplusplus
extern "C" {
#endif

#ifdef SWIGWORDSIZE64
  
  /* We have to define the `uintmax_t' type using `ldiv_t'.  */
  typedef struct
  {
    long int quot;		/* Quotient.  */
    long int rem;		/* Remainder.  */
  } imaxdiv_t;
  
#else
  
  /* We have to define the `uintmax_t' type using `lldiv_t'.  */
  typedef struct
  {
    long long int quot;		/* Quotient.  */
    long long int rem;		/* Remainder.  */
  } imaxdiv_t;

#endif

  /* Compute absolute value of N.  */
  extern intmax_t imaxabs (intmax_t n);

  /* Return the `imaxdiv_t' representation of the value of NUMER over DENOM. */
  extern imaxdiv_t imaxdiv (intmax_t numer, intmax_t denom);
  
#ifdef SWIG_WCHAR
  /* Like `wcstol' but convert to `intmax_t'.  */
  extern intmax_t wcstoimax (const wchar_t *nptr, wchar_t **endptr, int base);
  
  /* Like `wcstoul' but convert to `uintmax_t'.  */
  extern uintmax_t wcstoumax (const wchar_t *nptr, wchar_t ** endptr, int base);
#endif

#ifdef SWIGWORDSIZE64
  
  /* Like `strtol' but convert to `intmax_t'.  */
  extern  intmax_t strtoimax (const char *nptr, char **endptr, int base);
  
  /* Like `strtoul' but convert to `uintmax_t'.  */
  extern  uintmax_t strtoumax (const char *nptr, char **endptr,int base);
  
#ifdef SWIG_WCHAR
  /* Like `wcstol' but convert to `intmax_t'.  */
  extern  intmax_t wcstoimax (const wchar_t *nptr, wchar_t **endptr, int base);
  
  /* Like `wcstoul' but convert to `uintmax_t'.  */
  extern  uintmax_t wcstoumax (const wchar_t *nptr, wchar_t **endptr, int base);
#endif
  
#else /* SWIGWORDSIZE32 */
  
  /* Like `strtol' but convert to `intmax_t'.  */
  extern  intmax_t strtoimax (const char *nptr, char **endptr, int base);
  
  /* Like `strtoul' but convert to `uintmax_t'.  */
  extern  uintmax_t strtoumax (const char *nptr, char **endptr, int base);
  
#ifdef SWIG_WCHAR
  /* Like `wcstol' but convert to `intmax_t'.  */
  extern  uintmax_t wcstoumax (const wchar_t *nptr, wchar_t **endptr, int base);
#endif

#endif /* SWIGWORDSIZE64 */

#ifdef __cplusplus
}
#endif
