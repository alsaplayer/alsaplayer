#ifndef __alsaplayer_error_h__
#define __alsaplayer_error_h__

extern void (*alsaplayer_error)(const char *fmt, ...);
void alsaplayer_set_error_function (void (*func)(const char *, ...));


#endif /* __alsaplayer_error_h__ */
