// SimpleGraphic Engine
// (c) David Gowor, 2014
//
// Module: Console
//

#include "common.h"

// =============
// Configuration
// =============

#define CON_MAXLINES 1024
#define CON_MAXLINEMASK (CON_MAXLINES-1)

#define CON_MAXCMD 512			// Maximum commands
#define CON_MAXCVAR 512			// Maximum cvars
#define CON_MAXMATCH CON_MAXCMD + CON_MAXCVAR

#define CON_MAXCMDBUFFER 1024	// Maximum buffered commands

#define CON_MAXHIST 32			// Maximum history

// ==========
// Cvar Class
// ==========

conVar_c::conVar_c(IConsole* conHnd)
	: con(conHnd)
{
	name = NULL;
	strVal = NULL;
	defVal = NULL;
}

conVar_c::~conVar_c()
{
	FreeString(name);
	FreeString(strVal);
	FreeString(defVal);
}

void conVar_c::Set(int in)
{
	char tmp[16];
	_itoa_s(in, tmp, 16, 10);
	Set(tmp);
}

void conVar_c::Set(float in)
{
	char tmp[64];
	sprintf_s(tmp, 64, "%f", in);
	Set(tmp);
}

void conVar_c::Set(const char* in)
{
	if ( !strcmp(in, strVal) ) {
		// No change
		return;
	}

	FreeString(strVal);

	mod = true;			// Flag as modified
	intVal = atoi(in);	// Set values
	floatVal = (float)atof(in);
	strVal = AllocString(in);

	Clamp();			// Clamp value
}

void conVar_c::Toggle()
{
	Set(!intVal);
}

int conVar_c::Get(char* out, int outSz)
{
	if (out) {
		strncpy_s(out, outSz, strVal, outSz-1);
	}

	return intVal;
}

bool conVar_c::GetMod()
{
	bool wasMod = mod;
	mod = false;
	return wasMod;
}

void conVar_c::Reset()
{
	FreeString(strVal);
	intVal = atoi(defVal);
	floatVal = (float)atof(defVal);
	strVal = AllocString(defVal);
}

void conVar_c::Clamp()
{
	if ((flags & CV_CLAMP) == 0) {
		return;
	}

	if (intVal < min) {
		Set(min);
	} else if (intVal > max) {
		Set(max);
	} else {
		return;
	}
	
	con->Printf("\"%s\" clamped to %d\n", name, intVal);
}

// =======
// Classes
// =======

// Buffer line
struct conLine_s {
	char*	buf;
	int		len;
	bool	newLine;
};

// Print hook list entry
struct conHookEntry_s {
	conHookEntry_s* prev;
	conHookEntry_s* next;
	conPrintHook_c* hook;
};

// ==================
// IConsole Interface
// ==================

class console_c: public IConsole {
public:
	// Interface
	void	Print(const char* text);
	void	Printf(const char* fmt, ...);
	void	PrintFunc(const char* func);
	void	Warning(const char* fmt, ...);
	void	Clear();
	void	Scroll(int mode);
	const char*	EnumLines(int* index);
	char*	BuildBuffer();

	void	Execute(const char* cmd);
	void	Executef(const char* fmt, ...);
	void	ExecCommands(bool deferUnknown);

	conVar_c* Cvar_Add(const char* name, int flags, const char* def, int minVal = 0, int maxVal = 0);
	conVar_c* Cvar_Ptr(const char* name);

	conCmd_c* EnumCmd(int* index);
	conVar_c* EnumCvar(int* index);

	// Encapsulated
	console_c();
	~console_c();

	int		bufLen;		// Combined length of lines
	int		bufNumLine;	// Line count
	conLine_s bufLines[CON_MAXLINES];
	int		bufFirst;	// First line in buffer
	int		bufLast;	// Last line in buffer
	int		bufScroll;	// Scroll point

	void	Buffer_Init();
	void	Buffer_Shutdown();
	void	Buffer_PrintLine(char* text);

	conHookEntry_s* hookFirst;
	conHookEntry_s* hookLast;

	void	Hook_RunHooks(const char* text);
	void	Hook_RunClear();

	conCmd_c* cmdList[CON_MAXCMD];

	conCmd_c* Cmd_Ptr(const char* name);

	conVar_c* cvarList[CON_MAXCVAR];

	int		Cvar_Find(const char* name);

	int		cmdBuf_numLine;
	char*	cmdBuf_lines[CON_MAXCMDBUFFER];

