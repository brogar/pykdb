/*  C implementation for the date/time type documented at
 *  http://www.zope.org/Members/fdrake/DateTimeWiki/FrontPage
 */

#define PY_SSIZE_T_CLEAN

#include "Python.h"

#include <time.h>


/* We require that C int be at least 32 bits, and use int virtually
 * everywhere.  In just a few cases we use a temp long, where a Python
 * API returns a C long.  In such cases, we have to ensure that the
 * final result fits in a C int (this can be an issue on 64-bit boxes).
 */
#if SIZEOF_INT < 4
#       error "datetime.c requires that C int have at least 32 bits"
#endif

#define MINYEAR 1
#define MAXYEAR 9999
#define MAXORDINAL 3652059 /* date(9999,12,31).toordinal() */

/* Nine decimal digits is easy to communicate, and leaves enough room
 * so that two delta days can be added w/o fear of overflowing a signed
 * 32-bit int, and with plenty of room left over to absorb any possible
 * carries from adding seconds.
 */
#define MAX_DELTA_DAYS 999999999


/* M is a char or int claiming to be a valid month.  The macro is equivalent
 * to the two-sided Python test
 *      1 <= M <= 12
 */
#define MONTH_IS_SANE(M) ((unsigned int)(M) - 1 < 12)


/* ---------------------------------------------------------------------------
 * Math utilities.
 */

/* k = i+j overflows iff k differs in sign from both inputs,
 * iff k^i has sign bit set and k^j has sign bit set,
 * iff (k^i)&(k^j) has sign bit set.
 */
#define SIGNED_ADD_OVERFLOWED(RESULT, I, J) \
    ((((RESULT) ^ (I)) & ((RESULT) ^ (J))) < 0)

/* Compute Python divmod(x, y), returning the quotient and storing the
 * remainder into *r.  The quotient is the floor of x/y, and that's
 * the real point of this.  C will probably truncate instead (C99
 * requires truncation; C89 left it implementation-defined).
 * Simplification:  we *require* that y > 0 here.  That's appropriate
 * for all the uses made of it.  This simplifies the code and makes
 * the overflow case impossible (divmod(LONG_MIN, -1) is the only
 * overflow case).
 */
static int
divmod(int x, int y, int *r)
{
    int quo;

    assert(y > 0);
    quo = x / y;
    *r = x - quo * y;
    if (*r < 0) {
        --quo;
        *r += y;
    }
    assert(0 <= *r && *r < y);
    return quo;
}

/* Round a double to the nearest long.  |x| must be small enough to fit
 * in a C long; this is not checked.
 */
static long
round_to_long(double x)
{
    if (x >= 0.0)
        x = floor(x + 0.5);
    else
        x = ceil(x - 0.5);
    return (long)x;
}

/* ---------------------------------------------------------------------------
 * General calendrical helper functions
 */

/* For each month ordinal in 1..12, the number of days in that month,
 * and the number of days before that month in the same year.  These
 * are correct for non-leap years only.
 */
static int _days_in_month[] = {
    0, /* unused; this vector uses 1-based indexing */
    31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31
};

static int _days_before_month[] = {
    0, /* unused; this vector uses 1-based indexing */
    0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334
};

/* year -> 1 if leap year, else 0. */
static int
is_leap(int year)
{
    /* Cast year to unsigned.  The result is the same either way, but
     * C can generate faster code for unsigned mod than for signed
     * mod (especially for % 4 -- a good compiler should just grab
     * the last 2 bits when the LHS is unsigned).
     */
    const unsigned int ayear = (unsigned int)year;
    return ayear % 4 == 0 && (ayear % 100 != 0 || ayear % 400 == 0);
}

/* year, month -> number of days in that month in that year */
static int
days_in_month(int year, int month)
{
    assert(month >= 1);
    assert(month <= 12);
    if (month == 2 && is_leap(year))
        return 29;
    else
        return _days_in_month[month];
}

