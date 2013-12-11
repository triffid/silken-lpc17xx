#ifndef _DEBUG_H
#define _DEBUG_H

#ifdef DEBUG_MAIN
	#define DEBUG_PRINTF(...) printf(__VA_ARGS__)
#else
	#define DEBUG_PRINTF(...) do {} while (0)
#endif

#endif /* _DEBUG_H */