	textBuffer_c input;				// Input buffer
	textBuffer_c hist[CON_MAXHIST];	// Command history buffers
	int		numHist;				// Number of history items
	int		histSel;				// Current selection in history
};

IConsole* IConsole::GetHandle()
{
	return new console_c();
}

void IConsole::FreeHandle(IConsole* hnd)
{
	delete (console_c*)hnd;
}

console_c::console_c()
{
	Buffer_Init();

	// Initialise hooks
	hookFirst = NULL;
	hookLast = NULL;

	// Clear lists
	memset(cmdList, 0, sizeof(cmdList));
	memset(cvarList, 0, sizeof(cvarList));
	
	cmdBuf_numLine = 0;

	numHist = -1;
	histSel = -1;
}

console_c::~console_c()
{
	// Clear command buffer
	for (int l = 0; l < cmdBuf_numLine; l++) {
		FreeString(cmdBuf_lines[l]);
	}

	// Delete commands and variables
	for (int slot = 0; slot < CON_MAXCMD; slot++) {
		delete cmdList[slot];
	}
	for (int slot = 0; slot < CON_MAXCVAR; slot++) {
		delete cvarList[slot];
	}

	// Delete hooks
	conHookEntry_s* i = hookFirst;
	while (i) {
		delete LL_Next(i);
	}

	Buffer_Shutdown();
}

// ===========================
// Console Buffer and Printing
// ===========================

void console_c::Buffer_Init()
{
	bufLen = 0;
	bufNumLine = 1;
	bufLines[0].len = 0;
	bufLines[0].buf = NULL;
	bufLines[0].newLine = false;
	bufFirst = 0;
	bufLast = 0;
	bufScroll = 0;
}

void console_c::Buffer_Shutdown()
{
	for (int l = 0; l < bufNumLine; l++) {
		delete bufLines[l].buf;
	}
}

void console_c::Buffer_PrintLine(char* text)
{
	if (bufLines[bufLast].newLine) {
		// Start a new line
		conLine_s* line;
		if (bufNumLine < CON_MAXLINES) {
			// Make a new line
			line = bufLines + bufNumLine++;
		} else {
			// Overwrite first line
			line = bufLines + bufFirst;
			delete line->buf;
			bufLen-= line->len;
			bufFirst = (bufFirst + 1) & CON_MAXLINEMASK;
		}
		line->len = 0;
		line->buf = NULL;
		line->newLine = false;
		bufLast = (bufLast + 1) & CON_MAXLINEMASK;
	}
	if (*text) {
		int stlen = (int)strlen(text);
		bufLen+= stlen;
		conLine_s* line = bufLines + bufLast;
		trealloc(line->buf, line->len + stlen + 1);
		strcpy(line->buf + line->len, text);
		line->len+= stlen;
	}
}

void console_c::Print(const char* text)
{
	// Run print hooks
	Hook_RunHooks(text);

	char line[4096];
	int lineLen = 0;
	for (const char* p = text; *p; p++) {
		if (*p == '\n') {
			// Separate into lines
			line[lineLen] = 0;
			Buffer_PrintLine(line);
			bufLines[bufLast].newLine = true;
			lineLen = 0;
		} else {
			line[lineLen++] = *p;
		}
	}

	if (lineLen) {
		// Print the rest
		line[lineLen] = 0;
		Buffer_PrintLine(line);
	}

	// Scroll to the bottom
	bufScroll = bufLast;
}

void console_c::Printf(const char* fmt, ...)
{
	// Normal print
	va_list va;
	va_start(va, fmt);
	char text[4096];
	vsnprintf(text, 4095, fmt, va);
	text[4095] = 0;
	va_end(va);
	Print(text);
}

void console_c::PrintFunc(const char* func)
{
	// Print function title
	char text[256];
	sprintf_s(text, 256, "\n--- %s ---\n", func);
	Print(text);
}

void console_c::Warning(const char* fmt, ...)
{
	// Print warning text
	va_list va;
	va_start(va, fmt);
	char text[4096]; 
	vsprintf_s(text, 4096, fmt, va);
	va_end(va);
	Printf("^4Warning: %s\n", text);
}

void console_c::Clear()
{
	// Recreate output buffer and run hook clears
	Buffer_Shutdown();
	Buffer_Init();
	Hook_RunClear();
}

