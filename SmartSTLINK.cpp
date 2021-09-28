#include <windows.h>
#include <devioctl.h>
#include <ntdddisk.h>
#include <ntddscsi.h>
#include <stdio.h>
#include <Commctrl.h>

//#include <stddef.h>
//#include <stdlib.h>
//#include <strsafe.h>
//#include <intsafe.h>
#define _NTSCSI_USER_MODE_
#include <scsi.h>
#include "..\Inc\spti.h"
#include "..\Inc\resource.h"

/*-------- LIBRARY -----------------*/
#pragma comment (lib,"Kernel32.lib")
#pragma comment (lib,"Shell32.lib")
#pragma comment (lib,"User32.lib")
#pragma comment (lib,"ComDlg32.lib")
#pragma comment (lib,"Gdi32.lib")

/*------- GLOBAL VARIABLES ----------*/
SCSI_PASS_THROUGH_WITH_BUFFERS sptwb;
DWORD error;
HANDLE stlinkHandle;
HANDLE fileProg;
HANDLE hThread;
HWND hCopyDlg;
HWND hWndComp;
WORD idFocus;
bool threadEnabled=false;
HINSTANCE hInst;
HDC hDc;
HBRUSH hBrush;
HWND hWnd;
HWND hwndScroll;
COORD coord;
WNDPROC wndOldProc;
OPENFILENAMEA ofn;       	// common dialog box structure

char buffer[MAX_PATH];
char bufNum[5];
char disk[3];
bool diskOpen=false;
int index;
int read;
long size;
unsigned char pVariable;
unsigned char pName;

#define MAX_VARIABLES 20
/*-------- GLOBAL STRUCT VARIABLES ------*/
struct _data
{
    UINT32 Value[200];
    UINT32 Addr[200];
    char length[200];
    char kind[200];
    char name[200][30];
    int write;
    int varFound;
    int index;
}Variables;

