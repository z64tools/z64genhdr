#include "genhdr.h"

#define MultiLineFunc(wow, waw) \
	And heres some stuff \
	that belongs to the \
	macro

static bool chrchr(char c, const char* accept) {
	s32 ln = strlen(accept);
	
	for (s32 i = 0; i < ln; i++)
		if (c == accept[i])
			return true;
	
	return false;
}

ThreadLocal char gTokenPrev[4096];
ThreadLocal char** gTokenStack;

static s32 TokenStrLen(const char* str) {
	if (!isgraph(*str))
		return strspn(str, xFmt("%c", *str));
	
	if (StrStart(str, "//"))
		return LineLen(str);
	
	if (StrStart(str, "/*"))
		return (char*)(StrStr(str, "*/") + 2) - str;
	
	if (StrStart(Token_Stack(1), "#include")) {
		
		return strcspn(str, " \n\t\r");
	}
	
	if (StrStart(Token_Stack(1), "#define")) {
		u32 escape = 0;
		u32 l = 1;
		
		while (!(str[l] == '\n' && escape == 0)) {
			if (str[l] == '\\')
				escape++;
			if (str[l] == '\n')
				escape--;
			
			l++;
		}
		
		return l;
	}
	
	if (chrchr(*str, "()[]{},.;/"))
		return 1;
	if (StrStart(str, "<<") || StrStart(str, ">>"))
		return 2;
	if (chrchr(*str, "*=+-<>"))
		return strspn(str, "*=+-<>");
	if (*str == '\"') {
		s32 l = 1;
		
		while (!(str[l] == '\"' && str[l - 1] != '\\'))
			l++;
		
		return l + 1;
	}
	
	// Hex
	if (Regex(str, "0x[a-fA-F0-9]{1,}", REGFLAG_START) == str)
		return Regex(str, "0x[a-fA-F0-9]{1,}", REGFLAG_END) - str;
	
	// Bit
	if (Regex(str, "0b[01]{1,}", REGFLAG_START) == str)
		return Regex(str, "0x[a-fA-F0-9]{1,}", REGFLAG_END) - str;
	
	// Float
	if (Regex(str, "[0-9]{1,}.[f0-9]{1,}", REGFLAG_START) == str)
		return Regex(str, "[0-9]{1,}.[f0-9]{1,}", REGFLAG_END) - str;
	
	if (isgraph(*str))
		return strcspn(str, " \t\n\r(){}[];,.+-*/");
	
	return 0;
}

char* Token_Next(const char* s) {
	if (!s) return NULL;
	s32 l;
	char* z = (char*)s + strlen(s);
	
	if ((l = TokenStrLen(s)) == 0)
		printf_error("\aUnknown Token: " PRNT_REDD "\"%.8s\"", s);
	
	strncpy((char*)gTokenPrev, s, l);
	gTokenPrev[l] = '\0';
	
	if (gTokenStack) {
		if (!ChrPool(*gTokenPrev, " \t")) {
			ArrMoveR(gTokenStack, 0, 32);
			strcpy(gTokenStack[0], gTokenPrev);
		}
	}
	
	s += l;
	
	if (s >= z)
		return NULL;
	
	return (char*)s;
}

char* Token_Stack(s32 i) {
	if (!gTokenStack)
		return NULL;
	
	return gTokenStack[abs(i)];
}

char* Token_Prev(const char* str) {
	return gTokenPrev;
}

char* Token_Copy(const char* str) {
	if (!str) return NULL;
	
	return strndup(str, TokenStrLen(str));
}

void Token_AllocStack(void) {
	if (!gTokenStack) {
		gTokenStack = Calloc(sizeof(char*) * 32);
		
		for (s32 i = 0; i < 32; i++)
			gTokenStack[i] = Calloc(4096);
	}
}

void Token_FreeStack(void) {
	for (s32 i = 0; i < 32; i++)
		Free(gTokenStack[i]);
	Free(gTokenStack);
}
