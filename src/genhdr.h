#ifndef GENHDR_H
#define GENHDR_H

#include <ext_lib.h>

extern char* gIPath;
extern char* gOPath;
extern FILE* gF_SrcLD;
extern FILE* gF_ObjLD;
extern bool gVerbose;

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"

#define FOPEN(filename) ({ \
		MSG("FOPEN: %s", FileSys_File(filename)); \
		FILE* f = fopen(FileSys_File(filename), "w"); \
		if (!f) printf_error("Could not fopen file [%s]", filename); \
		f; \
	})

void GenHdr_OpenFiles(void);
void GenHdr_ParseZ64Map(void);
void GenHdr_GenerateHeaders(void);

char* Token_Next(const char* str);
char* Token_Stack(s32 i);
char* Token_Prev(const char* str);
char* Token_Copy(const char* str);
void Token_AllocStack(void);
void Token_FreeStack(void);

static inline void MSG(const char* fmt, ...) {
	char* msg;
	va_list va;
	
	if (!gVerbose)
		return;
	
	va_start(va, fmt);
	vasprintf(&msg, fmt, va);
	va_end(va);
	
	ThreadLock_Lock();
	printf_info("%s", msg);
	ThreadLock_Unlock();
	Free(msg);
}

#pragma GCC diagnostic pop

#endif