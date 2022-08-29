#include <ext_lib.h>

char* gIPath;
char* gOPath;
FILE* gF_SrcLD;
FILE* gF_ObjLD;

#define FOPEN(filename) ({ \
		FILE* f = fopen(FileSys_File(filename), "w"); \
		if (!f) printf_error("Could not fopen file [%s]", filename); \
		f; \
	})

void GenHdr_OpenFiles(void) {
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

static void GenHdr_CFile(const char* file) {
	MemFile* mem = New(MemFile);
	MemFile* hdr = New(MemFile);
	u32 brace = 0;
	u32 equals = 0;
	char* str;
	
	if (MemFile_LoadFile_String(mem, file))
		printf_error("Could not load file [%s]");
	MemFile_Alloc(hdr, mem->size);
	
	str = mem->str;
	
	while (*str != '\0') {
		if (*str == '{')
			brace++;
		
		if (!brace) {
			if (*str == '=')
				equals++;
			if (equals && *str == ';')
				equals--;
			
			if (!equals)
				if (!MemFile_Write(hdr, str, 1))
					printf_error("Ran out of space to write!");
			
		}
		
		if (*str == '}') {
			brace--;
			
			if (!equals && !brace)
				MemFile_Printf(hdr, ";");
		}
		
		str++;
	}
	
	StrRep(hdr->str, " ;\n", ";");
	hdr->size = strlen(hdr->str);
	
	FileSys_Path(gOPath);
	Sys_MakeDir(Path(FileSys_File(file)));
	MemFile_SaveFile_String(hdr, xRep(FileSys_File(file), ".c", ".h"));
	
	ThreadLock_Lock();
	printf_info("OK: %s", file);
	ThreadLock_Unlock();
	
	MemFile_Free(mem);
	MemFile_Free(hdr);
	Free(mem);
	Free(hdr);
}

void GenHdr_GenerateHeaders(void) {
	ItemList* list = New(ItemList);
	const char* wd = strdup(Sys_WorkDir());
	
	Sys_SetWorkDir(xFmt("%ssrc/", gIPath));
	ItemList_List(list, "", -1, LIST_FILES);
	
	for (u32 i = 0; i < list->num; i++) {
		if (StrStart(list->item[i], "overlays"))
			continue;
		if (StrStart(list->item[i], "gcc_fix"))
			continue;
		if (StrStart(list->item[i], "elf_message"))
			continue;
		if (!StrEnd(list->item[i], ".c"))
			continue;
		if (list->item[i] == NULL)
			continue;
		
		ThdPool_Add(GenHdr_CFile, list->item[i], 0);
	}
	
	ThreadLock_Init();
	ThdPool_Exec(16, true);
	ThreadLock_Free();
	
	Sys_SetWorkDir(wd);
	ItemList_Free(list);
	Free(wd);
	Free(list);
}

int main(int n, char** arg) {
	u32 i;
	
	if (ParseArgs(arg, "i", &i)) {
		gIPath = qFree(StrDupX(arg[i], 256));
		
		if (PathIsRel(gIPath))
			strcpy(gIPath, PathAbs(gIPath));
		
		if (!StrEnd(gIPath, "/"))
			strcat(gIPath, "/");
	}
	
	if (ParseArgs(arg, "o", &i)) {
		if (arg[i]) {
			gOPath = StrDupX(arg[i], 512);
		} else {
			gOPath = StrDupX(Sys_WorkDir(), 512);
		}
		
		if (PathIsRel(gOPath))
			strcpy(gOPath, PathAbs(gOPath));
		
		if (!StrEnd(gOPath, "/"))
			strcat(gOPath, "/");
	}
	
	GenHdr_GenerateHeaders();
	GenHdr_OpenFiles();
	GenHdr_ParseZ64Map();
}
