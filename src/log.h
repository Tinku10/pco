#ifndef LOG_H
#define LOG_H

#ifdef DEBUG
    #define dbg_print(fmt, ...) \
        fprintf(stderr, "[DBG] " fmt "\n", ##__VA_ARGS__); \
        fflush(stderr)
#else
    #define dbg_print(fmt, ...) ((void)0) // Completely compiled away
#endif

#endif
