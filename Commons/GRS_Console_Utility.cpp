#include "GRS_Console_Utility.h"

static CAtlMap<void*, SHORT> g_mapPoint2Line;
static CAtlArray<COORD> g_arConsoleCoordStack;

void GRSPushConsoleCursor()//保存光标位置
{
	HANDLE out_handle = GetStdHandle(STD_OUTPUT_HANDLE);
	CONSOLE_SCREEN_BUFFER_INFO info;
	GetConsoleScreenBufferInfo(out_handle, &info);
	g_arConsoleCoordStack.Add(info.dwCursorPosition);
}
void GRSPopConsoleCursor()//恢复光标位置
{
	HANDLE out_handle = GetStdHandle(STD_OUTPUT_HANDLE);
	COORD loc = {};
	if (g_arConsoleCoordStack.GetCount() > 0)
	{
		loc = g_arConsoleCoordStack[g_arConsoleCoordStack.GetCount() - 1];
		SetConsoleCursorPosition(out_handle, loc);
	}
}

void GRSSaveConsoleLine(void* p, SHORT sLine)
{
	g_mapPoint2Line.SetAt(p, sLine);
}

BOOL GRSFindConsoleLine(void* p, SHORT& sLine)
{
	if (g_mapPoint2Line.Lookup(p, sLine))
	{
		return TRUE;
	}
	return FALSE;
}