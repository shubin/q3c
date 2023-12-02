/*
===========================================================================
Copyright (C) 2017-2019 Gian 'myT' Schellenbaum

This file is part of Challenge Quake 3 (CNQ3).

Challenge Quake 3 is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

Challenge Quake 3 is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Challenge Quake 3. If not, see <https://www.gnu.org/licenses/>.
===========================================================================
*/
// writing JSON to files without dynamic memory allocation etc for crash reports

#include "crash.h"


#if defined(_MSC_VER)
typedef unsigned __int8  u8;
typedef unsigned __int32 u32;
#else
typedef uint8_t  u8;
typedef uint32_t u32;
#endif


static qbool UTF8_NextCodePoint(u32* codePoint, u32* byteCount, const char* input)
{
	if (!codePoint || !byteCount || !input || *input == '\0')
		return qfalse;

	u8 byte0 = (u8)input[0];
	if (byte0 <= 127) {
		*codePoint = (u32)byte0;
		*byteCount = 1;
		return qtrue;
	}

	// Starts with 110?
	if ((byte0 >> 5) == 6) {
		const u8 byte1 = (u8)input[1];
		*codePoint = ((u32)byte1 & 63) | (((u32)byte0 & 31) << 6);
		*byteCount = 2;
		return qtrue;
	}

	// Starts with 1110?
	if ((byte0 >> 4) == 14) {
		const u8 byte1 = (u8)input[1];
		const u8 byte2 = (u8)input[2];
		*codePoint = ((u32)byte2 & 63) | (((u32)byte1 & 63) << 6) | (((u32)byte0 & 15) << 12);
		*byteCount = 3;
		return qtrue;
	}
	
	// Starts with 11110?
	if ((byte0 >> 3) == 30) {
		const u8 byte1 = (u8)input[1];
		const u8 byte2 = (u8)input[2];
		const u8 byte3 = (u8)input[3];
		*codePoint = ((u32)byte3 & 63) | (((u32)byte2 & 63) << 6) | (((u32)byte1 & 63) << 12) | (((u32)byte0 & 7) << 18);
		*byteCount = 4;
		return qtrue;
	}
	
	return qfalse;
}

typedef struct {
	u32 CodePoint;
	char OutputChar;
} ShortEscape;

#define ENTRY(HexValue, Char) { 0x##HexValue, Char }
static const ShortEscape ShortEscapeCodePoints[] = {
	ENTRY(0022, '"'),
	ENTRY(005C, '\\'),
	ENTRY(002F, '/'),
	ENTRY(0008, 'b'),
	ENTRY(000C, 'f'),
	ENTRY(000A, 'n'),
	ENTRY(000D, 'r'),
	ENTRY(0009, 't')
};
#undef ENTRY

