/* Copyright 1998 Lars T Hansen.
 *
 * $Id$
 *
 * Larceny run-time system -- the version number :-)
 *
 * Some nicknames to choose from:
 *  Burpelson AFB               0.46
 *  'Bat' Guano                 0.47
 *  Big Board                   0.48
 *  'Buck' Turgidson            0.49
 *  Ambassador de Sadesky       0.50
 *  The Coca-Cola Company       0.51
 *  CRM-114                     0.52
 *  Dear John                   0.53
 *  Operation Drop-Kick         0.90
 *  Children's Ice Cream        0.91
 *  Definite Missile Track      0.92
 *  Deviated Prevert
 *  Doomsday Device
 *  First Safety
 *  Fluoridation
 *  General Ripper 
 *  Goldie
 *  Grain Alcohol and Rainwater
 *  Group Commander Mandrake
 *  Hi There!
 *  Horsing Around
 *  'King' Kong
 *  Mineshaft Gap
 *  Missile Complex at Laputa
 *  Nuclear Combat
 *  Ominous Rumors
 *  Peace is our Profession
 *  People's Central Air Defense
 *  Personal Feelings
 *  Plan R
 *  Precious Bodily Fluids
 *  Premier Kissoff
 *  President Muffley
 *  Purity of Essence
 *  Recall Code
 *  Ridiculous Camera
 *  Sleigh Bells
 *  Some Sunny Day
 *  Dr Strangelove
 *  Vegas
 *  War Room
 *  World Targets in MegaDeaths
 *  Zhokhov Islands
 *  Mein Fuhrer...I Can Walk!
 */

#include "config.h"

int  larceny_major_version = 0;
int  larceny_minor_version = 92;
char *larceny_version_qualifier = " \"Definite Missile Track\"";

char *date = DATE " " TIME;
char *user = USER;

/* Corresponds to the OS list in Rts/Sys/config.h */
#if defined(SUNOS4)
  char *osname = "SunOS4";
#elif defined(SUNOS5)
  char *osname = "SunOS5";
#elif defined(LINUX)
  char *osname = "Linux";
#elif defined(CYGWIN)
  char *osname = "Cygwin";
#elif defined(BSD_UNIX)
  char *osname = "BSD Unix";
#elif defined(POSIX_UNIX)
  char *osname = "Posix Unix";
#elif defined(XOPEN_UNIX)
  char *osname = "X/Open Unix";
#elif defined(WIN32)
  char *osname = "Win32";
#elif defined(MACOS)
  char *osname = "MacOS";
#elif defined(GENERIC_OS)
  char *osname = "Generic OS";
#else
# error "Can't define OSNAME."
#endif

/* eof */