/*--------- DECLARATIONS -----------*/
INT_PTR CALLBACK Dialog1(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK SendTextProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
DWORD WINAPI mainThread(LPVOID lpParameter);
bool sendCommand(PSCSI_PASS_THROUGH_WITH_BUFFERS pSptwb, HANDLE* file, char length);
void waitBusy(PSCSI_PASS_THROUGH_WITH_BUFFERS pSptwb, HANDLE* file);
bool readData(PSCSI_PASS_THROUGH_WITH_BUFFERS pSptwb, HANDLE* file, _data * bufData, int start, char numVar);
bool writeData(PSCSI_PASS_THROUGH_WITH_BUFFERS pSptwb, HANDLE* file, _data * bufData, char index);
bool initCommunication(void);
void exitSWIM(void);

int APIENTRY WinMain(_In_ HINSTANCE hInstance,
                     _In_opt_ HINSTANCE hPrevInstance,
                     _In_ LPTSTR    lpCmdLine,
                     _In_ int       nCmdShow)
{
	UNREFERENCED_PARAMETER(hPrevInstance);
	UNREFERENCED_PARAMETER(lpCmdLine);

	hInst=hInstance;
 	// TODO: inserire qui il codice.
	DialogBox(hInst, MAKEINTRESOURCE(IDD_DIALOG1), NULL, Dialog1);
    if(stlinkHandle!=INVALID_HANDLE_VALUE) {CloseHandle(stlinkHandle);}
    if(diskOpen) {exitSWIM();}
	return 0;
}

INT_PTR CALLBACK Dialog1(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	UNREFERENCED_PARAMETER(lParam);

	switch (message)
	{
        case WM_INITDIALOG:
            SendDlgItemMessage(hDlg, IDC_DISK, (UINT)CB_ADDSTRING, (WPARAM)0, (LPARAM)"D:");
            SendDlgItemMessage(hDlg, IDC_DISK, (UINT)CB_ADDSTRING, (WPARAM)0, (LPARAM)"E:");
            SendDlgItemMessage(hDlg, IDC_DISK, (UINT)CB_ADDSTRING, (WPARAM)0, (LPARAM)"F:");
            SendDlgItemMessage(hDlg, IDC_DISK, (UINT)CB_ADDSTRING, (WPARAM)0, (LPARAM)"G:");
            SendDlgItemMessage(hDlg, IDC_DISK, (UINT)CB_ADDSTRING, (WPARAM)0, (LPARAM)"H:");
            SendDlgItemMessage(hDlg, IDC_DISK, (UINT)CB_ADDSTRING, (WPARAM)0, (LPARAM)"I:");
            SendDlgItemMessage(hDlg, IDC_DISK, (UINT)CB_ADDSTRING, (WPARAM)0, (LPARAM)"J:");
            SendDlgItemMessage(hDlg, IDC_DISK, (UINT)CB_ADDSTRING, (WPARAM)0, (LPARAM)"K:");
            SendDlgItemMessage(hDlg, IDC_DISK, (UINT)CB_ADDSTRING, (WPARAM)0, (LPARAM)"L:");
            SendDlgItemMessage(hDlg, IDC_DISK, (UINT)CB_ADDSTRING, (WPARAM)0, (LPARAM)"M:");
            SendDlgItemMessage(hDlg, IDC_DISK, (UINT)CB_ADDSTRING, (WPARAM)0, (LPARAM)"N:");
            SendDlgItemMessage(hDlg, IDC_DISK, (UINT)CB_ADDSTRING, (WPARAM)0, (LPARAM)"O:");
            SendDlgItemMessage(hDlg, IDC_DISK, CB_SETCURSEL, (WPARAM)2, (LPARAM)0);

            hCopyDlg=hDlg;
            // Fill comboboxes options
            // COM
            for(index=0;index<MAX_VARIABLES;index++)
            {
                hWndComp = GetDlgItem(hDlg, IDE_VALUE_00+index);
                wndOldProc = (WNDPROC)GetWindowLongPtr(hWndComp, -4);//GWL_WNDPROC);
                SetWindowLong(hWndComp, -4, (LONG)SendTextProc);
            }
            Variables.varFound=0;
            Variables.index=0;
            hwndScroll=GetDlgItem(hDlg,IDB_VSLIDE);     
            ShowWindow(hDlg, true);
            UpdateWindow(hDlg);
            return (INT_PTR)TRUE;

		/* Change colours of static controls*/
/*        case WM_CTLCOLORSTATIC:
            if (::GetDlgCtrlID((HWND)lParam)==IDS_STATUS)
            {
                //hBrush = CreateSolidBrush(RGB(0,0,0));
                hDc = (HDC)wParam;
                SetTextColor(hDc, RGB(255, 0, 255));
                SetBkColor(hDc, RGB(255, 255, 255));
                return (INT_PTR)TRUE;
            }
*/
		case WM_CTLCOLOREDIT:
            if ((::GetDlgCtrlID((HWND)lParam)>=IDE_VALUE_00) & (::GetDlgCtrlID((HWND)lParam)<(IDE_VALUE_00+MAX_VARIABLES)))
            {
                //hBrush = CreateSolidBrush(RGB(0,0,0));
                hDc = (HDC)wParam;
                SetTextColor(hDc, RGB(0, 255, 0));
                SetBkColor(hDc, RGB(0, 0, 0));
                return (INT_PTR)TRUE; //(BOOL)hBrush;
            }
            if ((::GetDlgCtrlID((HWND)lParam)>=IDE_NAME_00) & (::GetDlgCtrlID((HWND)lParam)<(IDE_NAME_00+MAX_VARIABLES)))
            {
                //hBrush = CreateSolidBrush(RGB(0,0,0));
                hDc = (HDC)wParam;
                SetTextColor(hDc, RGB(255, 255, 0));
                SetBkColor(hDc, RGB(0, 0, 0));
                return (INT_PTR)TRUE; //(BOOL)hBrush;
            }
            if ((::GetDlgCtrlID((HWND)lParam)>=IDE_LENGTH_00) & (::GetDlgCtrlID((HWND)lParam)<(IDE_LENGTH_00+MAX_VARIABLES)))
            {
                //hBrush = CreateSolidBrush(RGB(0,0,0));
                hDc = (HDC)wParam;
                SetTextColor(hDc, RGB(0, 255, 255));
                SetBkColor(hDc, RGB(0, 0, 0));
                return (INT_PTR)TRUE; //(BOOL)hBrush;
            }
	    	return (INT_PTR)FALSE;

			// The End
		case WM_CLOSE:
			KillTimer(hDlg,33);
			EndDialog(hDlg, 0);
    		return (INT_PTR)TRUE;

		case WM_COMMAND:
       		if (LOWORD(wParam) == IDC_REFRESH)
            {
                if(SendDlgItemMessageA(hDlg, IDC_REFRESH, BM_GETCHECK, 0,0)==1)
                {
                    pVariable=0;
                    index=0;
                    read=0;
                    while(pVariable<MAX_VARIABLES)
                    {
                        GetDlgItemTextA(hDlg,IDE_LENGTH_00+pVariable,&buffer[0], 20);
                        if(buffer[0]!='T') 
                        {
                            Variables.length[pVariable+Variables.index]=buffer[1]-0x30; 
                            Variables.kind[pVariable+pVariable+Variables.index]=buffer[0];
                        }
                        else {Variables.length[pVariable+pVariable+Variables.index]=0;}
                        //GetDlgItemTextA(hDlg,IDE_NAME_00+pVariable,&buffer[0], 30);
                        pVariable++;
                    }
                    threadEnabled=true;
                    hThread = CreateThread(NULL,0, (LPTHREAD_START_ROUTINE) &mainThread, nullptr, 0, NULL);
                }
                else {threadEnabled=false;}
            }       		
            if (LOWORD(wParam) == IDB_OPEN)
    		{
                if(!diskOpen)
                {
                    GetDlgItemTextA(hDlg,IDC_DISK, disk,3);
                    strcpy_s(buffer,"\\\\.\\");
                    strcat_s(buffer,disk);
                    stlinkHandle = CreateFileA((LPCSTR)&buffer,
                                            GENERIC_READ | GENERIC_WRITE, 
                                            FILE_SHARE_READ | FILE_SHARE_WRITE,
                                            NULL,
                                            OPEN_EXISTING,
                                            0,
                                            NULL);
                    if(stlinkHandle==INVALID_HANDLE_VALUE) {MessageBoxA(hDlg, "STLINK NOT FOUND!", "ERROR", MB_ICONERROR | MB_OK);}
                    else 
                    {
                        SetDlgItemTextA(hDlg, IDS_STATUS,(LPCSTR)"Connecting...");                              
                        if(!initCommunication()) 
                        {
                            MessageBoxA(hDlg, "ERROR SCSI COMMUNICATION!", "ERROR", MB_ICONERROR | MB_OK);
                            EnableWindow(hWnd, false);
                            diskOpen=false;
                            CloseHandle(stlinkHandle);
                            stlinkHandle=INVALID_HANDLE_VALUE;  
                            SetDlgItemTextA(hDlg, IDS_STATUS,(LPCSTR)"Error connection!");                              
                        }
                        else
                        {
                            hWnd= GetDlgItem(hDlg, IDC_REFRESH);
                            EnableWindow(hWnd, true);
                            hWnd= GetDlgItem(hDlg, IDB_READ_SINGLE);
                            EnableWindow(hWnd, true);
                            diskOpen=true;
                            SetDlgItemTextA(hDlg, IDB_OPEN,"CLOSE STLINK");
                            SetDlgItemTextA(hDlg, IDS_STATUS,(LPCSTR)"Connected!");                              

                        }
                    }
                }
                else
                {
                    SendDlgItemMessageA(hDlg, IDC_REFRESH, BM_SETCHECK, FALSE,0);
                    threadEnabled=false;
                    Sleep(1000);
                    exitSWIM();
                    hWnd= GetDlgItem(hDlg, IDC_REFRESH);
                    EnableWindow(hWnd, false);
                    hWnd= GetDlgItem(hDlg, IDB_READ_SINGLE);
                    EnableWindow(hWnd, false);
                    diskOpen=false;
                    CloseHandle(stlinkHandle);
                    stlinkHandle=INVALID_HANDLE_VALUE;    
                    SetDlgItemTextA(hDlg, IDB_OPEN,"OPEN STLINK");
                    SetDlgItemTextA(hDlg, IDS_STATUS,(LPCSTR)"Disconnected!");                              
                }
            }

       		if (LOWORD(wParam) == IDB_BROWSE)
            {
                // Initialize OPENFILENAME
                ZeroMemory(&ofn, sizeof(ofn));
                ofn.lStructSize = sizeof(ofn);
                ofn.hwndOwner = hCopyDlg;
                ofn.lpstrFile = (LPSTR)buffer;
                ofn.nMaxFile = sizeof(buffer);
                // Set lpstrFile[0] to '\0' so that GetOpenFileName does not 
                // use the contents of szFile to initialize itself.
                ofn.lpstrFile[0] = '\0';
                ofn.lpstrFilter = "*.RST\0*.RST";
                ofn.nFilterIndex = 1;
                ofn.lpstrFileTitle = NULL;
                ofn.nMaxFileTitle = 0;
                ofn.lpstrInitialDir = NULL;
                ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

                // Display the Open dialog box. 
                if(GetOpenFileNameA(&ofn) == TRUE)
                {
                    SetDlgItemTextA(hCopyDlg, IDE_FILE_PATH, buffer);
                }
            }
       		if (LOWORD(wParam) == IDB_LOAD)
    		{
                GetDlgItemTextA(hDlg,IDE_FILE_PATH, &buffer[0], MAX_PATH);
                fileProg=CreateFileA(buffer,
                                    GENERIC_READ | GENERIC_WRITE, 
                                    0,
                                    NULL,
                                    OPEN_EXISTING,
                                    0,
                                    NULL);
                if(fileProg==INVALID_HANDLE_VALUE) {MessageBoxA(hDlg, "FILE NOT FOUND!", "ERROR", MB_ICONERROR | MB_OK);}
                else
                {
                    index=0;
                    Variables.varFound=0;
                    Variables.index=0;
                    read;
                    size=0;
                    GetFileSizeEx(fileProg,(PLARGE_INTEGER)&size);
                    pVariable=0;
                    pName=0;

                    while(index<size)
                    {
                        ReadFile(fileProg,&buffer[0],1,(LPDWORD)&read,NULL);
                        index++;
                        if(buffer[0]=='a')
                        {                      
                            ReadFile(fileProg,&buffer[1],8,(LPDWORD)&read,NULL);
                            index+=8;
                            if((buffer[0]=='a') & (buffer[1]=='r') & (buffer[2]=='e') & (buffer[3]=='a') & (buffer[4]==' ') &
                            (buffer[5]=='D') & (buffer[6]=='A') & (buffer[7]=='T') & (buffer[8]=='A'))
                            {
                                while((index<size) & (pVariable<200))
                                {
                                    while((buffer[0]!='$') & (index<size))
                                    {
                                        ReadFile(fileProg,&buffer[0],1,(LPDWORD)&read,NULL);
                                        index++;
                                        if(buffer[0]=='a')
                                        {
                                            ReadFile(fileProg,&buffer[1],9,(LPDWORD)&read,NULL);
                                            if((buffer[0]=='a') & (buffer[1]=='r') & (buffer[2]=='e') & (buffer[3]=='a') & (buffer[4]==' ') &
                                            (buffer[5]=='I') & (buffer[6]=='N') & (buffer[7]=='I') & (buffer[8]=='T'))
                                            {
                                                CloseHandle(fileProg);                                  
                                                index=size;
                                                fileProg=INVALID_HANDLE_VALUE;
                                            }
                                            index+=9;
                                        }
                                    }
                                    if(index<size)
                                    {
                                        pName=0;
                                        do  // Read variable name
                                        {
                                            ReadFile(fileProg,&buffer[0],1,(LPDWORD)&read,NULL);
                                            if(buffer[0]!='$') {Variables.name[pVariable][pName++]=buffer[0];}
                                            index++;
                                        } while ((buffer[0]!='$') & (index<size));
                                        Variables.name[pVariable][pName++]='/';
                                        Variables.name[pVariable][pName++]='0';
                                        Variables.name[pVariable][pName++]='x';
                                        do
                                        {
                                            ReadFile(fileProg,&buffer[0],1,(LPDWORD)&read,NULL);
                                            index++;
                                        } while ((buffer[0]!=0x0A) & (index<size));
                                        do
                                        {
                                            ReadFile(fileProg,&buffer[0],1,(LPDWORD)&read,NULL);
                                            index++;
                                        } while ((buffer[0]!=0x0A) & (index<size));
                                        do
                                        {
                                            ReadFile(fileProg,&buffer[0],1,(LPDWORD)&read,NULL);
                                            index++;
                                        } while ((buffer[0]==0x20) & (index<size));
                                        Variables.Addr[pVariable]=0;
                                        do
                                        {
                                            if((buffer[0]>='0') & (buffer[0]<='9'))
                                            {
                                                Variables.Addr[pVariable]<<=4;
                                                Variables.Addr[pVariable]+=(buffer[0]-0x30);
                                            }
                                            if((buffer[0]>='a') & (buffer[0]<='f'))
                                            {
                                                Variables.Addr[pVariable]<<=4;
                                                Variables.Addr[pVariable]+=(buffer[0]-87);
                                            }
                                            if((buffer[0]>='A') & (buffer[0]<='F'))
                                            {
                                                Variables.Addr[pVariable]<<=4;
                                                Variables.Addr[pVariable]+=(buffer[0]-55);
                                            }
                                            Variables.name[pVariable][pName++]=buffer[0];
                                            ReadFile(fileProg,&buffer[0],1,(LPDWORD)&read,NULL);
                                            index++;
                                        } while ((buffer[0]!=0x20) & (index<size));
                                        SetDlgItemTextA(hDlg,IDE_NAME_00 + pVariable, Variables.name[pVariable]);
                                        while((buffer[0]!='.') & (index<size))
                                        {
                                            ReadFile(fileProg,&buffer[0],1,(LPDWORD)&read,NULL);
                                            index++;
                                        };
                                        ReadFile(fileProg,&buffer[0],2,(LPDWORD)&read,NULL);
                                        ReadFile(fileProg,&buffer[0],2,(LPDWORD)&read,NULL);
                                        buffer[0]='S'; buffer[2]=0;
                                        SetDlgItemTextA(hDlg,IDE_LENGTH_00 + pVariable, buffer);
                                        Variables.kind[pVariable]='S';
                                        Variables.length[pVariable]=buffer[1]-0x30;
                                        pVariable++;
                                        Variables.varFound++;
                                    }
                                }
                            }
                        }
                    }
                }
                index=Variables.varFound/MAX_VARIABLES;
                SendDlgItemMessageA(hDlg, IDB_VSLIDE, TBM_SETRANGE, TRUE, MAKELONG(0,index));
                SendDlgItemMessageA(hDlg, IDB_VSLIDE, TBM_SETPAGESIZE, 0, 1);

                GetDlgItemTextA(hCopyDlg, IDS_INDEX, buffer, sizeof(buffer));
                index=0;
                _itoa_s(Variables.index, bufNum, 10);
                while(buffer[index++]!=':');
                buffer[index++]=' ';
                buffer[index++]=0;
                strcat_s(buffer,bufNum);
                while(buffer[index++]!=0);
                buffer[--index]='/'; index++;
                buffer[index]=0;
                _itoa_s(Variables.varFound-1, bufNum, 10);
                strcat_s(buffer,bufNum);
                SetDlgItemTextA(hCopyDlg, IDS_INDEX, buffer);
            }
            if(HIWORD(wParam)==EN_SETFOCUS)
            {
                idFocus=LOWORD(wParam);
                return (INT_PTR)FALSE;
            }
            if(LOWORD(wParam)==IDB_READ_SINGLE)
            {
                SetDlgItemTextA(hCopyDlg, IDS_STATUS,(LPCSTR)"Reading....");
                GetDlgItemTextA(hDlg,IDE_LENGTH_00,&buffer[0], 20);
                if(buffer[0]!='T') {Variables.length[0]=buffer[1]-0x30; Variables.kind[0]=buffer[0];}
                else {Variables.length[0]=0;}
                GetDlgItemTextA(hDlg,IDE_NAME_00,&buffer[0], 30);
                index=0;
                read=0;
                while(buffer[index++]!='0')
                {
                    if(buffer[index+1]=='x') 
                    {
                        index+=2;
                        while((buffer[index]!=0) & (index<30))
                        {
                            if((buffer[index]>='0') & (buffer[index]<='9'))
                            {
                                read<<=4;
                                read+=(buffer[index]-0x30);
                            }
                            if((buffer[index]>='a') & (buffer[index]<='f'))
                            {
                                read<<=4;
                                read+=(buffer[index]-87);
                            }
                            if((buffer[index]>='A') & (buffer[index]<='F'))
                            {
                                read<<=4;
                                read+=(buffer[index]-55);
                            }
                            index++;
                        }
                        Variables.Addr[0]=read;
                    }
                }

                if(!readData(&sptwb, &stlinkHandle, &Variables, 0, 1))
                {
                    MessageBoxA(hCopyDlg, "ERROR READING DATA!", "ERROR", MB_ICONERROR | MB_OK);
                }
                else
                {
                    if(Variables.kind[0]=='S') 
                    {_itoa_s(Variables.Value[0], buffer, 30, 10);}
                    else if(Variables.kind[0]=='U') {_ultoa(Variables.Value[0], buffer, 10);}
                    else {buffer[0]='-'; buffer[1]=0;}    
                    SetDlgItemTextA(hCopyDlg, IDE_VALUE_00,&buffer[0]);
                }
                SetDlgItemTextA(hCopyDlg, IDS_STATUS,(LPCSTR)"Read!"); 
                return (INT_PTR)FALSE;
            }
        return (INT_PTR)FALSE;

		case WM_TIMER:
			Sleep(20);
	    	return (INT_PTR)FALSE;
        case WM_VSCROLL:
            // Get all the vertial scroll bar information.
            //si.cbSize = sizeof (si);
            //si.fMask  = SIF_ALL;
            //GetScrollInfo (hwndScroll, SB_VERT, &si);

            switch (LOWORD (wParam))
            {
                // User clicked the HOME keyboard key.
                case SB_TOP:
                    Variables.index = 0;
                    SendDlgItemMessageA(hDlg, IDB_VSLIDE, TBM_SETRANGE, FALSE,MAKELONG(0,Variables.varFound/MAX_VARIABLES));
                    SendDlgItemMessageA(hDlg, IDB_VSLIDE, TBM_SETPOS, TRUE,0);
                    break;
                    
                // User clicked the END keyboard key.
                case SB_BOTTOM:
                    Variables.index = Variables.varFound-1;
                    SendDlgItemMessageA(hDlg, IDB_VSLIDE, TBM_SETRANGE, FALSE,MAKELONG(0,Variables.varFound/MAX_VARIABLES));
                    SendDlgItemMessageA(hDlg, IDB_VSLIDE, TBM_SETPOS, TRUE, Variables.index);
                    break;
                    
                // User clicked the top arrow.
                case SB_LINEUP:
                    Variables.index -= 1;
                    if(Variables.index<0) {Variables.index=0;}
                    SendDlgItemMessageA(hDlg, IDB_VSLIDE, TBM_SETRANGE, FALSE,MAKELONG(0,Variables.varFound));
                    SendDlgItemMessageA(hDlg, IDB_VSLIDE, TBM_SETPOS, TRUE, Variables.index);
                    break;

                // User clicked the top arrow.
                case SB_PAGEUP:
                    Variables.index -= MAX_VARIABLES;
                    if(Variables.index<0) {Variables.index=0;}
                    SendDlgItemMessageA(hDlg, IDB_VSLIDE, TBM_SETRANGE, FALSE,MAKELONG(0,Variables.varFound/MAX_VARIABLES));
                    SendDlgItemMessageA(hDlg, IDB_VSLIDE, TBM_SETPOS, TRUE, (Variables.index/MAX_VARIABLES));
                    break;

                // User clicked the bottom arrow.
                case SB_LINEDOWN:
                    Variables.index += 1;
                    if(Variables.index>=Variables.varFound) {Variables.index=Variables.varFound-1;}
                    SendDlgItemMessageA(hDlg, IDB_VSLIDE, TBM_SETRANGE, FALSE,MAKELONG(0,Variables.varFound));
                    SendDlgItemMessageA(hDlg, IDB_VSLIDE, TBM_SETPOS, TRUE, Variables.index);
                    break;                    
                // User clicked page down.
                case SB_PAGEDOWN:
                    Variables.index += MAX_VARIABLES;
                    if(Variables.index>=Variables.varFound) {Variables.index=Variables.varFound-1;}
                    SendDlgItemMessageA(hDlg, IDB_VSLIDE, TBM_SETRANGE, FALSE,MAKELONG(0,Variables.varFound/MAX_VARIABLES));
                    SendDlgItemMessageA(hDlg, IDB_VSLIDE, TBM_SETPOS, TRUE, (Variables.index/MAX_VARIABLES));
                    break;                    
                // User dragged the scroll box.
                case SB_THUMBTRACK:
                    SendDlgItemMessageA(hDlg, IDB_VSLIDE, TBM_SETRANGE, FALSE,MAKELONG(0,Variables.varFound));
                    Variables.index=SendDlgItemMessage(hDlg,IDB_VSLIDE, TBM_GETPOS, 0,0);
                    break;
                default:
                    break;
            }
            // Set the position and then retrieve it.  Due to adjustments
            // by Windows it may not be the same as the value set.

            GetDlgItemTextA(hCopyDlg, IDS_INDEX, buffer, sizeof(buffer));
            index=0;
            _itoa_s(Variables.index, bufNum, 10);
            while(buffer[index++]!=':');
            buffer[index++]=' ';
            buffer[index++]=0;
            strcat_s(buffer,bufNum);
            while(buffer[index++]!=0);
            buffer[--index]='/'; index++;
            buffer[index]=0;
            _itoa_s(Variables.varFound-1, bufNum, 10);
            strcat_s(buffer,bufNum);
            SetDlgItemTextA(hCopyDlg, IDS_INDEX, buffer);

            if(threadEnabled==false)
            {
                index= Variables.index;
                pVariable=0;
                while(pVariable<MAX_VARIABLES)
                {
                    if(index<Variables.varFound)
                    {
                        SetDlgItemTextA(hDlg,IDE_NAME_00 + pVariable, Variables.name[index]);
                        SetDlgItemInt(hDlg,IDE_VALUE_00 + pVariable,Variables.Value[index], true);
                        bufNum[0]=Variables.kind[index];
                        bufNum[1]=0x30+Variables.length[index];
                        bufNum[2]=0;
                        SetDlgItemTextA(hDlg,IDE_LENGTH_00 + pVariable, bufNum);
                    }
                    else
                    {
                        SetDlgItemTextA(hDlg,IDE_NAME_00 + pVariable, "Data Name/Address");
                        SetDlgItemTextA(hDlg,IDE_LENGTH_00 + pVariable, "T&Size");
                        SetDlgItemTextA(hDlg,IDE_VALUE_00 + pVariable, "-");
                    }
                    pVariable++;
                    index++;
                }
            }
	    return (INT_PTR)FALSE;
	}
	return (INT_PTR)FALSE;
}

LRESULT CALLBACK SendTextProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    int x;

	switch (message)
	{
	case WM_KEYUP:
		//Process this message to avoid message beeps.
		if (wParam == VK_RETURN)
		{
            if(diskOpen)
            {
                SetDlgItemTextA(hCopyDlg, IDS_STATUS,(LPCSTR)"Writing...."); 
                Variables.write=GetDlgCtrlID(hwnd);
                if(SendDlgItemMessageA(hCopyDlg, IDC_REFRESH, BM_GETCHECK, 0,0)!=1)
                {
                    x=Variables.write-IDE_VALUE_00+Variables.index;
                    Variables.Value[x]=GetDlgItemInt(hCopyDlg, Variables.write, NULL, TRUE);
                    if(!writeData(&sptwb, &stlinkHandle, &Variables, x))
                    {
                        SetDlgItemTextA(hCopyDlg, IDS_STATUS,(LPCSTR)"Error writing data");
                    }
                    else
                    {
                        SetDlgItemTextA(hCopyDlg, IDS_STATUS,(LPCSTR)"Data written!"); 
                    }
                    Variables.write=0;
                }
            }
            //SendMessage(GetParent(hwnd), WM_COMMAND, (WPARAM)IDOK, (LPARAM)locId);
			return 0;
		}
		else
			return (CallWindowProc((WNDPROC) wndOldProc, hwnd, message, wParam, lParam));
		break;

	default:        
		return CallWindowProc((WNDPROC) wndOldProc, hwnd, message, wParam, lParam);
	}
	return 0;
}