void console_c::Scroll(int mode)
{
	switch (mode) {
	case CBSC_UP:
		// Try to scroll up 4 lines
		for (int sc = 0; sc < 4; sc++) {
			if (bufScroll == bufFirst) {
				break;
			}
			bufScroll = (bufScroll - 1) & CON_MAXLINEMASK;
		}
		break;
	case CBSC_DOWN:
		// Try to scroll down 4 lines
		for (int sc = 0; sc < 4; sc++) {
			if (bufScroll == bufLast) {
				break;
			}
			bufScroll = (bufScroll + 1) & CON_MAXLINEMASK;
		}
		break;
	case CBSC_BOTTOM:
		// Scroll to last line in buffer
		bufScroll = bufLast;
		break;
	}
}

const char* console_c::EnumLines(int* index)
{
	if (*index <= -1) {
		// Start traversing from scroll point
		*index = bufScroll;
	} else if (*index == bufFirst) {
		// Reached the end of the buffer
		return NULL;
	} else {
		*index = (*index - 1) & CON_MAXLINEMASK;
	}
	
	// Return next line
	const char* line = bufLines[*index].buf;
	return line? line:"";
}

char* console_c::BuildBuffer()
{
	// Allocate space for line data + newlines + terminator
	char* buf = AllocStringLen(bufLen + bufNumLine); 

	// Append the lines
	char* p = buf;
	for (int l = 0; l < bufNumLine; l++) {
		conLine_s* line = bufLines + ((bufFirst + l) & CON_MAXLINEMASK);
		if (line->len) {
			strcpy(p, line->buf);
			p+= line->len;
		}
		*(p++) = '\n';
	}

	return buf;
}

// =====================
// Console Print Hooking
// =====================

conPrintHook_c::conPrintHook_c(IConsole* conHnd)
{
	_con = (console_c*)conHnd;
}

conPrintHook_c::~conPrintHook_c()
{
	RemovePrintHook();
}

void conPrintHook_c::InstallPrintHook()
{
	RemovePrintHook();
	conHookEntry_s* h = new conHookEntry_s;
	h->hook = this;
	LL_Link(_con->hookFirst, _con->hookLast, h);
}

void conPrintHook_c::RemovePrintHook()
{
	conHookEntry_s* i = _con->hookFirst;
	while (i) {
		if (i->hook == this) {
			LL_Unlink(_con->hookFirst, _con->hookLast, i);
			delete i;
			return;
		}
		LL_Next(i);
	}
}

void console_c::Hook_RunHooks(const char* text)
{
	conHookEntry_s* i = hookFirst;
	while (i) {
		LL_Next(i)->hook->ConPrintHook(text);
	}
}

void console_c::Hook_RunClear()
{
	conHookEntry_s* i = hookFirst;
	while (i) {
		LL_Next(i)->hook->ConPrintClear();
	}
}

// ================
// Console Commands
// ================

conCmdHandler_c::conCmdHandler_c(IConsole* conHnd)
{
	_con = (console_c*)conHnd;
}

conCmdHandler_c::~conCmdHandler_c()
{
	// Remove any commands added by this handler
	for (int slot = 0; slot < CON_MAXCMD; slot++) {
		if (_con->cmdList[slot] && _con->cmdList[slot]->obj == this) {
			delete _con->cmdList[slot];
			_con->cmdList[slot] = NULL;
		}
	}
}

void conCmdHandler_c::Cmd_PrivAdd(const char* name, int minArgs, const char* usage, conCmdHandler_c* obj, conCmdMethod_t method)
{
	if (_con->Cmd_Ptr(name)) {
		_con->Warning("command '%s' already exists", name);
		return;
	}

	// Find a free slot
	for (int slot = 0; slot < CON_MAXCMD; slot++) {
		if (_con->cmdList[slot] == NULL) {
			// Allocate and fill new command structure
			_con->cmdList[slot] = new conCmd_c;
			_con->cmdList[slot]->name = AllocString(name);
			_con->cmdList[slot]->minArgs = minArgs;
			_con->cmdList[slot]->usage = AllocString(usage);
			_con->cmdList[slot]->obj = obj;
			_con->cmdList[slot]->method = method;
			return;
		}
	}

	// Command array is full...
	_con->Warning("Reached console command limit (CON_MAXCMD)");
}

conCmd_c* console_c::Cmd_Ptr(const char* name)
{
	for (int slot = 0; slot < CON_MAXCMD; slot++) {
		if (cmdList[slot] && _stricmp(cmdList[slot]->name, name) == 0) {
			return cmdList[slot];
		}
	}
	return NULL;
}

conCmd_c* console_c::EnumCmd(int* index)
{
	if (*index < -1 || *index >= CON_MAXCMD - 1) {
		return NULL;
	}
	while (1) {
		(*index)++;
		if (*index >= CON_MAXCMD) {
			return NULL;
		}
		if (cmdList[*index]) {
			return cmdList[*index];
		}
	}
}