/* year, month -> number of days in year preceeding first day of month */
static int
days_before_month(int year, int month)
{
    int days;

    assert(month >= 1);
    assert(month <= 12);
    days = _days_before_month[month];
    if (month > 2 && is_leap(year))
        ++days;
    return days;
}

/* year -> number of days before January 1st of year.  Remember that we
 * start with year 1, so days_before_year(1) == 0.
 */
static int
days_before_year(int year)
{
    int y = year - 1;
    /* This is incorrect if year <= 0; we really want the floor
     * here.  But so long as MINYEAR is 1, the smallest year this
     * can see is 0 (this can happen in some normalization endcases),
     * so we'll just special-case that.
     */
    assert (year >= 0);
    if (y >= 0)
        return y*365 + y/4 - y/100 + y/400;
    else {
        assert(y == -1);
        return -366;
    }
}

/* Number of days in 4, 100, and 400 year cycles.  That these have
 * the correct values is asserted in the module init function.
 */
#define DI4Y    1461    /* days_before_year(5); days in 4 years */
#define DI100Y  36524   /* days_before_year(101); days in 100 years */
#define DI400Y  146097  /* days_before_year(401); days in 400 years  */

/* ordinal -> year, month, day, considering 01-Jan-0001 as day 1. */
static void
ord_to_ymd(int ordinal, int *year, int *month, int *day)
{
    int n, n1, n4, n100, n400, leapyear, preceding;

    /* ordinal is a 1-based index, starting at 1-Jan-1.  The pattern of
     * leap years repeats exactly every 400 years.  The basic strategy is
     * to find the closest 400-year boundary at or before ordinal, then
     * work with the offset from that boundary to ordinal.  Life is much
     * clearer if we subtract 1 from ordinal first -- then the values
     * of ordinal at 400-year boundaries are exactly those divisible
     * by DI400Y:
     *
     *    D  M   Y            n              n-1
     *    -- --- ----        ----------     ----------------
     *    31 Dec -400        -DI400Y       -DI400Y -1
     *     1 Jan -399         -DI400Y +1   -DI400Y      400-year boundary
     *    ...
     *    30 Dec  000        -1             -2
     *    31 Dec  000         0             -1
     *     1 Jan  001         1              0          400-year boundary
     *     2 Jan  001         2              1
     *     3 Jan  001         3              2
     *    ...
     *    31 Dec  400         DI400Y        DI400Y -1
     *     1 Jan  401         DI400Y +1     DI400Y      400-year boundary
     */
    assert(ordinal >= 1);
    --ordinal;
    n400 = ordinal / DI400Y;
    n = ordinal % DI400Y;
    *year = n400 * 400 + 1;

    /* Now n is the (non-negative) offset, in days, from January 1 of
     * year, to the desired date.  Now compute how many 100-year cycles
     * precede n.
     * Note that it's possible for n100 to equal 4!  In that case 4 full
     * 100-year cycles precede the desired day, which implies the
     * desired day is December 31 at the end of a 400-year cycle.
     */
    n100 = n / DI100Y;
    n = n % DI100Y;

    /* Now compute how many 4-year cycles precede it. */
    n4 = n / DI4Y;
    n = n % DI4Y;

    /* And now how many single years.  Again n1 can be 4, and again
     * meaning that the desired day is December 31 at the end of the
     * 4-year cycle.
     */
    n1 = n / 365;
    n = n % 365;

    *year += n100 * 100 + n4 * 4 + n1;
    if (n1 == 4 || n100 == 4) {
        assert(n == 0);
        *year -= 1;
        *month = 12;
        *day = 31;
        return;
    }

    /* Now the year is correct, and n is the offset from January 1.  We
     * find the month via an estimate that's either exact or one too
     * large.
     */
    leapyear = n1 == 3 && (n4 != 24 || n100 == 3);
    assert(leapyear == is_leap(*year));
    *month = (n + 50) >> 5;
    preceding = (_days_before_month[*month] + (*month > 2 && leapyear));
    if (preceding > n) {
        /* estimate is too large */
        *month -= 1;
        preceding -= days_in_month(*year, *month);
    }
    n -= preceding;
    assert(0 <= n);
    assert(n < days_in_month(*year, *month));

    *day = n + 1;
}