void waitBusy(PSCSI_PASS_THROUGH_WITH_BUFFERS pSptwb, HANDLE* file)
{
    pSptwb->ucDataBuf[0]=1;
    while(pSptwb->ucDataBuf[0]!=0)
    {
        pSptwb->spt.Cdb[0] = 0xF4;    // SWIM GET BUSY
        pSptwb->spt.Cdb[1] = 0x09;
        pSptwb->spt.Cdb[2] = 0x01;
        pSptwb->spt.Cdb[3] = 0x00;
        pSptwb->spt.Cdb[4] = 0x00;
        pSptwb->spt.Cdb[5] = 0x00;
        pSptwb->spt.Cdb[6] = 0x00;
        pSptwb->spt.Cdb[7] = 0x00;
        pSptwb->spt.Cdb[8] = 0x00;
        pSptwb->spt.Cdb[9] = 0x00;
        sendCommand(pSptwb,file,10);
        Sleep(20);
    }    
}

bool readData(PSCSI_PASS_THROUGH_WITH_BUFFERS pSptwb, HANDLE* file, _data * bufData, int start, char numVar)
{
    int length=0;
    int x=start;
    int tValue;
    int counter;

    while(x<numVar)
    {
        length+=bufData->length[x++];
    }
    
    pSptwb->spt.Cdb[0] = 0xF4;    // SWIM BEGIN READ
    pSptwb->spt.Cdb[1] = 0x0B;
    pSptwb->spt.Cdb[2] = 0x00;
    pSptwb->spt.Cdb[3] = length;
    pSptwb->spt.Cdb[4] = (bufData->Addr[start]>>24)&0xFF;
    pSptwb->spt.Cdb[5] = (bufData->Addr[start]>>16)&0xFF;
    pSptwb->spt.Cdb[6] = (bufData->Addr[start]>>8)&0xFF;
    pSptwb->spt.Cdb[7] = bufData->Addr[start] & 0xFF;
    pSptwb->spt.Cdb[8] = 0x30;
    pSptwb->spt.Cdb[9] = 0x00;
    sendCommand(pSptwb,file,10);

    //waitBusy(pSptwb,file);

    pSptwb->spt.Cdb[0] = 0xF4;    // SWIM READ
    pSptwb->spt.Cdb[1] = 0x0C;
    pSptwb->spt.Cdb[2] = 0x00;
    pSptwb->spt.Cdb[3] = length;
    pSptwb->spt.Cdb[4] = (bufData->Addr[start]>>24)&0xFF;
    pSptwb->spt.Cdb[5] = (bufData->Addr[start]>>16)&0xFF;
    pSptwb->spt.Cdb[6] = (bufData->Addr[start]>>8)&0xFF;
    pSptwb->spt.Cdb[7] = bufData->Addr[start] & 0xFF;
    pSptwb->spt.Cdb[8] = 0x30;
    pSptwb->spt.Cdb[9] = 0x00;
    if(!sendCommand(pSptwb,file,10))
    {
        return false;
        //goto exit;
    }
    
    x=start;
    length=0;
    tValue=0;
    counter=0;

    while(x<numVar)
    {
        tValue<<=8;
        tValue+=pSptwb->ucDataBuf[counter++];
        length++;
        if(length>=bufData->length[x]) {bufData->Value[x]=tValue; x++; length=0; tValue=0;}
    }
    return true;
}

