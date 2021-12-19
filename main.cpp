// RegisterDeviceNotification.cpp
#include <windows.h>
#include <stdio.h>
#include <tchar.h>
#include <strsafe.h>
#include <dbt.h>

// This GUID is for all USB serial host PnP drivers, but you can replace it 
// with any valid device class guid.
GUID WceusbshGUID = { 0x25dbce51, 0x6c8f, 0x4a72,
                      0x8a,0x6d,0xb5,0x4c,0x2b,0x4f,0xc8,0x35 };

// For informational messages and window titles.
PWSTR g_pszAppName;

void OutputMessage(
    HWND hOutWnd,
    WPARAM wParam,
    LPARAM lParam
)
//     lParam  - String message to send to the window.
{
    printf("Message: %s\n", lParam);
}

void ErrorHandler(
    LPCTSTR lpszFunction
)
{

    LPVOID lpMsgBuf;
    LPVOID lpDisplayBuf;
    DWORD dw = GetLastError();

    FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL,
        dw,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        (LPTSTR)&lpMsgBuf,
        0, NULL);

    printf("Error: %d - %s\n", dw, lpMsgBuf);
    LocalFree(lpMsgBuf);
}

BOOL DoRegisterDeviceInterfaceToHwnd(
    IN GUID InterfaceClassGuid,
    IN HWND hWnd,
    OUT HDEVNOTIFY* hDeviceNotify
)
{
    DEV_BROADCAST_DEVICEINTERFACE NotificationFilter;

    ZeroMemory(&NotificationFilter, sizeof(NotificationFilter));
    NotificationFilter.dbcc_size = sizeof(DEV_BROADCAST_DEVICEINTERFACE);
    NotificationFilter.dbcc_devicetype = DBT_DEVTYP_DEVICEINTERFACE;
    NotificationFilter.dbcc_classguid = InterfaceClassGuid;

    *hDeviceNotify = RegisterDeviceNotification(
        hWnd,                       // events recipient
        &NotificationFilter,        // type of device
        DEVICE_NOTIFY_WINDOW_HANDLE | DEVICE_NOTIFY_ALL_INTERFACE_CLASSES
    );

    if (NULL == *hDeviceNotify)
    {
        ErrorHandler("RegisterDeviceNotification");
        return FALSE;
    }

    return TRUE;
}

void MessagePump(
    HWND hWnd
)
{
    MSG msg;
    int retVal;

    while ((retVal = GetMessage(&msg, NULL, 0, 0)) != 0)
    {
        if (retVal == -1)
        {
            ErrorHandler("GetMessage");
            break;
        }
        else
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }
}

void startThing(int VID, int PID)
{

    char cmd[5123];
    snprintf(cmd, sizeof(cmd), "py attach.py %4x:%4x", VID, PID);
    printf("Starting '%s'\n", cmd);
    system(cmd);
}