/* year, month, day -> ordinal, considering 01-Jan-0001 as day 1. */
static int
ymd_to_ord(int year, int month, int day)
{
    return days_before_year(year) + days_before_month(year, month) + day;
}

/* Day of week, where Monday==0, ..., Sunday==6.  1/1/1 was a Monday. */
static int
weekday(int year, int month, int day)
{
    return (ymd_to_ord(year, month, day) + 6) % 7;
}

/* Ordinal of the Monday starting week 1 of the ISO year.  Week 1 is the
 * first calendar week containing a Thursday.
 */
static int
iso_week1_monday(int year)
{
    int first_day = ymd_to_ord(year, 1, 1);     /* ord of 1/1 */
    /* 0 if 1/1 is a Monday, 1 if a Tue, etc. */
    int first_weekday = (first_day + 6) % 7;
    /* ordinal of closest Monday at or before 1/1 */
    int week1_monday  = first_day - first_weekday;

    if (first_weekday > 3)      /* if 1/1 was Fri, Sat, Sun */
        week1_monday += 7;
    return week1_monday;
}

/* ---------------------------------------------------------------------------
 * Range checkers.
 */

/* Check that -MAX_DELTA_DAYS <= days <= MAX_DELTA_DAYS.  If so, return 0.
 * If not, raise OverflowError and return -1.
 */
static int
check_delta_day_range(int days)
{
    if (-MAX_DELTA_DAYS <= days && days <= MAX_DELTA_DAYS)
        return 0;
    PyErr_Format(PyExc_OverflowError,
                 "days=%d; must have magnitude <= %d",
                 days, MAX_DELTA_DAYS);
    return -1;
}

/* Check that date arguments are in range.  Return 0 if they are.  If they
 * aren't, raise ValueError and return -1.
 */
static int
check_date_args(int year, int month, int day)
{

    if (year < MINYEAR || year > MAXYEAR) {
        PyErr_SetString(PyExc_ValueError,
                        "year is out of range");
        return -1;
    }
    if (month < 1 || month > 12) {
        PyErr_SetString(PyExc_ValueError,
                        "month must be in 1..12");
        return -1;
    }
    if (day < 1 || day > days_in_month(year, month)) {
        PyErr_SetString(PyExc_ValueError,
                        "day is out of range for month");
        return -1;
    }
    return 0;
}

/* Check that time arguments are in range.  Return 0 if they are.  If they
 * aren't, raise ValueError and return -1.
 */
/*static*/ int
check_time_args(int h, int m, int s, int us)
{
    if (h < 0 || h > 23) {
        PyErr_SetString(PyExc_ValueError,
                        "hour must be in 0..23");
        return -1;
    }
    if (m < 0 || m > 59) {
        PyErr_SetString(PyExc_ValueError,
                        "minute must be in 0..59");
        return -1;
    }
    if (s < 0 || s > 59) {
        PyErr_SetString(PyExc_ValueError,
                        "second must be in 0..59");
        return -1;
    }
    if (us < 0 || us > 999999) {
        PyErr_SetString(PyExc_ValueError,
                        "microsecond must be in 0..999999");
        return -1;
    }
    return 0;
}

/* ---------------------------------------------------------------------------
 * Normalization utilities.
 */

/* One step of a mixed-radix conversion.  A "hi" unit is equivalent to
 * factor "lo" units.  factor must be > 0.  If *lo is less than 0, or
 * at least factor, enough of *lo is converted into "hi" units so that
 * 0 <= *lo < factor.  The input values must be such that int overflow
 * is impossible.
 */
/*static*/ void
normalize_pair(int *hi, int *lo, int factor)
{
    assert(factor > 0);
    assert(lo != hi);
    if (*lo < 0 || *lo >= factor) {
        const int num_hi = divmod(*lo, factor, lo);
        const int new_hi = *hi + num_hi;
        assert(! SIGNED_ADD_OVERFLOWED(new_hi, *hi, num_hi));
        *hi = new_hi;
    }
    assert(0 <= *lo && *lo < factor);
}

