#pragma once
#include <SDKDDKVer.h>
#define WIN32_LEAN_AND_MEAN // 从 Windows 头中排除极少使用的资料
#include <windows.h>
#include <tchar.h>
#include <strsafe.h>
#include <atlconv.h>
#include <atlcoll.h>

#define GRS_MAX_OUTPUT_BUFFER_LEN 1024

__inline void GRSInitConsole(LPCTSTR pszTitle)
{
	::AllocConsole(); 
	SetConsoleTitle(pszTitle);
}

__inline void GRSUninitConsole()
{
	::_tsystem(_T("PAUSE")); 
	::FreeConsole();
}

__inline void GRSPrintfW(LPCWSTR pszFormat, ...)
{
	WCHAR pBuffer[GRS_MAX_OUTPUT_BUFFER_LEN] = {};
	size_t szStrLen = 0;
	va_list va;
	va_start(va, pszFormat);
	if (S_OK != ::StringCchVPrintfW(pBuffer, _countof(pBuffer), pszFormat, va))
	{
		va_end(va);
		return;
	}
	va_end(va);
	
	StringCchLengthW(pBuffer, GRS_MAX_OUTPUT_BUFFER_LEN, &szStrLen);

	(szStrLen > 0) ?
		WriteConsoleW(GetStdHandle(STD_OUTPUT_HANDLE), pBuffer, (DWORD)szStrLen, nullptr, nullptr)
		: 0;
}

__inline void GRSPrintfA(LPCSTR pszFormat, ...)
{
	CHAR pBuffer[GRS_MAX_OUTPUT_BUFFER_LEN] = {};
	size_t szStrLen = 0;

	va_list va;
	va_start(va, pszFormat);
	if (S_OK != ::StringCchVPrintfA(pBuffer, _countof(pBuffer), pszFormat, va))
	{
		va_end(va);
		return;
	}
	va_end(va);

	StringCchLengthA(pBuffer, GRS_MAX_OUTPUT_BUFFER_LEN, &szStrLen);

	(szStrLen > 0) ?
		WriteConsoleA(GetStdHandle(STD_OUTPUT_HANDLE), pBuffer, (DWORD)szStrLen, nullptr, nullptr)
		: 0;
}

__inline void GRSSetConsoleMax()
{
	HANDLE hStdout = GetStdHandle(STD_OUTPUT_HANDLE);

	COORD NewSize = GetLargestConsoleWindowSize(hStdout);//获得控制台最大坐标，坐标以字符数为单位
	NewSize.X -= 1;
	NewSize.Y -= 1;
	SMALL_RECT DisplayArea = { 0,0,0,0 };
	DisplayArea.Right = NewSize.X;
	DisplayArea.Bottom = NewSize.Y;
	SetConsoleWindowInfo(hStdout, TRUE, &DisplayArea);    //设置控制台大小

	CONSOLE_SCREEN_BUFFER_INFO stCSBufInfo = {};
	GetConsoleScreenBufferInfo(hStdout, &stCSBufInfo);

	stCSBufInfo.dwSize.Y = 9999;
	SetConsoleScreenBufferSize(hStdout, stCSBufInfo.dwSize);

	//控制台已经最大化，但是初始位置不在屏幕左上角，添加如下代码
	HWND hwnd = GetConsoleWindow();
	ShowWindow(hwnd, SW_MAXIMIZE);    //窗体最大化
}

__inline void GRSConsolePause()
{
	::_tsystem(_T("PAUSE"));
}
// 设置当前光标显示的位置
__inline void GRSSetConsoleCursorLocate(int x, int y)
{//定位到(x,y)
	HANDLE out_handle = GetStdHandle(STD_OUTPUT_HANDLE);
	COORD loc;
	loc.X = x;
	loc.Y = y;
	SetConsoleCursorPosition(out_handle, loc);
}

// 移动光标到指定的位置
__inline void GRSMoveConsoleCursor(int x, int y)
//移动光标
{
	HANDLE out_handle = GetStdHandle(STD_OUTPUT_HANDLE);
	CONSOLE_SCREEN_BUFFER_INFO info;
	GetConsoleScreenBufferInfo(out_handle, &info);
	COORD loc = {};
	loc.X = info.dwCursorPosition.X + x;
	loc.Y = info.dwCursorPosition.Y + y;
	SetConsoleCursorPosition(out_handle, loc);
}


__inline int GRSGetConsoleCurrentY()
{
	HANDLE out_handle = GetStdHandle(STD_OUTPUT_HANDLE);
	CONSOLE_SCREEN_BUFFER_INFO info = {};
	GetConsoleScreenBufferInfo(out_handle, &info);
	return info.dwCursorPosition.Y;
}

__inline void GRSPrintBlank(int iBlank)
{
	HANDLE out_handle = GetStdHandle(STD_OUTPUT_HANDLE);
	CONSOLE_SCREEN_BUFFER_INFO info = {};
	GetConsoleScreenBufferInfo(out_handle, &info);
	for (int i = 0; i < iBlank; i++)
	{
		info.dwCursorPosition.X += 4;
	}
	SetConsoleCursorPosition(out_handle, info.dwCursorPosition);
}
// 光标位置的栈，用数组模拟
void GRSPushConsoleCursor();
void GRSPopConsoleCursor();

// 将某个指针与输出行号位置绑定并保存
void GRSSaveConsoleLine(void* p, SHORT sLine);
// 根据指针值读取出对应的行号
BOOL GRSFindConsoleLine(void* p, SHORT& sLine);