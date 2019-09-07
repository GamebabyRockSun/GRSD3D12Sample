#pragma once
#include "GRS_Win.h"

//用于WM_CHAR消息处理LPARAM参数的位域结构体
struct ST_GRS_WMCHAR_LPARAM
{
	DWORD m_dwRepCnt : 16;	//重复次数
	DWORD m_dwScanCode : 8;	//键盘扫描码,用于OEM键盘
	DWORD m_dwExKey : 1;	//是否按下右手的ALT+CTRL键
	DWORD m_dwReserved : 4;	//保留,永远为0
	DWORD m_dwALTKey : 1;	//为1 表示ALT键按下
	DWORD m_dwPrevKeyState : 1;	//为1 表示接到WM_CHAR消息时键是按下的,否则键已弹起
	DWORD m_dwKeyReleased : 1;	//为1 表示键已释放
};

class CGRSWndMsgProc
{
public:
	CGRSWndMsgProc()
	{

	}

	virtual ~CGRSWndMsgProc()
	{

	}

public:
	virtual LRESULT CALLBACK WndMsgProc( HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam ) = 0;
};