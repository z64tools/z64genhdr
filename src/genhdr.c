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
		
		MSG("%-8s %-32s -> %s", "copy:", input + strlen(gIPath), output + strlen(gOPath));
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

static bool ValidName(const char* s) {
	if (s == NULL) return false;
	if (Regex(s, "[a-zA-Z_][a-zA-Z0-9_]{0,}", REGFLAG_START) == s)
		return true;
	
	return false;
}

static void GenHdr_ParseC(const char* file) {
	MemFile* in = New(MemFile);
	MemFile* out = New(MemFile);
	char* s;
	s32 brace = 0;
	s32 equals = 0;
	bool typdef = false;
	
	Time_Start(0);
	
	MemFile_LoadFile_String(in, file);
	MemFile_Alloc(out, in->size * 2);
	s = in->str;
	
	Token_AllocStack();
	
	do {
		char* token = Token_Copy(s);
		bool newline = *Token_Stack(0) == '\n' ? true : false;
		
		if (!strcmp(token, "typedef"))
			typdef = true;
		
		if (typdef) {
			if (*token == '{')
				brace++;
			if (*token == '}')
				brace--;
			
			if (!brace && *token == ';')
				typdef = false;
			
			if (MemFile_Write(out, token, strlen(token)) != strlen(token))
				goto write_error;
			
			continue;
		}
		
		if (*token == '{')
			brace++;
		
		if (!brace) {
			
			// if (!strcmp(token, "KillTHISnow")) {
			// 	for (s32 i = 32; i > 0; i--)
			// 		printf_info("\"%s\"", xRep(Token_Stack(i - 1), "\n", "\\n"));
			// 	exit(0);
			// }
			
			if (newline) {
				char* tokens[4] = {
					s
				};
				
				if (!strcmp(token, "#include")) {
					char* nt = Token_Copy(Token_Next(Token_Next(s)));
					
					if (StrStr(nt, "\"tables/")) {
						if (!MemFile_Printf(out, "// #include \"tables/...\""))
							goto write_error;
						s = Token_Next(Token_Next(s)); // Skip include
						
						Free(nt);
						
						continue;
					}
					
					if (StrStr(nt, ".c\"")) {
						if (!MemFile_Printf(out, "#include %s", xRep(nt, ".c\"", ".h\"")))
							goto write_error;
						s = Token_Next(Token_Next(s)); // Skip include
						
						Free(nt);
						
						continue;
					}
					
					Free(nt);
				}
				
				for (s32 i = 1; i < 4; i++) {
					char* t = Token_Next(tokens[i - 1]);
					
					while (t && ChrPool(*t, " \t"))
						t = Token_Next(t);
					
					tokens[i] = t;
				}
				
				for (s32 i = 0; i < 4; i++) {
					tokens[i] = Token_Copy(tokens[i]);
					Log("tokens[%d] == %s\"%s\"", i, i == 0 ? PRNT_BLUE : "", tokens[i]);
				}
				
				if (ValidName(tokens[0])) { // Valid type name?
					
					if (StrPool(tokens[1], "*")) { // Pointer Variable
						
						if (ValidName(tokens[2])) { // Variable Name
							if (ChrPool(*tokens[3], "[;=")) // Array (un)initialized
								if (!MemFile_Printf(out, "extern "))
									goto write_error;
							
						} else if (*tokens[2] == '(') // Function Variable
							if (*tokens[3] == '*')
								if (!MemFile_Printf(out, "extern "))
									goto write_error;
						
					} else if (ValidName(tokens[1])) { // Data Variable
						
						if (ChrPool(*tokens[2], "[;=")) // Array (un)initialized
							if (!MemFile_Printf(out, "extern "))
								goto write_error;
						
					} else if (!strcmp(tokens[0], "static")) { // Static to Extern
						
						for (s32 i = 0; i < 4; i++)
							Free(tokens[i]);
						
						if (!MemFile_Printf(out, "extern "))
							goto write_error;
						
						s = Token_Next(s);
						
						continue;
						
					} else if (*tokens[1] == '(') // Function Variable
						
						if (*tokens[2] == '*')
							if (!MemFile_Printf(out, "extern "))
								goto write_error;
					
				}
				
				for (s32 i = 0; i < 4; i++)
					Free(tokens[i]);
			}
			
			if (!strcmp(token, "="))
				equals++;
			
			if (equals)
				if (*token== ';')
					equals--;
			
			if (!equals)
				if (MemFile_Write(out, token, strlen(token)) != strlen(token))
					goto write_error;
		}
		
		if (*token == '}') {
			brace--;
			
			if (!brace)
				if (!MemFile_Printf(out, ";"))
					goto write_error;
		}
		
		Free(token);
		Log("Next");
	} while ((s = Token_Next(s)));
	
	StrRep(out->str, ";;", ";");
	StrRep(out->str, " ;", ";");
	
	out->size = strlen(out->str);
	
	FileSys_Path(xFmt("%sinclude/", gOPath));
	MSG("%-8s time: %2.2fs %s", "parse:", Time_Get(0), xRep(FileSys_File(file), ".c", ".h") + strlen(gOPath));
	Sys_MakeDir(Path(FileSys_File(file)));
	
	MemFile_SaveFile_String(out, xRep(FileSys_File(file), ".c", ".h"));
	MemFile_Free(in);
	MemFile_Free(out);
	Free(in);
	Free(out);
	
	Token_FreeStack();
	
	return;
	
write_error:
	printf_error("Ran out of space...");
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
	ThdPool_Exec(16, true);
	MSG("OK!");
	ThreadLock_Free();
	
	Sys_SetWorkDir(wd);
	ItemList_Free(list);
	Free(wd);
	Free(list);
}
