/* acconfig.h -- hand-written definitions to eventually go into config.h */

/* Define this if there is a connect(2) call */
#undef HAVE_CONNECT

/* Define if we have an off64_t largefile type */
#undef HAVE_OFF64_T

/* Ask for large file support (LFS).  Should always be on, even if it
 * achieves nothing. */
#undef _LARGEFILE_SOURCE
#undef _LARGEFILE64_SOURCE

/* How many bits would you like to have in an off_t? */
#undef _FILE_OFFSET_BITS

/* Define to get i18n support */
#undef ENABLE_NLS

/* Define if you want the suboptimal X/Open catgets implementation */
#undef HAVE_CATGETS

/* Define if you want the nice new GNU and Uniforum gettext system */
#undef HAVE_GETTEXT

/* Define if your system has the LC_MESSAGES locale category */
#undef HAVE_LC_MESSAGES

/* Define if you have stpcpy (copy a string and return a pointer to
 * the end of the result.) */
#undef HAVE_STPCPY

/* end of acconfig.h */
