#include "genhdr.h"

char* gIPath;
char* gOPath;
FILE* gF_SrcLD;
FILE* gF_ObjLD;
bool gVerbose;

int main(s32 n, char** arg) {
	u32 i;
	
	Log_NoOutput();
	
	if (ParseArgs(arg, "verbose", &i))
		gVerbose = true;
	
	if (ParseArgs(arg, "i", &i)) {
		gIPath = qFree(StrDupX(arg[i], 256));
		
		if (PathIsRel(gIPath))
			strcpy(gIPath, PathAbs(gIPath));
		
		if (!StrEnd(gIPath, "/"))
			strcat(gIPath, "/");
		
		printf_info("Input: %s", gIPath);
	}
	
	if (ParseArgs(arg, "o", &i)) {
		if (!arg[i])
			printf_error("Provide path");
		
		gOPath = StrDupX(arg[i], 512);
		
		if (PathIsRel(gOPath))
			strcpy(gOPath, PathAbs(gOPath));
		
		if (!StrEnd(gOPath, "/"))
			strcat(gOPath, "/");
		
		printf_info("Output: %s", gOPath);
		
		GenHdr_OpenFiles();
	} else
		printf_error("Please provide output directory with --o");
	
	GenHdr_GenerateHeaders();
	GenHdr_ParseZ64Map();
}

void GenHdr_OpenFiles(void) {
	ItemList* list = New(ItemList);
	
	Sys_MakeDir(gOPath);
	
	ItemList_List(list, xFmt("%sinclude/", gIPath), -1, LIST_FILES);
	
	for (s32 i = 0; i < list->num; i++) {
		char* input = list->item[i];
		char* output = xFmt("%s%s", gOPath, StrStr(input, "include/"));
		
		MSG("Copy %s -> %s", input, output);
		Sys_MakeDir(Path(output));
		Sys_Copy(input, output);
	}
	
	FileSys_Path(gOPath);
	gF_SrcLD = FOPEN("symbol_src.ld");
	gF_ObjLD = FOPEN("symbol_obj.ld");
}

void GenHdr_ParseZ64Map(void) {
	MemFile* mem = New(MemFile);
	const char* str;
	char* title = strdup("none");
	char* file = strdup("none");
	u32 id = 0;
	bool fst[2] = { true, true };
	bool newTitle = false, newFile = false;
	bool queNewFile = false, queNewTitle = false;
	FILE* out = NULL;
	
	FileSys_Path(gIPath);
	if (MemFile_LoadFile_String(mem, FileSys_File("build/z64.map")))
		printf_error("Could not locate " PRNT_REDD "z64.map");
	
	str = StrStr(mem->str, "..makerom");
	
	do {
		u32 addr = 0;
		char* word = NULL;
		
		newFile = newTitle = false;
		if (*str == '\0')
			break;
		if (StrStart(str, ".")) {
			Free(title);
			title = strdup(CopyWord(str + 1, 0));
			newTitle = queNewTitle = true;
		}
		if (StrStart(str, " build/")) {
			Free(file);
			file = strdup(CopyWord(str, 0));
			newFile = queNewFile = true;
		}
		
		if (newFile || newTitle)
			continue;
		if (StrStart(str, " ."))
			continue;
		
		addr = Value_Hex(CopyWord(str, 0));
		word = CopyWord(str, 1);
		
		if (*word == '_' || *word == '.' || *word == '*')
			continue;
		if (Value_ValidateHex(word))
			continue;
		
		if (StrStart(file, "build/src")) {
			if (StrStart(file, "build/src/overlays"))
				continue;
			
			out = gF_SrcLD;
			id = 0;
		} else if (StrStart(file, "build/assets")) {
			if (StrStart(file, "build/assets/scenes"))
				continue;
			
			if (StrStart(title, ".link_anime"))
				if (StrStr(word, "possiblePadding"))
					continue;
			
			out = gF_ObjLD;
			id = 1;
		} else
			continue;
		
		if (queNewTitle) {
			if (!fst[id]) fprintf(out, "\n");
			fprintf(out, "/* %s */\n", title);
		}
		if (queNewFile) fprintf(out, "\t/* %s */\n", file);
		fprintf(out, "\t\t%-32s = 0x%08X;\n", word, addr);
		
		queNewTitle = false;
		queNewFile = false;
		fst[id] = false;
	} while ((str = Line(str, 1)));
}

static void GenHdr_ParseC(const char* file) {
	MemFile* in = New(MemFile);
	MemFile* out = New(MemFile);
	char* s;
	s32 brace = 0;
	s32 equals = 0;
	
	MemFile_LoadFile_String(in, file);
	MemFile_Alloc(out, in->size);
	s = in->str;
	
	Token_AllocStack();
	
	FileSys_Path(xFmt("%sinclude/", gOPath));
	MSG("File: %s", xRep(FileSys_File(file), ".c", ".h"));
	Sys_MakeDir(Path(FileSys_File(file)));
	
	do {
		Log("Token Copy");
		char* token = Token_Copy(s);
		
		Log("%s", token);
		
		if (*token == '{')
			brace++;
		
		if (!brace) {
			
			if (!strcmp(token, "="))
				equals++;
			
			if (equals)
				if (*token== ';')
					equals--;
			
			if (!equals)
				if (MemFile_Write(out, token, strlen(token)) != strlen(token))
					printf_error("Exit");
		}
		
		if (*token == '}') {
			brace--;
			
			if (!brace)
				MemFile_Printf(out, ";");
		}
		
		Free(token);
		Log("Next");
	} while ((s = Token_Next(s)));
	
	StrRep(out->str, " ;", ";");
	out->size = strlen(out->str);
	
	MemFile_SaveFile_String(out, xRep(FileSys_File(file), ".c", ".h"));
	MemFile_Free(in);
	MemFile_Free(out);
	Free(in);
	Free(out);
	
	Token_FreeStack();
}

void GenHdr_GenerateHeaders(void) {
	ItemList* list = New(ItemList);
	const char* wd = strdup(Sys_WorkDir());
	
	Sys_SetWorkDir(xFmt("%ssrc/", gIPath));
	ItemList_List(list, "", -1, LIST_FILES);
	
	for (u32 i = 0; i < list->num; i++) {
		if (StrStr(list->item[i], "overlays/"))
			continue;
		if (StrStr(list->item[i], "gcc_fix/"))
			continue;
		if (StrStr(list->item[i], "elf_message/"))
			continue;
		if (!StrEnd(list->item[i], ".c"))
			continue;
		if (list->item[i] == NULL)
			continue;
		
		ThdPool_Add(GenHdr_ParseC, list->item[i], 0);
	}
	
	ThreadLock_Init();
	ThdPool_Exec(1, true);
	ThreadLock_Free();
	
	Sys_SetWorkDir(wd);
	ItemList_Free(list);
	Free(wd);
	Free(list);
}