bool writeData(PSCSI_PASS_THROUGH_WITH_BUFFERS pSptwb, HANDLE* file, _data * bufData, char index)
{
    
    pSptwb->spt.Cdb[0] = 0xF4;    // SWIM WRITE
    pSptwb->spt.Cdb[1] = 0x0A;
    pSptwb->spt.Cdb[2] = 0x00;
    pSptwb->spt.Cdb[3] = bufData->length[index];
    pSptwb->spt.Cdb[4] = (bufData->Addr[index]>>24)&0xFF;
    pSptwb->spt.Cdb[5] = (bufData->Addr[index]>>16)&0xFF;
    pSptwb->spt.Cdb[6] = (bufData->Addr[index]>>8)&0xFF;
    pSptwb->spt.Cdb[7] = bufData->Addr[index] & 0xFF;
    if(bufData->length[index]==1) 
    {
        pSptwb->spt.Cdb[8] = bufData->Value[index] & 0xFF;
        pSptwb->spt.Cdb[9] = 0;
        pSptwb->spt.Cdb[10] = 0;
        pSptwb->spt.Cdb[11] = 0;
    }
    else if(bufData->length[index]==2) 
    {
        pSptwb->spt.Cdb[8] = (bufData->Value[index]>>8)&0xFF;
        pSptwb->spt.Cdb[9] = bufData->Value[index] & 0xFF;;
        pSptwb->spt.Cdb[10] = 0;
        pSptwb->spt.Cdb[11] = 0;
    }
    else if(bufData->length[index]==4) 
    {
        pSptwb->spt.Cdb[8] = (bufData->Value[index]>>24)&0xFF;
        pSptwb->spt.Cdb[9] = (bufData->Value[index]>>16)&0xFF;
        pSptwb->spt.Cdb[10] = (bufData->Value[index]>>8)&0xFF;
        pSptwb->spt.Cdb[11] = bufData->Value[index] & 0xFF;    
    }
    pSptwb->spt.Cdb[12] = 0x00;
    pSptwb->spt.Cdb[13] = 0x00;
    pSptwb->spt.Cdb[14] = 0x00;
    pSptwb->spt.Cdb[15] = 0x00;
 
    sendCommand(pSptwb,file,16);
    waitBusy(pSptwb,file);
    return true;
}

