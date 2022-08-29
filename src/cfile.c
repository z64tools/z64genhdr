#include "genhdr.h"

ThreadLocal s32 brace;
ThreadLocal s32 equals;
ThreadLocal s32 typdef;
ThreadLocal s32 comment;
ThreadLocal s32 stat;
ThreadLocal s32 macro;
ThreadLocal s32 slash;
ThreadLocal s32 lcom;

static bool Macro(char* str, MemFile* hdr) {
	if (StrStart(str, "#define"))
		macro++;
	
	if (macro) {
		if (*str == '\n') {
			if (!slash)
				macro--;
			else
				slash--;
		}
		
		if (*str == '\\')
			slash++;
		
		MemFile_Write(hdr, str, 1);
		
		return true;
	}
	
	return false;
}

static bool Comment(char* str, MemFile* hdr) {
	if (brace)
		return false;
	
	if (StrStart(str, "/*"))
		comment++;
	if (StrStart(str - 2, "*/"))
		comment--;
	
	if (comment)
		if (!MemFile_Write(hdr, str, 1))
			printf_error("Ran out of space to write!");
	
	return comment;
}

static bool Typedef(char* str, MemFile* hdr) {
	if (StrStart(str, "typedef ")) {
		typdef++;
	}
	
	if (typdef) {
		if (*str == '{')
			brace++;
		if (*str == '}')
			brace--;
		
		if (!brace && *str == ';')
			typdef--;
		
		if (!MemFile_Write(hdr, str, 1))
			printf_error("Ran out of space to write!");
		
		return true;
	}
	
	return false;
}

static bool Static(char* str, MemFile* hdr) {
	if (StrStart(str - 1, "\nstatic "))
		stat++;
	
	if (stat) {
		if (*str == ';')
			stat--;
		
		return true;
	}
	
	return false;
}

static bool LineComment(char* str, MemFile* hdr) {
	if (brace)
		return false;
	
	if (StrStart(str, "//"))
		lcom++;
	
	if (lcom) {
		if (*str == '\n')
			lcom--;
		
		if (!MemFile_Write(hdr, str, 1))
			printf_error("Ran out of space to write!");
		
		return true;
	}
	
	return false;
}

static bool Braces(char* str, MemFile* hdr) {
	if (*str == '{')
		brace++;
	
	if (!brace) {
		if (*str == '=')
			equals++;
		if (equals && *str == ';')
			equals--;
		
		if (!equals) {
			if (str[-1] == '\n' && isgraph(*str)) {
				if (*str != '#')
					if (*str != '/')
						if (!StrStart(str, "typedef"))
							if (!StrStart(str, "extern"))
								if (!(StrStr(CopyLine(str, -2), "BAD_RETURN") && LineLen(CopyLine(str, -2)) < strlen("BAD_RETURN(big_type)")))
									MemFile_Printf(hdr, "extern ");
			}
			
			if (!MemFile_Write(hdr, str, 1))
				printf_error("Ran out of space to write!");
		}
		
	}
	
	if (*str == '}') {
		brace--;
		
		if (!equals && !brace)
			MemFile_Printf(hdr, ";");
	}
	
	return false;
}

void GenHdr_CFile(const char* file) {
	MemFile* mem = New(MemFile);
	MemFile* hdr = New(MemFile);
	char* str;
	
	bool (*fnc[])(char*, MemFile*) = {
		LineComment,
		Comment,
		Typedef,
		Macro,
		Static,
		
		Braces,
	};
	
	MSG("CFile: %s", Basename(file));
	if (MemFile_LoadFile_String(mem, file))
		printf_error("Could not load file [%s]");
	MemFile_Alloc(hdr, mem->size * 4);
	
	for (str = mem->str; *str != '\0'; str++) {
		foreach(i, fnc) {
			if (fnc[i](str, hdr))
				break;
		}
		
	}
	
	StrRep(hdr->str, " ;", ";");
	StrRep(hdr->str, ";\n\n", ";\n");
	StrRep(hdr->str, "extern static", "extern");
	
	while (StrRep(hdr->str, "\n\n\n", "\n\n"))
		(void)0;
	
	str = hdr->str;
	
	hdr->size = strlen(hdr->str);
	FileSys_Path(xFmt("%sinclude/", gOPath));
	Sys_MakeDir(Path(FileSys_File(file)));
	MemFile_SaveFile_String(hdr, xRep(FileSys_File(file), ".c", ".h"));
	
	MemFile_Free(mem);
	MemFile_Free(hdr);
	Free(mem);
	Free(hdr);
}
