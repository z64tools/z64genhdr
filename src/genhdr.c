#include "genhdr.h"

char* gIPath;
char* gOPath;
FILE* gF_SrcLD;
FILE* gF_ObjLD;
FILE* gF_ScnLD;
bool gVerbose;
extern DataFile gGbi;
extern DataFile gActorLinkerScript;

static bool ValidName(const char* s) {
	if (s == NULL) return false;
	if (Regex(s, "[a-zA-Z_][a-zA-Z0-9_]{0,}", REGFLAG_START) == s)
		return true;
	
	return false;
}

int main(s32 n, char** arg) {
	u32 i;
	
	if (ParseArgs(arg, "verbose", &i))
		gVerbose = true;
	
	Log_NoOutput();
	
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
	GenHdr_PatchC();
	
	fclose(gF_SrcLD);
	fclose(gF_ObjLD);
	fclose(gF_ScnLD);
}

static void GenHdr_CopyFiles(void) {
	ItemList* list = New(ItemList);
	
	ItemList_List(list, xFmt("%sinclude/", gIPath), -1, LIST_FILES);
	for (s32 i = 0; i < list->num; i++) {
		char* input = list->item[i];
		char* output = xFmt("%s%s", gOPath, StrStr(input, "include/"));
		
		if (StrEnd(list->item[i], "gbi.h")) {
			MemFile* mem = New(MemFile);
			
			MemFile_LoadMem(mem, gGbi.data, gGbi.size);
			MemFile_SaveFile(mem, output);
			MSG("%-8s %-32s -> %s", "copy:", "gbi.h", output + strlen(gOPath));
			
			Free(mem);
			
			continue;
		}
		
		MSG("%-8s %-32s -> %s", "copy:", input + strlen(gIPath), output + strlen(gOPath));
		Sys_MakeDir(Path(output));
		Sys_Copy(input, output);
	}
	ItemList_Free(list);
	
	ItemList_List(list, xFmt("%sassets/", gIPath), -1, LIST_FILES);
	for (s32 i = 0; i < list->num; i++) {
		if (!StrEnd(list->item[i], ".h"))
			continue;
		
		char* input = list->item[i];
		char* output = xFmt("%sinclude/%s", gOPath, StrStr(input, "assets/"));
		
		MSG("%-8s %-32s -> %s", "copy:", input + strlen(gIPath), output + strlen(gOPath));
		Sys_MakeDir(Path(output));
		Sys_Copy(input, output);
	}
	ItemList_Free(list);
	
	ItemList_List(list, xFmt("%ssrc/overlays/actors/", gIPath), -1, LIST_FILES);
	for (s32 i = 0; i < list->num; i++) {
		if (!StrEnd(list->item[i], ".h"))
			continue;
		
		char* input = list->item[i];
		char* output = xFmt("%sinclude/%s", gOPath, StrStr(input, "overlays/actors/"));
		
		MSG("%-8s %-32s -> %s", "copy:", input + strlen(gIPath), output + strlen(gOPath));
		Sys_MakeDir(Path(output));
		Sys_Copy(input, output);
	}
	ItemList_Free(list);
	
	Free(list);
}

void GenHdr_OpenFiles(void) {
	FILE* ldscript;
	
	Sys_MakeDir(gOPath);
	
	GenHdr_CopyFiles();
	
	FileSys_Path(gOPath);
	gF_SrcLD = FOPEN("linker/oot_mq_dbg_pal/sym_src.ld");
	gF_ObjLD = FOPEN("linker/oot_mq_dbg_pal/sym_obj.ld");
	gF_ScnLD = FOPEN("linker/oot_mq_dbg_pal/sym_scn.ld");
	ldscript = FOPEN("linker/actor.ld");
	
	if (!ldscript)
		printf_error("ErrorABC (search this, I'm lazy)");
	
	fwrite(gActorLinkerScript.data, 1, gActorLinkerScript.size, ldscript);
	fclose(ldscript);
}