// =================
// Console Variables
// =================

conVar_c* console_c::Cvar_Add(const char* name, int flags, const char* def, int minVal, int maxVal)
{
	char* setVal = NULL;
	int slot = Cvar_Find(name);
	if (slot >= 0) {
		if (cvarList[slot]->flags & CV_SET) {
			// Has been set, take value and delete old cvar
			setVal = AllocString(cvarList[slot]->strVal);
			delete cvarList[slot];
			cvarList[slot] = NULL;
		} else {
			return cvarList[slot];
		}
	} else {
		// Find a free slot
		for (int s = 0; s < CON_MAXCVAR; s++) {
			if (cvarList[s] == NULL) {
				slot = s;
				break;
			}
		}
		if (slot < 0) {
			// Cvar list is full...
			Warning("Reached console variable limit (CON_MAXCVAR)");
			return NULL;
		}
	}

	cvarList[slot] = new conVar_c(this);
	cvarList[slot]->name = AllocString(name);
	cvarList[slot]->flags = flags;
	cvarList[slot]->defVal = AllocString(def);
	if (flags & CV_CLAMP) {
		cvarList[slot]->min = minVal;
		cvarList[slot]->max = maxVal;
	}
	cvarList[slot]->Reset();

	if (setVal) {
		cvarList[slot]->Set(setVal);
		cvarList[slot]->mod = false;
		FreeString(setVal);
	}

	return cvarList[slot];
}

conVar_c* console_c::Cvar_Ptr(const char* name)
{
	int slot = Cvar_Find(name);
	if (slot >= 0) {
		return cvarList[slot];
	} else {
		return NULL;
	}
}

int console_c::Cvar_Find(const char* name)
{
	// Find the cvar and return the index
	for (int slot = 0; slot < CON_MAXCVAR; slot++) {
		if (cvarList[slot] && _stricmp(name, cvarList[slot]->name) == 0) {
			return slot;
		}
	}
	return -1;
}

conVar_c* console_c::EnumCvar(int* index)
{
	if (*index < -1 || *index >= CON_MAXCMD - 1) {
		return NULL;
	}
	while (1) {
		(*index)++;
		if (*index >= CON_MAXCMD) {
			return NULL;
		}
		if (cvarList[*index]) {
			return cvarList[*index];
		}
	}
}

// ===============
// String Executor
// ===============

void console_c::Execute(const char* cmd)
{
	char* newCmd = AllocString(cmd);
	char* tk_context = NULL;
	char* lp = strtok_s(newCmd, ";\n", &tk_context);
	while (lp) {
		if (cmdBuf_numLine >= CON_MAXCMDBUFFER) {
			Warning("console command buffer overflow");
		} else {
			cmdBuf_lines[cmdBuf_numLine++] = AllocString(lp);
		}
		lp = strtok_s(NULL, ";\n", &tk_context);
	}
	FreeString(newCmd);
}

void console_c::Executef(const char* fmt, ...)
{
	va_list va;
	char cmd[4096];
	va_start(va, fmt);
	vsnprintf_s(cmd, 4095, fmt, va);
	cmd[4095] = 0;
	va_end(va);
	Execute(cmd);
}

void console_c::ExecCommands(bool deferUnknown)
{
	int numDefer = 0;
	for (int l = 0; l < cmdBuf_numLine; l++) {
		// Split command string
		args_c args(cmdBuf_lines[l]);
		if (args.argc == 0) {
			continue;
		}

		// Check for commands first
		conCmd_c* cmd = Cmd_Ptr(args[0]);
		if (cmd) {
			if (args.argc < cmd->minArgs + 1) {
				// Too few arguments
				Printf("Usage: %s %s\n", cmd->name, cmd->usage);
			} else {
				// We've got arguments, or the command doesn't care
				(cmd->obj->*cmd->method)(this, args);
			}
		} else {
			conVar_c* cv = Cvar_Ptr(args[0]);
			if (cv) {
				if (args.argc >= 2) {
					// There are arguments, try and set cvar
					if (cv->flags & CV_READONLY) {
						Printf("'%s' is read only.\n", cv->name);
					} else {
						cv->Set(args[1]);
					}
				} else {
					// No arguments, so print current value
					Printf("'%s' is: \"%s\" default: \"%s\"\n", cv->name, cv->strVal, cv->defVal);
				}
			} else if (deferUnknown) {
				// Defer execution of unknown commands
				cmdBuf_lines[numDefer++] = cmdBuf_lines[l];
				continue;
			} else {
				Printf("Unknown command '%s'\n", args[0]);	
			}
		}
		FreeString(cmdBuf_lines[l]);
	}
	cmdBuf_numLine = numDefer;
}

