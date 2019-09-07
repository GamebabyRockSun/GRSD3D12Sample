#pragma once
#include "GRS_Win.h"
#include "GRSException.h"
#include "GRSWnd.h"

#define GRS_MAX_CLASSNAME_LEN 256

class CGRSWndClass : public WNDCLASS
{
public:
	CGRSWndClass(LPCTSTR pszClassName, WNDPROC wndProc, LPCTSTR pszIcon = nullptr, LPCTSTR pszCursor = nullptr, HINSTANCE hInstance = nullptr)
		:WNDCLASS()
	{
		Register(pszClassName, wndProc, pszIcon, pszCursor, hInstance);
	}
	CGRSWndClass(LPCTSTR pszClassName, WNDPROC wndProc, WORD nIDIcon, WORD nIDCursor, HINSTANCE hInstance)
		:WNDCLASS()
	{
		Register(pszClassName, wndProc, MAKEINTRESOURCE(nIDIcon), MAKEINTRESOURCE(nIDCursor), hInstance);
	}

	CGRSWndClass(LPCTSTR pszClassName, LPCTSTR pszIcon = nullptr, LPCTSTR pszCursor = nullptr, HINSTANCE hInstance = nullptr)
		:WNDCLASS()
	{
		Register(pszClassName, CGRSWnd::WindowProc, pszIcon, pszCursor, hInstance);
	}
public:
	BOOL Register(LPCTSTR pszClassName,WNDPROC wndProc, LPCTSTR pszIcon = nullptr, LPCTSTR pszCursor = nullptr, HINSTANCE hInstance = nullptr)
	{
		BOOL bRet = TRUE;
		try
		{
			if (nullptr == hInstance)
			{
				hInstance = (HINSTANCE)GetModuleHandle(nullptr);
			}

			style = CS_GLOBALCLASS;
			lpfnWndProc = wndProc;
			hInstance = hInstance;

			if (nullptr != pszIcon)
			{
				hIcon = LoadIcon(hInstance, pszIcon);
			}
			else
			{
				hIcon = LoadIcon(0, IDI_APPLICATION);
			}

			if (nullptr != pszCursor)
			{
				hCursor = LoadCursor(hInstance, pszCursor);
			}
			else
			{
				hCursor = LoadCursor(nullptr, IDC_ARROW);
			}

			hbrBackground = (HBRUSH)GetStockObject(NULL_BRUSH);
			lpszClassName = pszClassName;

			ATOM am = ::RegisterClass(this);
			if (NULL == am)
			{
				GRS_THROW_LASTERROR();
			}
		}
		catch (CGRSException& e)
		{
			bRet = FALSE;
			GRS_TRACE_EXPW(CGRSWnd::Register, e);
		}
		return bRet;
	}
};