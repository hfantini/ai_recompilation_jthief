// Windows 3.1 API prototypes for the functions JWLTHIEF.EXE imports, in SDK
// order and naming. Read by tools/ghidra/scripts/Win16Api.java, which only
// understands one declaration per line:
//
//     [__cdecl] RETURN Name(TYPE name, ...);
//
// Every type must be a single identifier known to Win16Api. Functions are
// FAR PASCAL unless marked __cdecl.
//
// Deliberately absent: INITTASK and DOS3CALL pass values in registers, and
// __WINFLAGS is an absolute symbol, not a function.

// KERNEL
void FatalExit(int nCode);
DWORD GetVersion(void);
HLOCAL LocalAlloc(UINT fuAllocFlags, UINT fuAlloc);
HLOCAL LocalReAlloc(HLOCAL hloc, UINT fuNewSize, UINT fuFlags);
HLOCAL LocalFree(HLOCAL hloc);
UINT LocalSize(HLOCAL hloc);
HGLOBAL GlobalAlloc(UINT fuAlloc, DWORD cbAlloc);
HGLOBAL GlobalReAlloc(HGLOBAL hglb, DWORD cbNewSize, UINT fuAlloc);
HGLOBAL GlobalFree(HGLOBAL hglb);
LPVOID GlobalLock(HGLOBAL hglb);
BOOL GlobalUnlock(HGLOBAL hglb);
DWORD GlobalSize(HGLOBAL hglb);
HGLOBAL LockSegment(UINT uSegment);
void UnlockSegment(UINT uSegment);
BOOL WaitEvent(HTASK hTask);
int GetModuleFileName(HINSTANCE hinst, LPSTR lpszFilename, int cbFileName);
FARPROC MakeProcInstance(FARPROC lpProc, HINSTANCE hinst);
void FreeProcInstance(FARPROC lpProc);
HFILE OpenFile(LPCSTR lpszFileName, LPOFSTRUCT lpOpenBuff, UINT fuMode);
HFILE _lclose(HFILE hf);
UINT _lread(HFILE hf, LPVOID hpvBuffer, UINT cbBuffer);
LONG _llseek(HFILE hf, LONG lOffset, int nOrigin);
LPSTR lstrcpy(LPSTR lpszString1, LPCSTR lpszString2);
LPSTR lstrcat(LPSTR lpszString1, LPCSTR lpszString2);
int lstrlen(LPCSTR lpszString);
int GetPrivateProfileString(LPCSTR lpszSection, LPCSTR lpszEntry, LPCSTR lpszDefault, LPSTR lpszReturnBuffer, int cbReturnBuffer, LPCSTR lpszFilename);
BOOL WritePrivateProfileString(LPCSTR lpszSection, LPCSTR lpszEntry, LPCSTR lpszString, LPCSTR lpszFilename);
LPSTR GetDOSEnvironment(void);
void FatalAppExit(UINT fuAction, LPCSTR lpszMessageText);

// GDI
COLORREF SetBkColor(HDC hdc, COLORREF clrref);
BOOL LineTo(HDC hdc, int nXEnd, int nYEnd);
DWORD MoveTo(HDC hdc, int nXPos, int nYPos);
BOOL Arc(HDC hdc, int nLeftRect, int nTopRect, int nRightRect, int nBottomRect, int nXStartArc, int nYStartArc, int nXEndArc, int nYEndArc);
BOOL Ellipse(HDC hdc, int nLeftRect, int nTopRect, int nRightRect, int nBottomRect);
BOOL TextOut(HDC hdc, int nXStart, int nYStart, LPCSTR lpszString, int cbString);
BOOL BitBlt(HDC hdcDest, int nXDest, int nYDest, int nWidth, int nHeight, HDC hdcSrc, int nXSrc, int nYSrc, DWORD dwRop);
BOOL FillRgn(HDC hdc, HRGN hrgn, HBRUSH hbr);
HGDIOBJ SelectObject(HDC hdc, HGDIOBJ hgdiobj);
HDC CreateCompatibleDC(HDC hdc);
HPEN CreatePen(int fnPenStyle, int nWidth, COLORREF clrref);
HRGN CreatePolygonRgn(LPPOINT lppt, int cPoints, int fnPolyFillMode);
HBRUSH CreateSolidBrush(COLORREF clrref);
BOOL DeleteDC(HDC hdc);
BOOL DeleteObject(HGDIOBJ hgdiobj);
int GetObject(HGDIOBJ hgdiobj, int cbBuffer, LPVOID lpvObject);
HGDIOBJ GetStockObject(int fnObject);
COLORREF GetNearestColor(HDC hdc, COLORREF clrref);
int SetDIBitsToDevice(HDC hdc, int XDest, int YDest, int cx, int cy, int XSrc, int YSrc, UINT uStartScan, UINT cScanLines, LPVOID lpvBits, LPBITMAPINFO lpbmi, UINT fuColorUse);