INT_PTR WINAPI WinProcCallback(
    HWND hWnd,
    UINT message,
    WPARAM wParam,
    LPARAM lParam
)
{
    LRESULT lRet = 1;
    static HDEVNOTIFY hDeviceNotify;
    static HWND hEditWnd;
    static ULONGLONG msgCount = 0;

    switch (message)
    {
    case WM_CREATE:
        if (!DoRegisterDeviceInterfaceToHwnd(
            WceusbshGUID,
            hWnd,
            &hDeviceNotify))
        {
            // Terminate on failure.
            ErrorHandler("DoRegisterDeviceInterfaceToHwnd");
            ExitProcess(1);
        }

        break;

    case WM_DEVICECHANGE:
    {
        //
        // This is the actual message from the interface via Windows messaging.
        // This code includes some additional decoding for this particular device type
        // and some common validation checks.
        //
        // Note that not all devices utilize these optional parameters in the same
        // way. Refer to the extended information for your particular device type 
        // specified by your GUID.
        //
        PDEV_BROADCAST_DEVICEINTERFACE b = (PDEV_BROADCAST_DEVICEINTERFACE)lParam;
        TCHAR strBuff[256];

        // Output some messages to the window.
        switch (wParam)
        {
        case DBT_DEVICEARRIVAL:
            msgCount++;
            StringCchPrintf(
                strBuff, 256,
                TEXT("Message %d: DBT_DEVICEARRIVAL\n"), (int)msgCount);
            
            {
                static const char* types[] = {"OEM", "???", "VOLUME", "PORT", "???", "DEVICEINTERFACE", "HANDLE"};

                const auto* dev = reinterpret_cast<DEV_BROADCAST_HDR*>(lParam);
                printf("Type: %d - %s\n", dev->dbch_devicetype, types[dev->dbch_devicetype]);

                static int VID = -1;
                static int PID = -1;
                if(dev->dbch_devicetype == DBT_DEVTYP_DEVICEINTERFACE)
                {
                    const auto* device = reinterpret_cast<const DEV_BROADCAST_DEVICEINTERFACE*>(dev);
                    wchar_t guidStringBuffer[256];
                    StringFromGUID2(device->dbcc_classguid, guidStringBuffer, sizeof(guidStringBuffer)/sizeof(guidStringBuffer[0]));
                    printf("Device guid: %S\n", guidStringBuffer);
                    printf("Device name: %s\n", device->dbcc_name);

                    // \\?\USB#VID_1A86&PID_7523#5&521a615&0&9                    
                    const bool longEnough = strlen(device->dbcc_name) >= 25;
                    const bool startsOk = device->dbcc_name == strstr(device->dbcc_name, "\\\\?\\USB#VID_");
                    auto ampPIDOffset = device->dbcc_name + 16 * longEnough;
                    const bool continuesOk = ampPIDOffset == strstr(ampPIDOffset, "&PID_");
                    if(startsOk && continuesOk)
                    {
                        char vid[5], pid[5];

                        memcpy(vid, device->dbcc_name+12, 4);
                        memcpy(pid, device->dbcc_name+21, 4);
                        vid[4] = pid[4] = 0;
                        VID = (int)strtol(vid, NULL, 16);
                        PID = (int)strtol(pid, NULL, 16);

                        printf("GOT YOU NOW %04x:%04x\n", VID, PID);
                    }
                }
                if(dev->dbch_devicetype == DBT_DEVTYP_PORT)
                {
                    const auto* port = reinterpret_cast<const DEV_BROADCAST_PORT*>(dev);
                    printf("Port name: %s\n", port->dbcp_name);
                    if(VID != -1 && PID != -1)
                    {
                        startThing(VID,PID);
                        VID = PID = -1;
                    }
                }
                
            }

            break;
        case DBT_DEVICEREMOVECOMPLETE:
            msgCount++;
            StringCchPrintf(
                strBuff, 256,
                TEXT("Message %d: DBT_DEVICEREMOVECOMPLETE\n"), (int)msgCount);
            break;
        case DBT_DEVNODES_CHANGED:
            msgCount++;
            StringCchPrintf(
                strBuff, 256,
                TEXT("Message %d: DBT_DEVNODES_CHANGED\n"), (int)msgCount);
            break;
        default:
            msgCount++;
            StringCchPrintf(
                strBuff, 256,
                TEXT("Message %d: WM_DEVICECHANGE message received, value %d unhandled.\n"),
                (int)msgCount, wParam);
            break;
        }
        OutputMessage(hEditWnd, wParam, (LPARAM)strBuff);
    }
    break;
    case WM_CLOSE:
        if (!UnregisterDeviceNotification(hDeviceNotify))
        {
            ErrorHandler("UnregisterDeviceNotification");
        }
        DestroyWindow(hWnd);
        break;

    case WM_DESTROY:
        PostQuitMessage(0);
        break;

    default:
        // Send all other messages on to the default windows handler.
        lRet = DefWindowProc(hWnd, message, wParam, lParam);
        break;
    }

    return lRet;
}

#define WND_CLASS_NAME TEXT("Usbipd-win-wsl-autoattach")

BOOL InitWindowClass()
{
    WNDCLASSEX wndClass;

    wndClass.cbSize = sizeof(WNDCLASSEX);
    wndClass.style = CS_OWNDC | CS_HREDRAW | CS_VREDRAW;
    wndClass.hInstance = reinterpret_cast<HINSTANCE>(GetModuleHandle(0));
    wndClass.lpfnWndProc = reinterpret_cast<WNDPROC>(WinProcCallback);
    wndClass.cbClsExtra = 0;
    wndClass.cbWndExtra = 0;
    wndClass.hIcon = LoadIcon(0, IDI_APPLICATION);
    wndClass.hbrBackground = 0; //CreateSolidBrush(RGB(192, 192, 192));
    wndClass.hCursor = LoadCursor(0, IDC_ARROW);
    wndClass.lpszClassName = WND_CLASS_NAME;
    wndClass.lpszMenuName = NULL;
    wndClass.hIconSm = wndClass.hIcon;

    if (!RegisterClassEx(&wndClass))
    {
        ErrorHandler("RegisterClassEx");
        return FALSE;
    }
    return TRUE;
}

int __stdcall _tWinMain(
    _In_ HINSTANCE hInstanceExe,
    _In_opt_ HINSTANCE, // should not reference this parameter
    _In_ PTSTR lpstrCmdLine,
    _In_ int nCmdShow)
{
    //
    // To enable a console project to compile this code, set
    // Project->Properties->Linker->System->Subsystem: Windows.
    //

    if (!InitWindowClass())
    {
        return -1;
    }

    HWND hWnd = CreateWindowEx(
        WS_EX_CLIENTEDGE | WS_EX_APPWINDOW,
        WND_CLASS_NAME,
        "Yolo boi",
        WS_OVERLAPPEDWINDOW, // style
        CW_USEDEFAULT, 0,
        32, 32,
        NULL, NULL,
        hInstanceExe,
        NULL);

    if (hWnd == NULL)
    {
        ErrorHandler("CreateWindowEx: main appwindow hWnd");
        return -1;
    }

    UpdateWindow(hWnd);

    MessagePump(hWnd);

    return 1;
}