void GenHdr_ParseZ64Map(void) {
	MemFile* mem = New(MemFile);
	const char* str;
	char* title = strdup("none");
	char* file = strdup("none");
	u32 id = 0;
	bool fst[3] = { true, true, true };
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
		} else if (StrStart(file, "build/assets/scenes")) {
			
			out = gF_ScnLD;
			id = 1;
		} else if (StrStart(file, "build/assets")) {
			if (StrStart(title, ".link_anime"))
				if (StrStr(word, "possiblePadding"))
					continue;
			
			out = gF_ObjLD;
			id = 2;
		} else continue;
		
		if (queNewTitle || fst[id]) {
			if (!fst[id]) fprintf(out, "\n");
			fprintf(out, "/* %s */\n", title + 1);
		}
		if (queNewFile || fst[id]) {
			char* f = xStrNDup(file, strcspn(file, "("));
			
			if (StrStart(f, "build/src/")) {
				f[strlen(f) - 1] = 'c';
				fprintf(out, "\t/* %s */\n", f + strlen("build/src/"));
			}
			if (StrStart(f, "build/assets/"))
				fprintf(out, "\t/* %s */\n", f + strlen("build/assets/"));
		}
		fprintf(out, "\t\t%-32s = 0x%08X;\n", word, addr);
		
		queNewTitle = false;
		queNewFile = false;
		fst[id] = false;
	} while ((str = Line(str, 1)));
	
	MemFile_Free(mem);
	Free(mem);
	Free(title);
	Free(file);
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
		bool newline = (*Token_Stack(0) == '\n' || *Token_Stack(0) == '\0') ? true : false;
		
		if (!strcmp(token, "typedef") || !strcmp(token, "struct"))
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
					
					if (!strcmp(tokens[0], "const")) { // const
						if (!MemFile_Printf(out, "extern "))
							goto write_error;
						
					} else if (!strcmp(tokens[0], "static")) { // Static to Extern
						
						for (s32 i = 0; i < 4; i++)
							Free(tokens[i]);
						
						if (!MemFile_Printf(out, "extern "))
							goto write_error;
						
						s = Token_Next(s);
						
						continue;
						
					} else if (StrPool(tokens[1], "*")) { // Pointer Variable
						
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
						
					} else if (*tokens[1] == '(') { // Function Variable
						
						if (*tokens[2] == '*')
							if (!MemFile_Printf(out, "extern "))
								goto write_error;
						
					}
					
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
	Free(list);
	Free(wd);
}

typedef struct {
	const char* src;
	const char* rep;
} Patch;

Patch gPatch_stddef = {
	"size_t",
	"#ifndef __clang__\n"
	"  typedef unsigned long size_t;\n"
	"#endif"
};

Patch gPatch_mbi[] = {
	{
		"G_ON",
		"// #define G_ON    (1)"
	},
	{
		"G_OFF",
		"// #define G_OFF   (0)"
	}
};

Patch gPatch_z64_h = {
	"ultra64/gs2dex.h",
	"// #include \"ultra64/gs2dex.h\""
};

Patch gPatch_z64actor[] = {
	{
		"world;",
		"    /* 0x024 */ union {\n"
		"                    PosRot world; // Position/rotation in the world\n"
		"                    struct {\n"
		"                        Vec3f pos;\n"
		"                        Vec3s dir;\n"
		"                    };\n"
		"                };"
	},
	{
		"colChkInfo;",
		"    /* 0x098 */ union {\n"
		"                    CollisionCheckInfo colChkInfo; // Variables related to the Collision Check system\n"
		"                    struct {\n"
		"                        char __pad[0x16];\n"
		"                        u8 mass; // Used to compute displacement for OC collisions\n"
		"                        u8 health; // Note: some actors may use their own health variable instead of this one\n"
		"                        u8 damage; // Amount to decrement health by\n"
		"                        u8 damageEffect; // Stores what effect should occur when hit by a weapon\n"
		"                    };\n"
		"                };"
	},
	{
		"shape;",
		"    /* 0x0B4 */ union {\n"
		"                    ActorShape shape; // Variables related to the physical shape of the actor\n"
		"                    struct {\n"
		"                        Vec3s rot;\n"
		"                    };\n"
		"                };"
	},
	{
		"dbgPad[0x10];",
		"#ifdef OOT_MQ_DEBUG_PAL\n"
		"    /* 0x13C */ char dbgPad[0x10]; // Padding that only exists in the debug rom\n"
		"#endif"
	}
};

