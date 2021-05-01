// SimpleGraphic Engine
// (c) David Gowor, 2014
//
// Module: Common
//

#include "common.h"

// ===================
// Argument List Class
// ===================

args_c::args_c(const char* in)
{
	argc = 0;
	memset(argv, 0, sizeof(char*) * 256);

	argBuf = AllocString(in);
	char* ptr = argBuf;
	while (*ptr) {
		if (isspace(*ptr)) {
			ptr++;
		} else if (*ptr == '"') {
			argv[argc++] = ++ptr;
			while (*ptr && *ptr != '"') {
				ptr++;
			}
			if (*ptr) *(ptr++) = 0;
		} else {
			argv[argc++] = ptr++;
			while (*ptr && !isspace(*ptr)) {
				ptr++;
			}
			if (*ptr) *(ptr++) = 0;
		}
	}
}

args_c::~args_c()
{
	FreeString(argBuf);
}

const char* args_c::operator[](int i)
{
	if (i >= 0 && i < argc) {
		return argv[i];
	} else {
		return "";
	}
}

// =================
// Text Buffer Class
// =================

textBuffer_c::textBuffer_c()
{
	buf = NULL;
	Init();
}

textBuffer_c::~textBuffer_c()
{
	Free();
}

void textBuffer_c::Alloc(int sz)
{
	// Allocate, set length and position, and null-teriminate.
	Free();
	buf = new char[sz+1];
	caret = len = sz;
	buf[len] = 0;
}

void textBuffer_c::Init()
{
	// Initialise to zero length
	Alloc(0);
}

void textBuffer_c::Free()
{
	// Delete buffer memory
	delete buf;
	buf = NULL;
}

textBuffer_c &textBuffer_c::operator=(const char* r)
{
	// Reallocate buffer and copy the string
	Alloc((int)strlen(r));
	strcpy(buf, r);
	return *this;
}

void textBuffer_c::IncSize()
{
	len++;
	char* tmp = new char[len+1];
	memcpy(tmp, buf, len);
	tmp[len] = 0;
	delete buf;
	buf = tmp;	
}

void textBuffer_c::DecSize()
{
	len--;
	char* tmp = new char[len+1];
	memcpy(tmp, buf, len);
	tmp[len] = 0;
	delete buf;
	buf = tmp;
}

bool textBuffer_c::KeyEvent(int key, int type)
{
	if (type == KE_KEYUP) {
		return false;
	}

	int escLen;
	switch (key) {
	// Left/Right: move cursor
	case KEY_LEFT:
		if (caret > 0) {
			caret--;
			if (caret > 2 && (escLen = IsColorEscape(&buf[caret-1]))) {
				caret-= escLen;
			}
		}
		return true;
	case KEY_RIGHT:
		if (caret < len) {
			caret++;
			if (len - caret > 2 && (escLen = IsColorEscape(&buf[caret]))) {
				caret+= escLen;
			}
		}
		return true;
	// Home/end: seek cursor to start/end
	case KEY_HOME:
		caret = 0;
		return true;
	case KEY_END:
		caret = len;
		return true;
	// Backspace: delete character to left of cursor, shift remaining buffer
	case KEY_BACK:
		if (len && caret) {
			for (int c = caret - 1; c <= len; c++) {
				buf[c] = buf[c + 1];
			}
			if (caret > 0) caret--;
			DecSize();
		}
		return true;
	// Delete: delete character above cursor, shift remaining buffer
	case KEY_DELETE:
		if (len && caret < len) {
			for (int c = caret; c <= len; c++) {
				buf[c] = buf[c + 1];
			}
			DecSize();
		}
		return true;
	default:
		if (type == KE_CHAR && key >= 32) {
			for (int c = len; c >= caret; c--) {
				buf[c] = buf[c - 1];
			}
			buf[caret++] = key;
			IncSize();
			return true;
		}
		break;
	}
	return false;
}

// ==============
// Color Escaping
// ==============

// Color escape table
static const float* colorEscape[10] = { 
	colorBlack,		// ^0
	colorRed,		// ^1
	colorGreen,		// ^2
	colorBlue,		// ^3
	colorYellow,	// ^4
	colorPurple,	// ^5
	colorAqua,		// ^6
	colorWhite,		// ^7
	colorGray,		// ^8
	colorDarkGray	// ^9
};

int IsColorEscape(const char* str)
{
	if (str[0] != '^') {
		return 0;
	}
	if (isdigit(str[1])) {
		return 2;
	} else if (str[1] == 'x' || str[1] == 'X') {
		for (int c = 0; c < 6; c++) {
			if ( !isxdigit(str[c + 2]) ) {
				return 0;
			}
		}
		return 8;
	}
	return 0;
}

void ReadColorEscape(const char* str, col3_t out)
{
	int len = IsColorEscape(str);
	switch (len) {
	case 2:
		VectorCopy(colorEscape[str[1] - '0'], out);
		break;
	case 8:
		{
			int xr, xg, xb;
			sscanf(str + 2, "%2x%2x%2x", &xr, &xg, &xb);
			out[0] = xr / 255.0f;
			out[1] = xg / 255.0f;
			out[2] = xb / 255.0f;
		}
		break;
	}
}

// ================
// String Functions
// ================

// MemTrak hack
#ifdef _MEMTRAK_H
#undef new
#else
#define new(f,l) new
#endif

char* _AllocString(const char* str, const char* file, int line)
{
	if (str == NULL) {
		return NULL;
	}

	size_t aslen = strlen(str) + 1;
	char* al = new(file, line) char[aslen];
	strcpy(al, str);
	return al;
}

char* _AllocStringLen(size_t len, const char* file, int line)
{
	char* al = new(file, line) char[len+1];
	al[len] = 0;
	return al;
}

void FreeString(const char* str)
{
	if (str) delete[] str;
}

dword StringHash(const char* str, int mask)
{
	size_t len = strlen(str);
	dword hash = 0;
	for (size_t i = 0; i < len; i++) {
		hash+= (str[i] * 4999) ^ (((dword)i + 17) * 2003);
	}
	return hash & mask;
}
