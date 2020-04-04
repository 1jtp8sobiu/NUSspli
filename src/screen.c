#include <string.h>
#include <stdbool.h>
#include <stdint.h>

#include <coreinit/cache.h>
#include <coreinit/memdefaultheap.h>
#include <coreinit/screen.h>
#include <coreinit/thread.h>
#include <coreinit/time.h>
#include <whb/log.h>

#include "screen.h"

size_t tvBufferSize;
size_t drcBufferSize;
void *tvBuffer;
void *drcBuffer;

bool initScreen() {
	OSScreenInit();

	tvBufferSize = OSScreenGetBufferSizeEx(SCREEN_TV);
	drcBufferSize = OSScreenGetBufferSizeEx(SCREEN_DRC);

	tvBuffer = MEMAllocFromDefaultHeapEx(tvBufferSize, 0x100);
	drcBuffer = MEMAllocFromDefaultHeapEx(drcBufferSize, 0x100);

	if (!tvBuffer || !drcBuffer) {
		WHBLogPrintf("Error initialising screen library");
		shutdownScreen();
		return false;
	}

	OSScreenSetBufferEx(SCREEN_TV, tvBuffer);
	OSScreenSetBufferEx(SCREEN_DRC, drcBuffer);

	OSScreenEnableEx(SCREEN_TV, true);
	OSScreenEnableEx(SCREEN_DRC, true);
		WHBLogPrintf("Screen library initialised successfully");
	return true;
}
void shutdownScreen() {
	if (tvBuffer) MEMFreeToDefaultHeap(tvBuffer);
	if (drcBuffer) MEMFreeToDefaultHeap(drcBuffer);
	
	OSScreenShutdown();
}

void startRefresh() {
	colorStartRefresh(SCREEN_COLOR_BLACK);
}

OSTime lastFlip = 0;
bool limitFps = true;
void endRefresh() {
	DCFlushRange(tvBuffer, tvBufferSize);
	DCFlushRange(drcBuffer, drcBufferSize);
	
	if(limitFps)
	{
		OSTime toSleep = OSMillisecondsToTicks(1);
		OSTime now = OSGetSystemTime();
		while(OSTicksToMilliseconds(now - lastFlip) < 33) // 30 FPS
		{
			OSSleepTicks(toSleep);
			now = OSGetSystemTime();
		}
		lastFlip = now;
	}
	OSScreenFlipBuffersEx(SCREEN_TV);
	OSScreenFlipBuffersEx(SCREEN_DRC);
}

void paintLine(uint32_t column, uint32_t colour)
{
	column *= 24;
	column += 45;
	uint32_t c2 = column + 1;
	for(uint32_t x = 53; x < MAX_X_DRC; x++)
	{
		OSScreenPutPixelEx(SCREEN_DRC, x, column, colour);
		OSScreenPutPixelEx(SCREEN_DRC, x, c2, colour);
		OSScreenPutPixelEx(SCREEN_TV, x, column, colour);
		OSScreenPutPixelEx(SCREEN_TV, x, c2, colour);
	}
	for(uint32_t x = MAX_X_DRC; x < MAX_X_TV; x++)
	{
		OSScreenPutPixelEx(SCREEN_TV, x, column, colour);
		OSScreenPutPixelEx(SCREEN_TV, x, c2, colour);
	}
}

inline void writeDRC(uint32_t row, uint32_t column, const char* str)
{
	OSScreenPutFontEx(SCREEN_DRC, row, column, str);
}

inline void writeTV(uint32_t row, uint32_t column, const char* str)
{
	OSScreenPutFontEx(SCREEN_TV, row, column, str);
}

void write(uint32_t row, uint32_t column, const char* str) { //Write to the two screens
	uint32_t dRow;
	if(row > 64)
	{
		uint32_t len = strlen(str);
		row = 98 - len;
		dRow = 62 - len;
	}
	else
		dRow = row;
	writeTV(row, column, str);
	writeDRC(dRow, column, str);
}

struct DownloadLogList;
typedef struct DownloadLogList DownloadLogList;
struct DownloadLogList
{
	char *line;
	DownloadLogList *nextEntry;
};