Patch gPatch_functions[] = {
	{
		"DmaMgr_SendRequest2",
		"s32 DmaMgr_SendRequest2(DmaRequest* req, u32 ram, u32 vrom, u32 size, u32 unk5, OSMesgQueue* queue, OSMesg msg"
	},
	{
		"    const char* file, s32 line);",
		"#ifdef OOT_MQ_DEBUG_PAL\n"
		"  , const char* file, s32 line\n"
		"#endif\n"
		");"
	},
	{
		"DmaMgr_SendRequest1",
		"s32 DmaMgr_SendRequest1(void* ram0, u32 vrom, u32 size\n"
		"#ifdef OOT_MQ_DEBUG_PAL\n"
		"  , const char* file, s32 line\n"
		"#endif\n"
		");"
	}
};

void GenHdr_PatchC(void) {
	MemFile* mem = New(MemFile);
	
	FileSys_Path(gOPath);
	MemFile_Alloc(mem, MbToBin(16));
	
	if (!MemFile_LoadFile_String(mem, FileSys_File("include/libc/stddef.h"))) {
		char* line = CopyLine(LineHead(StrStr(mem->str, gPatch_stddef.src), mem->str), 0);
		StrRep(mem->str, line, gPatch_stddef.rep);
		
		mem->size = strlen(mem->str);
		
		MemFile_SaveFile_String(mem, mem->info.name);
	}
	
	if (!MemFile_LoadFile_String(mem, FileSys_File("include/ultra64/mbi.h"))) {
		for (s32 i = 0; i < ArrayCount(gPatch_mbi); i++) {
			char* line = CopyLine(LineHead(StrStr(mem->str, gPatch_mbi[i].src), mem->str), 0);
			StrRep(mem->str, line, gPatch_mbi[i].rep);
		}
		
		mem->size = strlen(mem->str);
		
		MemFile_SaveFile_String(mem, mem->info.name);
	}
	
	if (!MemFile_LoadFile_String(mem, FileSys_File("include/z64.h"))) {
		char* line = CopyLine(LineHead(StrStr(mem->str, gPatch_z64_h.src), mem->str), 0);
		StrRep(mem->str, line, gPatch_z64_h.rep);
		
		mem->size = strlen(mem->str);
		
		MemFile_SaveFile_String(mem, mem->info.name);
	}
	
	if (!MemFile_LoadFile_String(mem, FileSys_File("include/z64actor.h"))) {
		for (s32 i = 0; i < ArrayCount(gPatch_z64actor); i++) {
			char* line = CopyLine(LineHead(StrStr(mem->str, gPatch_z64actor[i].src), mem->str), 0);
			StrRep(mem->str, line, gPatch_z64actor[i].rep);
		}
		
		mem->size = strlen(mem->str);
		
		MemFile_SaveFile_String(mem, mem->info.name);
	}
	
	if (!MemFile_LoadFile_String(mem, FileSys_File("include/functions.h"))) {
		for (s32 i = 0; i < ArrayCount(gPatch_functions); i++) {
			char* line = CopyLine(LineHead(StrStr(mem->str, gPatch_functions[i].src), mem->str), 0);
			StrRep(mem->str, line, gPatch_functions[i].rep);
		}
		
		mem->size = strlen(mem->str);
		
		MemFile_SaveFile_String(mem, mem->info.name);
	}
	
	if (!MemFile_LoadFile_String(mem, FileSys_File("include/boot/z_std_dma.h"))) {
		for (s32 i = 0; i < ArrayCount(gPatch_functions); i++) {
			char* line = CopyLine(LineHead(StrStr(mem->str, gPatch_functions[i].src), mem->str), 0);
			StrRep(mem->str, line, gPatch_functions[i].rep);
		}
		
		mem->size = strlen(mem->str);
		
		MemFile_SaveFile_String(mem, mem->info.name);
	}
	
	MemFile_Free(mem);
	Free(mem);
}
