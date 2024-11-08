/*
 * win32wrapper.c - wrapper to host a simple win32 main program
 * Copyright (C) 2024 Sanjay Rao
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#define UNICODE
#include <windows.h>
#include <stdint.h>
#include <stdio.h>

DWORD WINAPI threadfunc(void *info_in); 
extern const wchar_t *classname_win32wrapper;
extern const wchar_t *windowtitle_win32wrapper;

typedef void ignore;

static struct threadshared {
	HWND hwnd;
	CRITICAL_SECTION cs;
	uint16_t message[512];
} threadshared_global;

int logstring(wchar_t *msg) {
EnterCriticalSection(&threadshared_global.cs);
	wcsncpy(threadshared_global.message,msg,512-1);
LeaveCriticalSection(&threadshared_global.cs);
(ignore)SendNotifyMessage(threadshared_global.hwnd,WM_USER,0,0);
return 0;
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
switch (uMsg) {
	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;
	case WM_CREATE:
		break;
	case WM_USER:
//	fprintf(stderr,"%s:%d got user\n",__FILE__,__LINE__);
		if (!IsIconic(hwnd)) InvalidateRect(hwnd,NULL,0);
		return 0;
	case WM_PAINT:
		{
			uint16_t msg[512];
			PAINTSTRUCT ps;
			HDC hdc;
			EnterCriticalSection(&threadshared_global.cs);
				wcscpy(msg,threadshared_global.message);
			LeaveCriticalSection(&threadshared_global.cs);
			hdc=BeginPaint(hwnd, &ps);
				FillRect(hdc, &ps.rcPaint, (HBRUSH) (COLOR_WINDOW+1));
				DrawText(hdc,msg,-1,&ps.rcPaint,DT_CENTER|DT_SINGLELINE|DT_VCENTER);
			EndPaint(hwnd, &ps);
		}
		return 0;
}
return DefWindowProc(hwnd, uMsg, wParam, lParam);
}


int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
WNDCLASS wc={};
HANDLE threadhandle;

wc.lpfnWndProc=WindowProc;
wc.hInstance=hInstance;
wc.lpszClassName=classname_win32wrapper;

if (0==RegisterClass(&wc)) goto error;

(void)InitializeCriticalSection(&threadshared_global.cs);

{
	HWND hwnd;
	hwnd = CreateWindowEx(0,classname_win32wrapper,
			windowtitle_win32wrapper,
			WS_OVERLAPPEDWINDOW&~WS_MAXIMIZEBOX, 
			CW_USEDEFAULT, CW_USEDEFAULT,
			300,100,
			NULL,NULL,hInstance,NULL);

	if (!hwnd) goto error;
	threadshared_global.hwnd=hwnd;
	(ignore)ShowWindow(hwnd, nCmdShow);
}
{
	SECURITY_ATTRIBUTES sa;
	DWORD	thid;
	sa.nLength=sizeof(SECURITY_ATTRIBUTES);
	sa.lpSecurityDescriptor=NULL;
	sa.bInheritHandle=0;
	threadhandle=CreateThread(&sa,64*1024,threadfunc,NULL,0,&thid);
	if (!threadhandle) goto error;
}
{
	MSG msg={};
	while (GetMessage(&msg,NULL,0,0)) {
		(ignore)TranslateMessage(&msg);
		(ignore)DispatchMessage(&msg);
	}
(ignore)TerminateThread(threadhandle,0);
}
fprintf(stderr,"%s:%d exiting\n",__FILE__,__LINE__);
return 0;
error:
	return -1;
}