static const char HexDigits[16] = { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F' };

static qbool UTF8_NeedsEscaping(u32* newLength, u32 codePoint)
{
	if (!newLength)
		return qfalse;

	const u32 count = (u32)ARRAY_LEN(ShortEscapeCodePoints);
	for (u32 i = 0; i < count; i++) {
		if (codePoint == ShortEscapeCodePoints[i].CodePoint) {
			*newLength = 2; // Of form: "\A".
			return qtrue;
		}
	}

	// Range: 0x0000 - 0x001F
	if (codePoint <= 0x001F) {
		*newLength = 6; // Of form: "\uABCD".
		return qtrue;
	}

	return qfalse;
}

static void UTF8_WriteCodePoint(u32* newLength, char* output, u32 codePoint, const char* input)
{
	if (!newLength || !output || !input)
		return;

	const u32 count = (u32)ARRAY_LEN(ShortEscapeCodePoints);
	for (u32 i = 0; i < count; i++) {
		if (codePoint == ShortEscapeCodePoints[i].CodePoint) {
			*newLength = 2;
			output[0] = '\\';
			output[1] = ShortEscapeCodePoints[i].OutputChar;
			return;
		}
	}

	// Range: 0x0000 - 0x001F
	if (codePoint <= 0x001F) {
		*newLength = 6;
		output[0] = '\\';
		output[1] = 'u';
		output[2] = HexDigits[(codePoint >> 12) & 15];
		output[3] = HexDigits[(codePoint >> 8) & 15];
		output[4] = HexDigits[(codePoint >> 4) & 15];
		output[5] = HexDigits[codePoint & 15];
		return;
	}

	for (u32 i = 0; i < *newLength; ++i) {
		output[i] = input[i];
	}
}

static u32 UTF8_LengthForJSON(const char* input)
{
	if (!input)
		return 0;

	const char* s = input;
	u32 codePoint = 0;
	u32 byteCount = 0;
	u32 newLength = 0;
	while (UTF8_NextCodePoint(&codePoint, &byteCount, s)) {
		s += byteCount;
		UTF8_NeedsEscaping(&byteCount, codePoint);
		newLength += byteCount;
	}

	return newLength;
}

static void UTF8_CleanForJSON(char* output, const char* input)
{
	if (!output || !input)
		return;

	const char* in = input;
	char* out = output;
	u32 codePoint = 0;
	u32 inputByteCount = 0;
	while (UTF8_NextCodePoint(&codePoint, &inputByteCount, in)) {
		u32 outputByteCount = inputByteCount;
		UTF8_WriteCodePoint(&outputByteCount, out, codePoint, in);
		in += inputByteCount;
		out += outputByteCount;
	}

	*out = '\0';
}

typedef struct {
	unsigned int itemIndices[16];
	FILE* file;
	unsigned int level;
} JSONWriter;

static JSONWriter writer;

static void JSONW_Write(const char* string)
{
	if (!string)
		return;

	fwrite(string, strlen(string), 1, writer.file);
}

static void JSONW_WriteNewLine()
{
	JSONW_Write("\n");
	for (u32 i = 0; i < writer.level; i++) {
		JSONW_Write("\t");
	}
}

static void JSONW_WriteClean(const char* string)
{
	static char buffer[4096];

	if (!string)
		return;

	const u32 length = UTF8_LengthForJSON(string);
	if (length >= sizeof(buffer)) {
		JSONW_Write("bufferSizeTooSmall");
		return;
	}

	UTF8_CleanForJSON(buffer, string);
	JSONW_Write(buffer);
}

void JSONW_BeginFile(FILE* file)
{
	memset(&writer, 0, sizeof(writer));
	writer.file = file;
	
	JSONW_Write("{");
	++writer.level;
}

void JSONW_EndFile()
{
	JSONW_Write("\r\n}");
}

void JSONW_BeginObject()
{
	if (writer.itemIndices[writer.level] > 0)
		JSONW_Write(",");

	JSONW_WriteNewLine();
	JSONW_Write("{");
	++writer.level;
	writer.itemIndices[writer.level] = 0;
}

void JSONW_BeginNamedObject(const char* name)
{
	if (!name)
		return;

	if (writer.itemIndices[writer.level] > 0)
		JSONW_Write(",");

	JSONW_WriteNewLine();
	JSONW_Write("\"");
	JSONW_WriteClean(name);
	JSONW_Write("\":");
	JSONW_WriteNewLine();
	JSONW_Write("{");
	++writer.level;
	writer.itemIndices[writer.level] = 0;
}

void JSONW_EndObject()
{
	--writer.level;
	JSONW_WriteNewLine();
	JSONW_Write("}");
	++writer.itemIndices[writer.level];
}

void JSONW_BeginArray()
{
	if (writer.itemIndices[writer.level] > 0)
		JSONW_Write(",");

	JSONW_WriteNewLine();
	JSONW_Write("[");
	++writer.level;
	writer.itemIndices[writer.level] = 0;
}

void JSONW_BeginNamedArray(const char* name)
{
	if (!name)
		return;

	if (writer.itemIndices[writer.level] > 0)
		JSONW_Write(",");

	JSONW_WriteNewLine();
	JSONW_Write("\"");
	JSONW_WriteClean(name);
	JSONW_Write("\":");
	JSONW_WriteNewLine();
	JSONW_Write("[");
	++writer.level;
	writer.itemIndices[writer.level] = 0;
}

void JSONW_EndArray()
{
	--writer.level;
	JSONW_WriteNewLine();
	JSONW_Write("]");
	++writer.itemIndices[writer.level];
}

void JSONW_IntegerValue(const char* name, int number)
{
	if (!name)
		return;

	JSONW_StringValue(name, "%d", number);
}

void JSONW_HexValue(const char* name, uint64_t number)
{
	if (!name)
		return;

	JSONW_StringValue(name, Q_itohex(number, qtrue, qtrue));
}

void JSONW_BooleanValue(const char* name, qbool value)
{
	JSONW_StringValue(name, value ? "true" : "false");
}

void JSONW_StringValue(const char* name, PRINTF_FORMAT_STRING const char* format, ...)
{
	static char buffer[4096];

	if (!name || !format || *format == '\0')
		return;

	va_list ap;
	va_start(ap, format);
	Q_vsnprintf(buffer, sizeof(buffer), format, ap);
	va_end(ap);

	if (writer.itemIndices[writer.level] > 0)
		JSONW_Write(", ");

	JSONW_WriteNewLine();
	JSONW_Write("\"");
	JSONW_Write(name);
	JSONW_Write("\": \"");
	JSONW_WriteClean(buffer);
	JSONW_Write("\"");
	++writer.itemIndices[writer.level];
}

void JSONW_UnnamedHex(uint64_t number)
{
	JSONW_UnnamedString(Q_itohex(number, qtrue, qtrue));
}

void JSONW_UnnamedString(PRINTF_FORMAT_STRING const char* format, ...)
{
	static char buffer[4096];

	if (!format || *format == '\0')
		return;

	va_list ap;
	va_start(ap, format);
	Q_vsnprintf(buffer, sizeof(buffer), format, ap);
	va_end(ap);

	if (writer.itemIndices[writer.level] > 0)
		JSONW_Write(", ");

	JSONW_WriteNewLine();
	JSONW_Write("\"");
	JSONW_WriteClean(buffer);
	JSONW_Write("\"");
	++writer.itemIndices[writer.level];
}
