/*
===========================================================================
Copyright (C) 2005-2006 Tim Angus
Copyright (C) 2018-2023 Gian 'myT' Schellenbaum

This file is part of Quake III Arena source code.

Quake III Arena source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

Quake III Arena source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Quake III Arena source code; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/
// captures audio/video to an AVI (raw BGR or MJPEG, PCM) file or pipes raw data to FFmpeg

#include "client.h"
#include "snd_local.h"


#define INDEX_FILE_EXTENSION ".index.dat"

#define MAX_RIFF_CHUNKS 16

#define PCM_BUFFER_SIZE 44100

#define MAX_AVI_BUFFER 4096

// flags for the "AVIH" RIFF chunk
#define AVIH_HASINDEX_BIT      0x010
#define AVIH_ISINTERLEAVED_BIT 0x100


struct audioFormat_t {
	int rate;
	int format;
	int channels;
	int bits;

	int sampleSize;
	int totalBytes;
};

struct aviFileData_t {
	qbool isPiped;
	qbool isFileOpen;
	fileHandle_t mainFile;
	char fileName[256 + 64]; // extra room for the "videos/" prefix and name suffix
	int fileSize;
	int moviOffset;
	int moviSize;

	fileHandle_t indexFile;
	int numIndices;

	int frameRate;
	int framePeriod;
	int width;
	int height;
	int numVideoFrames;
	int maxRecordSize;
	qbool useMotionJpeg;

	qbool hasAudio;
	audioFormat_t audioFormat;
	int numAudioFrames;

	int chunkStack[MAX_RIFF_CHUNKS];
	int chunkStackTop;

	byte* captureBuffer;
	byte* encodeBuffer;
};

enum closeMode_t {
	CM_SEQUENCE_COMPLETE,  //     last in the sequence -> safe to clear sounds etc.
	CM_SEQUENCE_INCOMPLETE // NOT last in the sequence -> we'll open a new one
};


static aviFileData_t afd;
static byte buffer[MAX_AVI_BUFFER];
static int bufferWriteIndex;


static qbool CloseAVI( closeMode_t closeMode );


static void AbortCapture( const char* message )
{
	Com_Printf("^1ERROR: %s\n", message);
	Cbuf_AddText(clc.newDemoPlayer ? "demo_stopvideo\n" : "stopvideo\n");
}


static void Safe_FS_Write( const void* buff, int len, fileHandle_t f )
{
	if (f <= 0) {
		return;
	}

	if (FS_Write(buff, len, f) < len) {
		AbortCapture("Failed to write to video file/pipe");
	}
}


static void Safe_FS_Close( fileHandle_t* file )
{
	if (*file > 0) {
		FS_FCloseFile(*file);
		*file = 0;
	}
}


static void Safe_Z_Free( byte** memory )
{
	if (*memory != NULL) {
		Z_Free(*memory);
		*memory = NULL;
	}
}


static void WRITE_STRING( const char* s )
{
	Com_Memcpy(&buffer[bufferWriteIndex], s, strlen(s));
	bufferWriteIndex += strlen(s);
}


static void WRITE_4BYTES( int x )
{
	buffer[bufferWriteIndex + 0] = (byte)((x >> 0) & 0xFF);
	buffer[bufferWriteIndex + 1] = (byte)((x >> 8) & 0xFF);
	buffer[bufferWriteIndex + 2] = (byte)((x >> 16) & 0xFF);
	buffer[bufferWriteIndex + 3] = (byte)((x >> 24) & 0xFF);
	bufferWriteIndex += 4;
}


static void WRITE_2BYTES( int x )
{
	buffer[bufferWriteIndex + 0] = (byte)((x >> 0) & 0xFF);
	buffer[bufferWriteIndex + 1] = (byte)((x >> 8) & 0xFF);
	bufferWriteIndex += 2;
}


static void START_CHUNK( const char* s )
{
	if (afd.chunkStackTop == MAX_RIFF_CHUNKS) {
		AbortCapture("Top of chunkstack breached");
		return;
	}

	afd.chunkStack[afd.chunkStackTop] = bufferWriteIndex;
	afd.chunkStackTop++;
	WRITE_STRING(s);
	WRITE_4BYTES(0);
}


static void END_CHUNK()
{
	if (afd.chunkStackTop <= 0) {
		AbortCapture("Bottom of chunkstack breached");
		return;
	}

	const int endIndex = bufferWriteIndex;
	afd.chunkStackTop--;
	bufferWriteIndex = afd.chunkStack[afd.chunkStackTop];
	bufferWriteIndex += 4;
	WRITE_4BYTES(endIndex - bufferWriteIndex - 4);
	bufferWriteIndex = endIndex;
	bufferWriteIndex = PAD(bufferWriteIndex, 2);
}


void CL_WriteAVIHeader()
{
	bufferWriteIndex = 0;
	afd.chunkStackTop = 0;

	START_CHUNK("RIFF");
	{
		WRITE_STRING("AVI ");
		{
			START_CHUNK("LIST");
			{
				WRITE_STRING("hdrl");
				WRITE_STRING("avih");
				WRITE_4BYTES(56);                                //"avih" "chunk" size
				WRITE_4BYTES(afd.framePeriod);                   //dwMicroSecPerFrame
				WRITE_4BYTES(afd.maxRecordSize * afd.frameRate); //dwMaxBytesPerSec
				WRITE_4BYTES(0);                                 //dwReserved1
				if (afd.isPiped)
					WRITE_4BYTES(AVIH_ISINTERLEAVED_BIT);
				else
					WRITE_4BYTES(AVIH_ISINTERLEAVED_BIT | AVIH_HASINDEX_BIT);
				WRITE_4BYTES(afd.numVideoFrames); //dwTotalFrames
				WRITE_4BYTES(0);                  //dwInitialFrame

				if (afd.hasAudio) //dwStreams
					WRITE_4BYTES(2);
				else
					WRITE_4BYTES(1);

				WRITE_4BYTES(afd.maxRecordSize); //dwSuggestedBufferSize
				WRITE_4BYTES(afd.width);         //dwWidth
				WRITE_4BYTES(afd.height);        //dwHeight
				WRITE_4BYTES(0);                 //dwReserved[ 0 ]
				WRITE_4BYTES(0);                 //dwReserved[ 1 ]
				WRITE_4BYTES(0);                 //dwReserved[ 2 ]
				WRITE_4BYTES(0);                 //dwReserved[ 3 ]

				START_CHUNK("LIST");
				{
					WRITE_STRING("strl");
					WRITE_STRING("strh");
					WRITE_4BYTES(56);     //"strh" "chunk" size
					WRITE_STRING("vids"); //fccType

					//fccHandler
					if (!afd.isPiped && afd.useMotionJpeg)
						WRITE_STRING("MJPG");
					else
						WRITE_4BYTES(0); //raw BGR

					WRITE_4BYTES(0); //dwFlags
					WRITE_4BYTES(0); //dwPriority
					WRITE_4BYTES(0); //dwInitialFrame

					WRITE_4BYTES(1);                  //dwTimescale
					WRITE_4BYTES(afd.frameRate);      //dwDataRate
					WRITE_4BYTES(0);                  //dwStartTime
					WRITE_4BYTES(afd.numVideoFrames); //dwDataLength

					WRITE_4BYTES(afd.maxRecordSize); //dwSuggestedBufferSize
					WRITE_4BYTES(-1);                //dwQuality
					WRITE_4BYTES(0);                 //dwSampleSize
					WRITE_2BYTES(0);                 //rcFrame
					WRITE_2BYTES(0);                 //rcFrame
					WRITE_2BYTES(afd.width);         //rcFrame
					WRITE_2BYTES(afd.height);        //rcFrame

					WRITE_STRING("strf");
					WRITE_4BYTES(40);         //"strf" "chunk" size
					WRITE_4BYTES(40);         //biSize
					WRITE_4BYTES(afd.width);  //biWidth
					WRITE_4BYTES(afd.height); //biHeight
					WRITE_2BYTES(1);          //biPlanes
					WRITE_2BYTES(24);         //biBitCount

					//biCompression and biSizeImage
					if (!afd.isPiped && afd.useMotionJpeg) {
						// We specify the pixel count
						WRITE_STRING("MJPG");
						WRITE_4BYTES(afd.width * afd.height);
					} else {
						// We must either specify 0 or the total byte count per image
						WRITE_4BYTES(0); // raw BGR
						WRITE_4BYTES(afd.width * afd.height * 3);
					}

					WRITE_4BYTES(0); //biXPelsPetMeter
					WRITE_4BYTES(0); //biYPelsPetMeter
					WRITE_4BYTES(0); //biClrUsed
					WRITE_4BYTES(0); //biClrImportant
				}
				END_CHUNK();

				if (afd.hasAudio) {
					START_CHUNK("LIST");
					{
						WRITE_STRING("strl");
						WRITE_STRING("strh");
						WRITE_4BYTES(56); //"strh" "chunk" size
						WRITE_STRING("auds");
						WRITE_4BYTES(0); //FCC
						WRITE_4BYTES(0); //dwFlags
						WRITE_4BYTES(0); //dwPriority
						WRITE_4BYTES(0); //dwInitialFrame

						WRITE_4BYTES(afd.audioFormat.sampleSize);                              //dwTimescale
						WRITE_4BYTES(afd.audioFormat.sampleSize * afd.audioFormat.rate);       //dwDataRate
						WRITE_4BYTES(0);                                                       //dwStartTime
						WRITE_4BYTES(afd.audioFormat.totalBytes / afd.audioFormat.sampleSize); //dwDataLength

						WRITE_4BYTES(0);                          //dwSuggestedBufferSize
						WRITE_4BYTES(-1);                         //dwQuality
						WRITE_4BYTES(afd.audioFormat.sampleSize); //dwSampleSize
						WRITE_2BYTES(0);                          //rcFrame
						WRITE_2BYTES(0);                          //rcFrame
						WRITE_2BYTES(0);                          //rcFrame
						WRITE_2BYTES(0);                          //rcFrame

						WRITE_STRING("strf");
						WRITE_4BYTES(18);                                                //"strf" "chunk" size
						WRITE_2BYTES(afd.audioFormat.format);                            //wFormatTag
						WRITE_2BYTES(afd.audioFormat.channels);                          //nChannels
						WRITE_4BYTES(afd.audioFormat.rate);                              //nSamplesPerSec
						WRITE_4BYTES(afd.audioFormat.sampleSize * afd.audioFormat.rate); //nAvgBytesPerSec
						WRITE_2BYTES(afd.audioFormat.sampleSize);                        //nBlockAlign
						WRITE_2BYTES(afd.audioFormat.bits);                              //wBitsPerSample
						WRITE_2BYTES(0);                                                 //cbSize
					}
					END_CHUNK();
				}
			}
			END_CHUNK();

			afd.moviOffset = bufferWriteIndex;

			START_CHUNK("LIST");
			{
				WRITE_STRING("movi");
			}
		}
	}
}


// creates an AVI file and gets it into a state where writing the actual data can begin
static qbool OpenAVI( const char* fileNameNoExt, qbool reOpen )
{
	static char avi_fileNameNoExt[MAX_QPATH];
	static int avi_fileNameIndex;

	if (!afd.isPiped && reOpen)
		CloseAVI(CM_SEQUENCE_INCOMPLETE);

	if (afd.isFileOpen)
		return qfalse;

	Com_Memset(&afd, 0, sizeof(aviFileData_t));
	afd.isPiped = cl_ffmpeg->integer != 0;

	// don't start if a framerate has not been chosen
	if (cl_aviFrameRate->integer <= 0) {
		Com_Printf("^1ERROR: cl_aviFrameRate must be >= 1\n");
		return qfalse;
	}

	if (afd.isPiped) {
		const char* fileExtension = cl_ffmpegOutExt->string;
		while (*fileExtension == '.') {
			fileExtension++;
		}

		if (cl_ffmpegOutPath->string[0] == '\0') {
			const char* homePath = Cvar_VariableString("fs_homepath");
			const char* final = FS_BuildOSPath(homePath, NULL, va("videos/%s.%s", fileNameNoExt, fileExtension));
			Com_sprintf(afd.fileName, sizeof(afd.fileName), final);
		} else {
			Com_sprintf(afd.fileName, sizeof(afd.fileName), "%s/%s.%s", cl_ffmpegOutPath->string, fileNameNoExt, fileExtension);
			FS_ReplaceSeparators(afd.fileName);
		}

		// @NOTE: can't double quote the executable's path with _popen
		const char* pipeCommand;
		if (cl_ffmpegLog->integer != 0) {
			pipeCommand = va("%s -f avi -i - %s -y \"%s\" > \"%s.log.txt\" 2>&1",
			                 cl_ffmpegExePath->string, cl_ffmpegCommand->string, afd.fileName, afd.fileName);
		} else {
			pipeCommand = va("%s -f avi -i - %s -y \"%s\"",
			                 cl_ffmpegExePath->string, cl_ffmpegCommand->string, afd.fileName);
		}

		afd.mainFile = FS_OpenPipeWrite(pipeCommand);
		if (afd.mainFile <= 0) {
			Com_Printf("^1ERROR: failed to open video process for writing\n");
			return qfalse;
		}
	} else {
		if (reOpen) {
			avi_fileNameIndex++;
		} else {
			Q_strncpyz(avi_fileNameNoExt, fileNameNoExt, sizeof(avi_fileNameNoExt));
			avi_fileNameIndex = 0;
		}
		Com_sprintf(afd.fileName, sizeof(afd.fileName), "videos/%s_%03d.avi", avi_fileNameNoExt, avi_fileNameIndex);

		afd.mainFile = FS_FOpenFileWrite(afd.fileName);
		afd.indexFile = FS_FOpenFileWrite(va("%s" INDEX_FILE_EXTENSION, afd.fileName));
		if (afd.mainFile <= 0 || afd.indexFile <= 0) {
			Safe_FS_Close(&afd.mainFile);
			Safe_FS_Close(&afd.indexFile);
			Com_Printf("^1ERROR: failed to open video file for writing\n");
			return qfalse;
		}
	}

	afd.frameRate = cl_aviFrameRate->integer;
	afd.framePeriod = (int)(1000000.0f / afd.frameRate);
	afd.width = cls.glconfig.vidWidth;
	afd.height = cls.glconfig.vidHeight;
	afd.useMotionJpeg = !afd.isPiped && cl_aviMotionJpeg->integer != 0;

	const int maxByteCount = PAD(afd.width, 4) * afd.height * 4;
	afd.captureBuffer = (byte*)Z_Malloc(maxByteCount);
	afd.encodeBuffer = (byte*)Z_Malloc(maxByteCount);

	afd.audioFormat.rate = dma.speed;
	afd.audioFormat.format = WAV_FORMAT_PCM;
	afd.audioFormat.channels = dma.channels;
	afd.audioFormat.bits = dma.samplebits;
	afd.audioFormat.sampleSize = (afd.audioFormat.bits / 8) * afd.audioFormat.channels;

	if (afd.audioFormat.rate % afd.frameRate) {
		int suggestRate = afd.frameRate;
		while ((afd.audioFormat.rate % suggestRate) && suggestRate)
			--suggestRate;
		Com_Printf("^3WARNING: cl_aviFrameRate is not a divisor of the audio rate, suggesting %d\n", suggestRate);
	}

	afd.hasAudio = qfalse;
	if (Cvar_VariableIntegerValue("s_initsound")) {
		afd.hasAudio = (afd.audioFormat.bits == 16 && afd.audioFormat.channels == 2);
		if (!afd.hasAudio) {
			Com_Printf("^3WARNING: Audio capture needs 16-bit stereo");
		}
	}

	// This doesn't write a real header, but allocates the
	// correct amount of space at the beginning of the file
	CL_WriteAVIHeader();

	Safe_FS_Write(buffer, bufferWriteIndex, afd.mainFile);
	afd.fileSize = bufferWriteIndex;

	if (!afd.isPiped) {
		bufferWriteIndex = 0;
		START_CHUNK("idx1");
		Safe_FS_Write(buffer, bufferWriteIndex, afd.indexFile);
	}

	afd.moviSize = 4; // For the "movi" header signature
	afd.isFileOpen = qtrue;

	Com_Printf("Recording to %s\n", afd.fileName);

	return qtrue;
}


qbool CL_OpenAVIForWriting( const char* fileNameNoExt )
{
	return OpenAVI(fileNameNoExt, qfalse);
}


static qbool CheckFileSize( int bytesToAdd )
{
	const unsigned int newFileSize =
	    afd.fileSize +          // Current file size
	    bytesToAdd +            // What we want to add
	    (afd.numIndices * 16) + // The index
	    4;                      // The index size

	// "AVI " limit is 1 GB (AVI 1.0)
	// "AVIX" limit is 2 GB (AVI 2.0 / OpenDML)
	if (!afd.isPiped && newFileSize > (1 << 30)) {
		OpenAVI(NULL, qtrue);
		return qtrue;
	}

	return qfalse;
}


void CL_WriteAVIVideoFrame( const byte* imageBuffer, int size )
{
	if (!afd.isFileOpen)
		return;

	// Chunk header + contents + padding
	if (CheckFileSize(8 + size + 2))
		return;

	bufferWriteIndex = 0;
	WRITE_STRING("00dc");
	WRITE_4BYTES(size);

	const int chunkOffset = afd.fileSize - afd.moviOffset - 8;
	const int chunkSize = 8 + size;
	const int paddingSize = PAD(size, 2) - size;
	const byte padding[4] = { 0 };
	Safe_FS_Write(buffer, 8, afd.mainFile);
	Safe_FS_Write(imageBuffer, size, afd.mainFile);
	Safe_FS_Write(padding, paddingSize, afd.mainFile);
	afd.fileSize += (chunkSize + paddingSize);

	afd.numVideoFrames++;
	afd.moviSize += (chunkSize + paddingSize);

	if (size > afd.maxRecordSize)
		afd.maxRecordSize = size;

	// Write index data
	if (!afd.isPiped) {
		bufferWriteIndex = 0;
		WRITE_STRING("00dc");      //dwIdentifier
		WRITE_4BYTES(0x00000010);  //dwFlags (all frames are KeyFrames)
		WRITE_4BYTES(chunkOffset); //dwOffset
		WRITE_4BYTES(size);        //dwLength
		Safe_FS_Write(buffer, 16, afd.indexFile);
	}

	afd.numIndices++;
}


void CL_WriteAVIAudioFrame( const byte* pcmBuffer, int size )
{
	static byte pcmCaptureBuffer[PCM_BUFFER_SIZE] = { 0 };
	static int bytesInBuffer = 0;

	if (!afd.hasAudio)
		return;

	if (!afd.isFileOpen)
		return;

	// Chunk header + contents + padding
	if (CheckFileSize(8 + bytesInBuffer + size + 2))
		return;

	if (bytesInBuffer + size > PCM_BUFFER_SIZE) {
		Com_Printf("^3WARNING: Audio capture buffer overflow -- truncating\n");
		size = PCM_BUFFER_SIZE - bytesInBuffer;
	}

	Com_Memcpy(&pcmCaptureBuffer[bytesInBuffer], pcmBuffer, size);
	bytesInBuffer += size;

	// Only write if we have a frame's worth of audio
	const int bytesToWrite = (int)ceil((float)afd.audioFormat.rate / (float)afd.frameRate) * afd.audioFormat.sampleSize;
	if (bytesInBuffer >= bytesToWrite) {
		bufferWriteIndex = 0;
		WRITE_STRING("01wb");
		WRITE_4BYTES(bytesInBuffer);

		const int chunkOffset = afd.fileSize - afd.moviOffset - 8;
		const int chunkSize = 8 + bytesInBuffer;
		const int paddingSize = PAD(bytesInBuffer, 2) - bytesInBuffer;
		const byte padding[4] = { 0 };
		Safe_FS_Write(buffer, 8, afd.mainFile);
		Safe_FS_Write(pcmCaptureBuffer, bytesInBuffer, afd.mainFile);
		Safe_FS_Write(padding, paddingSize, afd.mainFile);
		afd.fileSize += (chunkSize + paddingSize);

		afd.numAudioFrames++;
		afd.moviSize += (chunkSize + paddingSize);
		afd.audioFormat.totalBytes += bytesInBuffer;

		// Write index data
		if (!afd.isPiped) {
			bufferWriteIndex = 0;
			WRITE_STRING("01wb");        //dwIdentifier
			WRITE_4BYTES(0);             //dwFlags
			WRITE_4BYTES(chunkOffset);   //dwOffset
			WRITE_4BYTES(bytesInBuffer); //dwLength
			Safe_FS_Write(buffer, 16, afd.indexFile);
		}

		afd.numIndices++;

		bytesInBuffer = 0;
	}
}


void CL_TakeVideoFrame()
{
	if (!afd.isFileOpen)
		return;

	re.TakeVideoFrame(afd.width, afd.height, afd.captureBuffer, afd.encodeBuffer, afd.useMotionJpeg);
}


static qbool FixIndices()
{
	int indexSize = afd.numIndices * 16;
	FS_Seek(afd.indexFile, 4, FS_SEEK_SET);
	bufferWriteIndex = 0;
	WRITE_4BYTES(indexSize);
	Safe_FS_Write(buffer, bufferWriteIndex, afd.indexFile);
	Safe_FS_Close(&afd.indexFile); // needed in case the follow-up open fails

	// Open the temp index file
	const char* const idxFileName = va("%s" INDEX_FILE_EXTENSION, afd.fileName);
	if ((indexSize = FS_FOpenFileRead(idxFileName, &afd.indexFile, qtrue)) <= 0) {
		return qfalse;
	}

	// Append index to end of avi file
	int indexRemainder = indexSize;
	while (indexRemainder > MAX_AVI_BUFFER) {
		FS_Read(buffer, MAX_AVI_BUFFER, afd.indexFile);
		Safe_FS_Write(buffer, MAX_AVI_BUFFER, afd.mainFile);
		afd.fileSize += MAX_AVI_BUFFER;
		indexRemainder -= MAX_AVI_BUFFER;
	}
	FS_Read(buffer, indexRemainder, afd.indexFile);
	Safe_FS_Write(buffer, indexRemainder, afd.mainFile);
	afd.fileSize += indexRemainder;

	// Close and remove temp index file
	Safe_FS_Close(&afd.indexFile);
	FS_HomeRemove(idxFileName);

	// Write the real header
	FS_Seek(afd.mainFile, 0, FS_SEEK_SET);
	CL_WriteAVIHeader();

	bufferWriteIndex = 4;
	WRITE_4BYTES(afd.fileSize - 8); // "RIFF" size

	bufferWriteIndex = afd.moviOffset + 4; // Skip "LIST"
	WRITE_4BYTES(afd.moviSize);

	Safe_FS_Write(buffer, bufferWriteIndex, afd.mainFile);

	return qtrue;
}


// Closes the AVI file and optionally writes an index chunk
static qbool CloseAVI( closeMode_t closeMode )
{
	if (!afd.isFileOpen)
		return qfalse;

	afd.isFileOpen = qfalse;

	qbool result = qtrue;
	if (!afd.isPiped) {
		result = FixIndices();
	}

	Safe_Z_Free(&afd.captureBuffer);
	Safe_Z_Free(&afd.encodeBuffer);
	Safe_FS_Close(&afd.mainFile);
	Safe_FS_Close(&afd.indexFile);

	Com_Printf("Processed %d:%d V:A frames for %s\n", afd.numVideoFrames, afd.numAudioFrames, afd.fileName);

	if (closeMode == CM_SEQUENCE_COMPLETE) {
		S_StopAllSounds();
	}

	return result;
}


qbool CL_CloseAVI()
{
	return CloseAVI(CM_SEQUENCE_COMPLETE);
}


qbool CL_VideoRecording()
{
	return afd.isFileOpen;
}