DownloadLogList *downloadLogList[2] = { NULL, NULL };
void addToDownloadLog(char* str) {
	WHBLogPrintf(str);
	
	DownloadLogList *newTVEntry = MEMAllocFromDefaultHeap(sizeof(DownloadLogList));
	if(newTVEntry ==  NULL)
		return;
	DownloadLogList *newDRCEntry = MEMAllocFromDefaultHeap(sizeof(DownloadLogList));
	if(newDRCEntry ==  NULL)
	{
		MEMFreeToDefaultHeap(newTVEntry);
		return;
	}
	
	//TODO: We copy the string here for fast porting purposes
	newTVEntry->line = newDRCEntry->line = MEMAllocFromDefaultHeap(sizeof(char) * (strlen(str) + 1));
	if(newTVEntry->line == NULL)
	{
		MEMFreeToDefaultHeap(newTVEntry);
		MEMFreeToDefaultHeap(newDRCEntry);
		return;
	}
	strcpy(newTVEntry->line, str);
	
	newTVEntry->nextEntry = newDRCEntry->nextEntry = NULL;
	
	if(downloadLogList[0] == NULL)
	{
		downloadLogList[0] = newTVEntry;
		downloadLogList[1] = newDRCEntry;
		return;
	}
	
	DownloadLogList *lastTV;
	int i = 0;
	for(DownloadLogList *c = downloadLogList[0]; c != NULL; c = c->nextEntry)
	{
		
		i++;
		lastTV = c;
	}
	if(i == MAX_LINE_TV)
	{
		DownloadLogList *tmpList = downloadLogList[0];
		downloadLogList[0] = tmpList->nextEntry;
		MEMFreeToDefaultHeap(tmpList->line);
		MEMFreeToDefaultHeap(tmpList);
	}
	
	DownloadLogList *lastDRC;
	i = 0;
	for(DownloadLogList *c = downloadLogList[1]; c != NULL; c = c->nextEntry)
	{
		
		i++;
		lastDRC = c;
	}
	if(i == MAX_LINE_DRC)
	{
		DownloadLogList *tmpList = downloadLogList[1];
		downloadLogList[1] = tmpList->nextEntry;
		MEMFreeToDefaultHeap(tmpList);
	}
	
	lastTV->nextEntry = newTVEntry;
	lastDRC->nextEntry = newDRCEntry;
}
void clearDownloadLog() {
	DownloadLogList *tmpList;
	while(downloadLogList[0] != NULL)
	{
		tmpList = downloadLogList[0];
		downloadLogList[0] = tmpList->nextEntry;
		MEMFreeToDefaultHeap(tmpList->line);
		MEMFreeToDefaultHeap(tmpList);
	}
	while(downloadLogList[1] != NULL)
	{
		tmpList = downloadLogList[1];
		downloadLogList[1] = tmpList->nextEntry;
		MEMFreeToDefaultHeap(tmpList);
	}
}
void writeDownloadLog() {
	paintLine(2, SCREEN_COLOR_WHITE);
	int i = 3;
	for(DownloadLogList *entry = downloadLogList[0]; entry != NULL; entry = entry->nextEntry)
		writeTV(0, i++, entry->line);
	i = 3;
	for(DownloadLogList *entry = downloadLogList[1]; entry != NULL; entry = entry->nextEntry)
		writeDRC(0, i++, entry->line);
}

void colorStartRefresh(uint32_t color) {
	OSScreenClearBufferEx(SCREEN_TV, color);
	OSScreenClearBufferEx(SCREEN_DRC, color);
}

void errorScreen(int line, ErrorOptions option) {
	paintLine(line++, SCREEN_COLOR_WHITE);
	switch (option) {
		case B_RETURN:
			write(0, line, "Press (B) to return");
			return;
		case B_RETURN__A_CONTINUE:
			write(0, line++, "Press (A) to continue");
			write(0, line, "Press (B) to return");
			return;
		case B_RETURN__Y_RETRY:
			write(0, line++, "Press (Y) to retry");
			write(0, line, "Press (B) to return");
			return;
	}
}

void writeRetry() {
	startRefresh();
	write(0, 0, "Preparing the retry...");
	writeDownloadLog();
	endRefresh();
}