/* Fiddle days (d), seconds (s), and microseconds (us) so that
 *      0 <= *s < 24*3600
 *      0 <= *us < 1000000
 * The input values must be such that the internals don't overflow.
 * The way this routine is used, we don't get close.
 */
static void
normalize_d_s_us(int *d, int *s, int *us)
{
    if (*us < 0 || *us >= 1000000) {
        normalize_pair(s, us, 1000000);
        /* |s| can't be bigger than about
         * |original s| + |original us|/1000000 now.
         */

    }
    if (*s < 0 || *s >= 24*3600) {
        normalize_pair(d, s, 24*3600);
        /* |d| can't be bigger than about
         * |original d| +
         * (|original s| + |original us|/1000000) / (24*3600) now.
         */
    }
    assert(0 <= *s && *s < 24*3600);
    assert(0 <= *us && *us < 1000000);
}

/* Fiddle years (y), months (m), and days (d) so that
 *      1 <= *m <= 12
 *      1 <= *d <= days_in_month(*y, *m)
 * The input values must be such that the internals don't overflow.
 * The way this routine is used, we don't get close.
 */
static int
normalize_y_m_d(int *y, int *m, int *d)
{
    int dim;            /* # of days in month */

    /* This gets muddy:  the proper range for day can't be determined
     * without knowing the correct month and year, but if day is, e.g.,
     * plus or minus a million, the current month and year values make
     * no sense (and may also be out of bounds themselves).
     * Saying 12 months == 1 year should be non-controversial.
     */
    if (*m < 1 || *m > 12) {
        --*m;
        normalize_pair(y, m, 12);
        ++*m;
        /* |y| can't be bigger than about
         * |original y| + |original m|/12 now.
         */
    }
    assert(1 <= *m && *m <= 12);

    /* Now only day can be out of bounds (year may also be out of bounds
     * for a datetime object, but we don't care about that here).
     * If day is out of bounds, what to do is arguable, but at least the
     * method here is principled and explainable.
     */
    dim = days_in_month(*y, *m);
    if (*d < 1 || *d > dim) {
        /* Move day-1 days from the first of the month.  First try to
         * get off cheap if we're only one day out of range
         * (adjustments for timezone alone can't be worse than that).
         */
        if (*d == 0) {
            --*m;
            if (*m > 0)
                *d = days_in_month(*y, *m);
            else {
                --*y;
                *m = 12;
                *d = 31;
            }
        }
        else if (*d == dim + 1) {
            /* move forward a day */
            ++*m;
            *d = 1;
            if (*m > 12) {
                *m = 1;
                ++*y;
            }
        }
        else {
            int ordinal = ymd_to_ord(*y, *m, 1) +
                                      *d - 1;
            if (ordinal < 1 || ordinal > MAXORDINAL) {
                goto error;
            } else {
                ord_to_ymd(ordinal, y, m, d);
                return 0;
            }
        }
    }
    assert(*m > 0);
    assert(*d > 0);
    if (MINYEAR <= *y && *y <= MAXYEAR)
        return 0;
 error:
    PyErr_SetString(PyExc_OverflowError,
            "date value out of range");
    return -1;

}

/* Fiddle out-of-bounds months and days so that the result makes some kind
 * of sense.  The parameters are both inputs and outputs.  Returns < 0 on
 * failure, where failure means the adjusted year is out of bounds.
 */
/*static*/ int
normalize_date(int *year, int *month, int *day)
{
    return normalize_y_m_d(year, month, day);
}

/* Force all the datetime fields into range.  The parameters are both
 * inputs and outputs.  Returns < 0 on error.
 */
/*static */int
normalize_datetime(int *year, int *month, int *day,
                   int *hour, int *minute, int *second,
                   int *microsecond)
{
    normalize_pair(second, microsecond, 1000000);
    normalize_pair(minute, second, 60);
    normalize_pair(hour, minute, 60);
    normalize_pair(day, hour, 24);
    return normalize_date(year, month, day);
}