bool sendCommand(PSCSI_PASS_THROUGH_WITH_BUFFERS pSptwb, HANDLE* file, char cdbLlength)
{
    unsigned long length;   
    DWORD returned;
    bool bResult;

    pSptwb->spt.Length = sizeof(SCSI_PASS_THROUGH);
    pSptwb->spt.PathId = 0;
    pSptwb->spt.TargetId = 2;
    pSptwb->spt.Lun = 2;
    pSptwb->spt.CdbLength = cdbLlength;
    pSptwb->spt.SenseInfoLength = SPT_SENSE_LENGTH;
    pSptwb->spt.DataIn = SCSI_IOCTL_DATA_IN;
    pSptwb->spt.DataTransferLength = 192;
    pSptwb->spt.TimeOutValue = 1; //2;
    pSptwb->spt.DataBufferOffset = offsetof(SCSI_PASS_THROUGH_WITH_BUFFERS,ucDataBuf);
    pSptwb->spt.SenseInfoOffset = offsetof(SCSI_PASS_THROUGH_WITH_BUFFERS,ucSenseBuf);
    length = offsetof(SCSI_PASS_THROUGH_WITH_BUFFERS,ucDataBuf) + pSptwb->spt.DataTransferLength;

    bResult = DeviceIoControl(*file,
                            IOCTL_SCSI_PASS_THROUGH, // IOCTL_SCSI_PASS_THROUGH,
                            pSptwb,
                            sizeof(SCSI_PASS_THROUGH),
                            pSptwb,
                            length,
                            &returned,
                            FALSE);
    return bResult;
}

