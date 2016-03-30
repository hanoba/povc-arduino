#include <avr/pgmspace.h>
extern const char pic_prost_neujahr[40*151] PROGMEM;

typedef struct {
    const char *name;
    const unsigned int length;
    const unsigned char *data;
    const int rotinc;
	const int rotval;
} GifFile;

extern const GifFile gifFiles[] PROGMEM;