// =============
// Console Input
// =============

conInputHandler_c::conInputHandler_c(IConsole* conHnd)
{
	_con = (console_c*)conHnd;
}

void conInputHandler_c::ClearConInput()
{
	_con->histSel = -1;		
	_con->input.Init();

	RefreshConInput();
}

void conInputHandler_c::RefreshConInput()
{
	SetConInput(_con->input.buf, _con->input.caret);
}

void conInputHandler_c::ConInputKeyEvent(int key, int type)
{
	if (type == KE_KEYDOWN) {
		switch (key) {
		// PgUp/Dn and mousewheel scroll the buffer
		case KEY_PGUP:
		case KEY_MWHEELUP:
			_con->Scroll(CBSC_UP);
			return;
		case KEY_PGDN:
		case KEY_MWHEELDOWN:
			_con->Scroll(CBSC_DOWN);
			return;
		// Up/down select from command history
		case KEY_UP:
			if (_con->histSel < _con->numHist) {
				// Copy next history item
				_con->histSel++;
				_con->input = _con->hist[_con->histSel].buf;
				RefreshConInput();
			}
			return;
		case KEY_DOWN:
			if (_con->histSel > 0) {
				// Copy previous history item
				_con->histSel--;
				_con->input = _con->hist[_con->histSel].buf;
				RefreshConInput();
			} else if (_con->histSel == 0) {
				// Clear buffer if we go after history start
				ClearConInput();
			}
			return;
		// Tab completes or finds matches for input buffer text
		case KEY_TAB:
			if (_con->input.len) {
				char comp[1024];
				strcpy_s(comp, 1024, _con->input.buf);
				int	compLen = (int)strlen(comp);

				// Build match list
				int num = 0;
				char* match[CON_MAXMATCH];
				char args[CON_MAXMATCH][256];
				for (int slot = 0; slot < CON_MAXCMD; slot++) {
					conCmd_c* cmd = _con->cmdList[slot];
					if (cmd && _strnicmp(cmd->name, comp, compLen) == 0) {
						match[num] = cmd->name;
						strcpy_s(args[num], 256, cmd->usage);
						num++;
					}
				}
				for (int slot = 0; slot < CON_MAXCVAR; slot++) {
					conVar_c* cv = _con->cvarList[slot];
					if (cv && _strnicmp(cv->name, comp, compLen) == 0) {
						match[num] = cv->name;
						sprintf_s(args[num], 256, "= \"%s\"", cv->strVal);
						num++;
					}
				}

				if (num > 0) {
					// Matches were found
					if (num == 1) {
						// Exact match
						strcpy_s(comp, 1024, match[0]);
						if (*args[0]) {
							strcat_s(comp, 1024, " ");
						}
					} else {
						// Multiple matches, print them out
						_con->Printf("]%s\n", comp);
						for (int m = 0; m < num; m++) {
							_con->Printf("  %s %s\n", match[m], args[m]);
						}

						// Try to refine comparison string
						while (compLen < 1024) {
							char c = match[0][compLen];
							bool fail = false;
							for (int m = 1; m < num; m++) {
								if (match[m][compLen] != c) {
									fail = true;
									break;
								}
							}
							if (fail) {
								break;
							}
							comp[compLen++] = c;
						}
						comp[compLen] = 0;
					}

					// Copy comparison string back into input buffer
					_con->input = comp;
					RefreshConInput();
				}
			}
			return;
		// Return executes input buffer
		case KEY_RETURN:
			if (_con->input.len) {
				// Execute buffer
				_con->Printf("]%s\n", _con->input.buf);
				_con->Execute(_con->input.buf);

				// Add to command history if different from most recent command
				if (_stricmp(_con->hist[0].buf, _con->input.buf)) {
					if (_con->numHist + 1 < CON_MAXHIST) {
						_con->numHist++;
					}
					for (int h = _con->numHist; h > 0; h--) {
						_con->hist[h] = _con->hist[h-1].buf;
					}
					_con->hist[0] = _con->input.buf;
				}

				// Clear the input buffer
				ClearConInput();
			}
			return;
		}
	}

	// Send other key events to the input buffer
	if (_con->input.KeyEvent(key, type)) {
		RefreshConInput();
	}
}
