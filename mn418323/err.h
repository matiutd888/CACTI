#ifndef _ERR_
#define _ERR_

// Biblioteka stworzona przez pracowników MIM UW na potrzeby zajęć
// laboratoryjnych z przedmiotu Programowanie Współbieżne.

/* print system call error message and terminate */
extern void syserr(int bl, const char *fmt, ...);

/* print error message and terminate */
extern void fatal(const char *fmt, ...);

#endif