// USER
int MessageBox(HWND hwndParent, LPCSTR lpszText, LPCSTR lpszTitle, UINT fuStyle);
BOOL InitApp(HINSTANCE hInstance);
void PostQuitMessage(int nExitCode);
UINT SetTimer(HWND hwnd, UINT idTimer, UINT uTimeout, TIMERPROC tmprc);
BOOL KillTimer(HWND hwnd, UINT idTimer);
void GetClientRect(HWND hwnd, LPRECT lprc);
HDC BeginPaint(HWND hwnd, LPPAINTSTRUCT lpps);
void EndPaint(HWND hwnd, LPPAINTSTRUCT lpps);
HWND CreateWindow(LPCSTR lpszClassName, LPCSTR lpszWindowName, DWORD dwStyle, int x, int y, int nWidth, int nHeight, HWND hwndParent, HMENU hmenu, HINSTANCE hinst, LPVOID lpvCreateParams);
BOOL ShowWindow(HWND hwnd, int nCmdShow);
BOOL DestroyWindow(HWND hwnd);
ATOM RegisterClass(LPWNDCLASS lpwc);
HDC GetDC(HWND hwnd);
int ReleaseDC(HWND hwnd, HDC hdc);
HCURSOR SetCursor(HCURSOR hcur);
int FillRect(HDC hdc, LPRECT lprc, HBRUSH hbr);
int DialogBox(HINSTANCE hinst, LPCSTR lpszDlgTemp, HWND hwndOwner, DLGPROC dlgprc);
void EndDialog(HWND hwndDlg, int nResult);
void SetDlgItemText(HWND hwndDlg, int idControl, LPCSTR lpsz);
LRESULT SendDlgItemMessage(HWND hwndDlg, int idDlgItem, UINT uMsg, WPARAM wParam, LPARAM lParam);
void MessageBeep(UINT uAlert);
LRESULT DefWindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
BOOL GetMessage(LPMSG lpmsg, HWND hwnd, UINT uMsgFilterMin, UINT uMsgFilterMax);
BOOL PostMessage(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
BOOL TranslateMessage(LPMSG lpmsg);
LONG DispatchMessage(LPMSG lpmsg);
void UpdateWindow(HWND hwnd);
void InvalidateRect(HWND hwnd, LPRECT lprc, BOOL fErase);
LONG GetWindowLong(HWND hwnd, int nOffset);
BOOL WinHelp(HWND hwnd, LPCSTR lpszHelpFile, UINT fuCommand, DWORD dwData);
HCURSOR LoadCursor(HINSTANCE hinst, LPCSTR pszCursor);
HICON LoadIcon(HINSTANCE hinst, LPCSTR pszicon);
HBITMAP LoadBitmap(HINSTANCE hinst, LPCSTR lpszBitmap);
HACCEL LoadAccelerators(HINSTANCE hinst, LPCSTR lpszTableName);
int TranslateAccelerator(HWND hwnd, HACCEL haccl, LPMSG lpmsg);
__cdecl int _wsprintf(LPSTR lpszOutput, LPCSTR lpszFormat, ...);
int lstrcmp(LPCSTR lpszString1, LPCSTR lpszString2);