bool initCommunication(void)
{
        // TEST UNIT READY (command 0x00)
    ZeroMemory(&sptwb,sizeof(SCSI_PASS_THROUGH_WITH_BUFFERS));

    sptwb.spt.Cdb[0] = 0xF1;    // REQUEST VERSION
    sptwb.spt.Cdb[1] = 0x80;
    if(!sendCommand(&sptwb,&stlinkHandle,6)) {error=GetLastError(); if(error!=ERROR_SEM_TIMEOUT) {return false;}}

    sptwb.spt.Cdb[0] = 0xF5;    // GET CURRENT MODE
    sptwb.spt.Cdb[1] = 0x00;
    if(!sendCommand(&sptwb,&stlinkHandle,2)) {error=GetLastError(); if(error!=ERROR_SEM_TIMEOUT) {return false;}}

    sptwb.spt.Cdb[0] = 0xF3;    // EXIT FROM DFU
    sptwb.spt.Cdb[1] = 0x07;
    if(!sendCommand(&sptwb,&stlinkHandle,2)) {error=GetLastError(); if(error!=ERROR_SEM_TIMEOUT) {return false;}}

    sptwb.spt.Cdb[0] = 0xF4;    // ENTER SWIM MODE
    sptwb.spt.Cdb[1] = 0x00;
    if(!sendCommand(&sptwb,&stlinkHandle,2)) {error=GetLastError(); if(error!=ERROR_SEM_TIMEOUT) {return false;}}

    sptwb.spt.Cdb[0] = 0xF4;    // SWIM GET SIZE
    sptwb.spt.Cdb[1] = 0x0D;
    if(!sendCommand(&sptwb,&stlinkHandle,2)) {error=GetLastError(); if(error!=ERROR_SEM_TIMEOUT) {return false;}}

    sptwb.spt.Cdb[0] = 0xF4;    // SWIM GET 02
    sptwb.spt.Cdb[1] = 0x02;
    sptwb.spt.Cdb[2] = 0x01;
    if(!sendCommand(&sptwb,&stlinkHandle,3)) {error=GetLastError(); if(error!=ERROR_SEM_TIMEOUT) {return false;}}

    sptwb.spt.Cdb[0] = 0xF4;    // SWIM DO 04
    sptwb.spt.Cdb[1] = 0x04;
    sptwb.spt.Cdb[2] = 0x01;
    if(!sendCommand(&sptwb,&stlinkHandle,10)) {error=GetLastError(); if(error!=ERROR_SEM_TIMEOUT) {return false;}}

    waitBusy(&sptwb,&stlinkHandle);

    sptwb.spt.Cdb[0] = 0xF4;    // SWIM DO 03
    sptwb.spt.Cdb[1] = 0x03;
    sptwb.spt.Cdb[2] = 0x00;
    if(!sendCommand(&sptwb,&stlinkHandle,3)) {error=GetLastError(); if(error!=ERROR_SEM_TIMEOUT) {return false;}}

    waitBusy(&sptwb,&stlinkHandle);

    sptwb.spt.Cdb[0] = 0xF4;    // SWIM DO 0A (write in 0x7F80 value 0x30)
    sptwb.spt.Cdb[1] = 0x0A;
    sptwb.spt.Cdb[2] = 0x00;
    sptwb.spt.Cdb[3] = 0x01;
    sptwb.spt.Cdb[4] = 0x00;
    sptwb.spt.Cdb[5] = 0x00;
    sptwb.spt.Cdb[6] = 0x7F;
    sptwb.spt.Cdb[7] = 0x80;
    sptwb.spt.Cdb[8] = 0x30;
    sptwb.spt.Cdb[9] = 0x00;
    sptwb.spt.Cdb[10] = 0x00;
    sptwb.spt.Cdb[11] = 0x00;
    sptwb.spt.Cdb[12] = 0x00;
    sptwb.spt.Cdb[13] = 0x00;
    sptwb.spt.Cdb[14] = 0x00;
    sptwb.spt.Cdb[15] = 0x00;
    if(!sendCommand(&sptwb,&stlinkHandle,16)) {error=GetLastError(); if(error!=ERROR_SEM_TIMEOUT) {return false;}}

    waitBusy(&sptwb,&stlinkHandle);

    sptwb.spt.Cdb[0] = 0xF4;    // SWIM DO 03
    sptwb.spt.Cdb[1] = 0x03;
    sptwb.spt.Cdb[2] = 0x01;
    if(!sendCommand(&sptwb,&stlinkHandle,3)) {error=GetLastError(); if(error!=ERROR_SEM_TIMEOUT) {return false;}}
    waitBusy(&sptwb,&stlinkHandle);
    return true;
}

