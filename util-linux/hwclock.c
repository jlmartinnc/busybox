/* vi: set sw=4 ts=4: */
/*
 * really dumb hwclock implementation for busybox
 * Copyright (C) 2002 Robert Griebl <griebl@gmx.de>
 *
 */


#include <sys/utsname.h>
#include <stdlib.h>
#include <unistd.h>
#include <syslog.h>
#include <string.h>
#include <ctype.h>
#include <fcntl.h>
#include <sys/time.h>
#include <time.h>
#include <linux/rtc.h>
#include <sys/ioctl.h>
#include "busybox.h"

#ifdef CONFIG_FEATURE_HWCLOCK_LONGOPTIONS
# ifndef _GNU_SOURCE
#  define _GNU_SOURCE
# endif
#endif

#include <getopt.h>


enum OpMode {
	SHOW,
	SYSTOHC,
	HCTOSYS
};


time_t read_rtc ( int utc )
{
	int rtc;
	struct tm tm;
	char *oldtz = 0;
	time_t t = 0;

	if (( rtc = open ( "/dev/rtc", O_RDONLY )) < 0 ) {
		if (( rtc = open ( "/dev/misc/rtc", O_RDONLY )) < 0 )
			perror_msg_and_die ( "Could not access RTC" );
	}
 	memset ( &tm, 0, sizeof( struct tm ));
	if ( ioctl ( rtc, RTC_RD_TIME, &tm ) < 0 )
		perror_msg_and_die ( "Could not read time from RTC" );
	tm. tm_isdst = -1; // not known
	
	close ( rtc );

	if ( utc ) {	
		oldtz = getenv ( "TZ" );
		setenv ( "TZ", "UTC 0", 1 );
		tzset ( );
	}
	
	t = mktime ( &tm );
	
	if ( utc ) {
		if ( oldtz )
			setenv ( "TZ", oldtz, 1 );
		else
			unsetenv ( "TZ" );
		tzset ( );
	}
	return t;
}

void write_rtc ( time_t t, int utc )
{
	int rtc;
	struct tm tm;

	if (( rtc = open ( "/dev/rtc", O_WRONLY )) < 0 ) {
		if (( rtc = open ( "/dev/misc/rtc", O_WRONLY )) < 0 )
			perror_msg_and_die ( "Could not access RTC" );
	}
 	
 	printf ( "1\n" );
 	
	tm = *( utc ? gmtime ( &t ) : localtime ( &t ));
	tm. tm_isdst = 0;
 	
 	printf ( "2\n") ;
 	
	if ( ioctl ( rtc, RTC_SET_TIME, &tm ) < 0 )
		perror_msg_and_die ( "Could not set the RTC time" );
	
	close ( rtc );
}

int show_clock ( int utc )
{
	struct tm *ptm;
	time_t t;
	char buffer [64];

	t = read_rtc ( utc );		
	ptm = localtime ( &t );  /* Sets 'tzname[]' */
	
	safe_strncpy ( buffer, ctime ( &t ), sizeof( buffer ));
	if ( buffer [0] )
		buffer [xstrlen ( buffer ) - 1] = 0;
	
	//printf ( "%s  %.6f seconds %s\n", buffer, 0.0, utc ? "" : ( ptm-> tm_isdst ? tzname [1] : tzname [0] ));
	printf ( "%s  %.6f seconds\n", buffer, 0.0 );
	
	return 0;
}

int to_sys_clock ( int utc )
{
	struct timeval tv = { 0, 0 };
	const struct timezone tz = { timezone/60 - 60*daylight, 0 };
	
	tv. tv_sec = read_rtc ( utc );

	if ( settimeofday ( &tv, &tz ))
		perror_msg_and_die ( "settimeofday() failed" );

	return 0;
}

int from_sys_clock ( int utc )
{
	struct timeval tv = { 0, 0 };
	struct timezone tz = { 0, 0 };

	if ( gettimeofday ( &tv, &tz ))
		perror_msg_and_die ( "gettimeofday() failed" );

	write_rtc ( tv. tv_sec, utc );
	return 0;
}


int check_utc ( void )
{
	int utc = 0;
	FILE *f = fopen ( "/etc/adjtime", "r" );
	
	if ( f ) {
		char buffer [128];
	
		while ( fgets ( buffer, sizeof( buffer ), f )) {
			int len = xstrlen ( buffer );
			
			while ( len && isspace ( buffer [len - 1] ))
				len--;
				
			buffer [len] = 0;
		
			if ( strncmp ( buffer, "UTC", 3 ) == 0 ) {
				utc = 1;
				break;
			}
		}
		fclose ( f );
	}
	return utc;
}

extern int hwclock_main ( int argc, char **argv )
{
	int	opt;
	enum OpMode mode = SHOW;
	int utc = 0;
	int utc_arg = 0;

#ifdef CONFIG_FEATURE_HWCLOCK_LONGOPTIONS
	struct option long_options[] = {
		{ "show",      0, 0, 'r' },
		{ "utc",       0, 0, 'u' },
		{ "localtime", 0, 0, 'l' },
		{ "hctosys",   0, 0, 's' },
		{ "systohc",   0, 0, 'w' },
		{ 0,           0, 0, 0 }
	};
	
	while (( opt = getopt_long ( argc, argv, "rwsul", long_options, 0 )) != EOF ) {
#else
	while (( opt = getopt ( argc, argv, "rwsul" )) != EOF ) {
#endif
		switch ( opt ) {
		case 'r': 
			mode = SHOW; 
			break;
		case 'w': 
			mode = SYSTOHC;
			break;
		case 's': 
			mode = HCTOSYS;
			break;
		case 'u': 
			utc = 1;
			utc_arg = 1;
			break;
		case 'l': // -l is not supported by the normal hwclock (only --localtime)
			utc = 0;
			utc_arg = 1;
			break;
		default:
			show_usage();
			break;
		}
	}

	if ( !utc_arg )
		utc = check_utc ( );
	
	switch ( mode ) {
	case SYSTOHC:
		return from_sys_clock ( utc );

	case HCTOSYS:
		return to_sys_clock ( utc );

	case SHOW:
	default:
		return show_clock ( utc );	
	}	
}