void exitSWIM(void)
{
    sptwb.spt.Cdb[0] = 0xF4;    // EXIT SWIM
    sptwb.spt.Cdb[1] = 0x01;
    sptwb.spt.Cdb[2] = 0x00;
    sptwb.spt.Cdb[3] = 0x04;
    sptwb.spt.Cdb[4] = 0x00;
    sptwb.spt.Cdb[5] = 0x00;
    sptwb.spt.Cdb[6] = 0x00;
    sptwb.spt.Cdb[7] = 0x01;
    sptwb.spt.Cdb[8] = 0x00;
    sptwb.spt.Cdb[9] = 0x00;
    sendCommand(&sptwb,&stlinkHandle,10);
}

DWORD WINAPI mainThread(LPVOID lpParameter)
{
    int x;

    while(threadEnabled)
    {
        if(Variables.write!=0)
        {
            x=Variables.write-IDE_VALUE_00+Variables.index;
            Variables.Value[x]=GetDlgItemInt(hCopyDlg, Variables.write, NULL, TRUE);
            if(!writeData(&sptwb, &stlinkHandle, &Variables, x))
            {
                MessageBoxA(hCopyDlg, "ERROR WRITING DATA!", "ERROR", MB_ICONERROR | MB_OK);
            }
            else
            {
                SetDlgItemTextA(hCopyDlg, IDS_STATUS,(LPCSTR)"Data written!"); 
            }
            Variables.write=0;
        }
        else
        {
            if(!readData(&sptwb, &stlinkHandle, &Variables, 0, Variables.varFound))
            {
                MessageBoxA(hCopyDlg, "ERROR READING DATA!", "ERROR", MB_ICONERROR | MB_OK);
            }
            else
            {
                for(x=0; x<MAX_VARIABLES; x++)
                {
                    if((Variables.index+x)<Variables.varFound)
                    {
                        if(idFocus!=(IDE_VALUE_00+x)) //   SendDlgItemMessageA(hCopyDlg,IDE_VALUE_00+x,WM_GETTEXT))
                        {
                            SetDlgItemTextA(hCopyDlg, IDE_NAME_00+x,Variables.name[x+Variables.index]);
                            if(Variables.kind[x+Variables.index]=='S') {_itoa_s(Variables.Value[x+Variables.index], buffer, 30, 10);}
                            else if(Variables.kind[x+Variables.index]=='U') {_ultoa(Variables.Value[x+Variables.index], buffer, 10);}
                            else {buffer[0]='-'; buffer[1]=0;}    
                            SetDlgItemTextA(hCopyDlg, IDE_VALUE_00+x,(LPCSTR)buffer);
                            buffer[0]=Variables.kind[x+Variables.index];
                            buffer[1]=Variables.length[x+Variables.index]+0x30;
                            buffer[2]=0;
                            SetDlgItemTextA(hCopyDlg, IDE_LENGTH_00+x, buffer);
                        }
                    }
                    else
                    {
                        SetDlgItemTextA(hCopyDlg, IDE_NAME_00+x,"Data Name/Address");
                        SetDlgItemTextA(hCopyDlg, IDE_LENGTH_00+x,"T&size");
                        SetDlgItemTextA(hCopyDlg, IDE_VALUE_00+x,"-");

                    }
                }
            }
        }
    }
    return 0;
}
