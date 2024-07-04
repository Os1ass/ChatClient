#include "ChatClient.h"
#include <string>
#include <iostream>
#include <codecvt>
#include <atomic>

SOCKET g_connectSocket = INVALID_SOCKET;
HANDLE g_workerThread  = INVALID_HANDLE_VALUE;
HWND g_hEditSend;
HWND g_hEditRecv;
BOOL g_cancellationToken;
std::string g_userNickname = "";
std::string g_serverIpAdress = "127.0.0.1";
std::string g_serverPort = "26999";
HINSTANCE g_hInstance;

const BYTE magicNumber[4] = { 0xAA, 0xBB, 0xCC, 0xDD };
const std::string magicNumberString(reinterpret_cast<const char*>(magicNumber), sizeof(magicNumber));
// magicNumber + message + magicNumber
// identifiation for socket send/recv

DWORD WINAPI WorkerThread(LPVOID lpParam);

BOOL CALLBACK DlgProc(HWND hwndDlg, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) 
    {
    case WM_INITDIALOG:
        SetDlgItemTextA(hwndDlg, IDC_EDIT1, g_userNickname.c_str());
        SetDlgItemTextA(hwndDlg, IDC_EDIT2, g_serverIpAdress.c_str());
        SetDlgItemTextA(hwndDlg, IDC_EDIT3, g_serverPort.c_str());
        return FALSE;

    case WM_COMMAND:
        switch (LOWORD(wParam))
        {
        case IDOK:
        {
            wchar_t wbuffer[BUFFER_SIZE];
            GetDlgItemText(hwndDlg, IDC_EDIT1, wbuffer, BUFFER_SIZE);

            if (wcslen(wbuffer) > 0)
            {
                size_t bufferSize;
                char* buffer = new char[BUFFER_SIZE];
                wcstombs_s(&bufferSize, buffer, BUFFER_SIZE, wbuffer, _TRUNCATE);
                g_userNickname = buffer;
                delete[] buffer;
            }
            else
            {
                MessageBox(hwndDlg, L"Please enter a nickname.", L"Error", MB_OK | MB_ICONERROR);
                return FALSE;
            }

            wbuffer[0] = '\0';
            GetDlgItemText(hwndDlg, IDC_EDIT2, wbuffer, BUFFER_SIZE);

            if (wcslen(wbuffer) > 0)
            {
                size_t bufferSize;
                char* buffer = new char[BUFFER_SIZE];
                wcstombs_s(&bufferSize, buffer, BUFFER_SIZE, wbuffer, _TRUNCATE);
                g_serverIpAdress = buffer;
                delete[] buffer;
            }
            else
            {
                MessageBox(hwndDlg, L"Please enter a server ip adress.", L"Error", MB_OK | MB_ICONERROR);
                return FALSE;
            }

            wbuffer[0] = '\0';
            GetDlgItemText(hwndDlg, IDC_EDIT3, wbuffer, BUFFER_SIZE);

            if (wcslen(wbuffer) > 0)
            {
                size_t bufferSize;
                char* buffer = new char[BUFFER_SIZE];
                wcstombs_s(&bufferSize, buffer, BUFFER_SIZE, wbuffer, _TRUNCATE);
                g_serverPort = buffer;
                delete[] buffer;

            }
            else
            {
                MessageBox(hwndDlg, L"Please enter a server port.", L"Error", MB_OK | MB_ICONERROR);
                return FALSE;
            }

            EndDialog(hwndDlg, IDOK);
            return FALSE;
        }
        case WM_DESTROY:
        {
            if (wParam != IDOK)
            {
                PostQuitMessage(0);
            }
            return FALSE;
        }
        }
        break;
    }
    return FALSE;
}

void ShowNicknameDialog(HWND hwndParent, HINSTANCE hInstance) {
    DialogBox(hInstance, MAKEINTRESOURCE(IDD_DIALOG1), hwndParent, (DLGPROC)DlgProc);
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nCmdShow)
{
    g_hInstance = hInstance;
    setlocale(LC_ALL, "RUSSIAN");
    const wchar_t CLASS_NAME[] = L"ChatClientWindowClass";
    g_cancellationToken = false;

    WNDCLASS wc = { };
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;

    RegisterClass(&wc);

    HWND hwnd = CreateWindowEx(
        0,
        CLASS_NAME,
        L"Chat Client",
        WS_OVERLAPPEDWINDOW ^ WS_THICKFRAME,
        CW_USEDEFAULT, CW_USEDEFAULT, 500, 430,
        NULL, NULL, hInstance, NULL
    );

    if (hwnd == NULL)
    {
        return 0;
    }

    HMENU hMenu = CreateMenu();
    AppendMenu(hMenu, MF_STRING, IDM_SETTINGS, L"Settings");
    SetMenu(hwnd, hMenu);

    ShowWindow(hwnd, nCmdShow);

    ShowNicknameDialog(hwnd, hInstance);
    AppendText(g_hEditRecv, "Hello, " + g_userNickname);

    MSG msg = { };
    while (GetMessage(&msg, NULL, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return 0;
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_CREATE:
    {
        CreateWindow(L"STATIC", L"Messages:", WS_VISIBLE | WS_CHILD,
            10, 10, 460, 20, hwnd, NULL, NULL, NULL);
        g_hEditRecv = CreateWindow(L"EDIT", NULL,
            WS_HSCROLL | WS_VSCROLL | WS_VISIBLE | WS_CHILD | 
            WS_BORDER | ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY,
            10, 30, 460, 250, hwnd, NULL, NULL, NULL);

        CreateWindow(L"STATIC", L"Enter Message:", WS_VISIBLE | WS_CHILD,
            10, 290, 460, 20, hwnd, NULL, NULL, NULL);
        g_hEditSend = CreateWindow(L"EDIT", NULL,
            WS_VISIBLE | WS_CHILD | WS_BORDER,
            10, 310, 360, 20, hwnd, NULL, NULL, NULL);

        CreateWindow(L"BUTTON", L"Send",
            WS_VISIBLE | WS_CHILD | WS_BORDER,
            380, 310, 90, 20, hwnd, (HMENU)1, NULL, NULL);

        CreateWindow(L"BUTTON", L"Connect",
            WS_VISIBLE | WS_CHILD | WS_BORDER,
            10, 340, 90, 20, hwnd, (HMENU)2, NULL, NULL);

        CreateWindow(L"BUTTON", L"Disconnect",
            WS_VISIBLE | WS_CHILD | WS_BORDER,
            110, 340, 90, 20, hwnd, (HMENU)3, NULL, NULL);

        return FALSE;
    }

    case WM_COMMAND:
    {
        switch (LOWORD(wParam))
        {
        case 1: //send button
        {
            wchar_t* wbuffer = new wchar_t[BUFFER_SIZE];
            GetWindowText(g_hEditSend, wbuffer, BUFFER_SIZE);

            size_t bufferSize;
            char* buffer = new char[BUFFER_SIZE];
            wcstombs_s(&bufferSize, buffer, BUFFER_SIZE, wbuffer, _TRUNCATE);
            SendMessageToServer(std::string(buffer, strlen(buffer)));
            SetWindowText(g_hEditSend, L"");

            delete[] wbuffer;
            delete[] buffer;
            return FALSE;
        }
        case 2: //connect button
        {
            if (g_workerThread == INVALID_HANDLE_VALUE)
            {
                ConnectToServer();
            }
            else
            {
                AppendText(g_hEditRecv, "You are already connected to server.\r\n");
            }
            return FALSE;
        }
        case 3: //disconnect button
        {
            if (g_workerThread == INVALID_HANDLE_VALUE)
            {
                AppendText(g_hEditRecv, "Not connected to server.\r\n");
            }
            else
            {
                DisconnectFromServer();
            }
            return FALSE;
        }
        case IDM_SETTINGS:
        {
            if (g_workerThread != INVALID_HANDLE_VALUE)
            {
                MessageBox(hwnd, L"First disconnect from server", L"Settings", MB_OK | MB_ICONINFORMATION);
                return FALSE;
            }
            ShowNicknameDialog(hwnd, g_hInstance);
            return FALSE;
        }
        case IDM_ABOUT:
        {
            MessageBox(hwnd, L"ToDo", L"Error", MB_OK | MB_ICONERROR);
            return FALSE;
        }
        }
        return FALSE;
    }

    case WM_DESTROY:
        DisconnectFromServer();
        PostQuitMessage(0);
        return FALSE;
    }

    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

void ConnectToServer()
{
    WSADATA wsaData;
    int iResult;

    iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (iResult != 0) {
        AppendText(g_hEditRecv, "WSAStartup failed.\r\n");
        return;
    }

    struct addrinfo* result = NULL, * ptr = NULL, hints;
    ZeroMemory(&hints, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    iResult = getaddrinfo(g_serverIpAdress.c_str(), g_serverPort.c_str(), &hints, &result);
    if (iResult != 0) {
        AppendText(g_hEditRecv, "getaddrinfo failed.\r\n");
        WSACleanup();
        return;
    }

    for (ptr = result; ptr != NULL; ptr = ptr->ai_next) {
        g_connectSocket = socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol);
        if (g_connectSocket == INVALID_SOCKET) {
            AppendText(g_hEditRecv, "Error at socket().\r\n");
            freeaddrinfo(result);
            WSACleanup();
            return;
        }

        iResult = connect(g_connectSocket, ptr->ai_addr, (int)ptr->ai_addrlen);
        if (iResult == SOCKET_ERROR) {
            closesocket(g_connectSocket);
            g_connectSocket = INVALID_SOCKET;
            continue;
        }
        break;
    }

    freeaddrinfo(result);

    if (g_connectSocket == INVALID_SOCKET) {
        AppendText(g_hEditRecv, "Unable to connect to server.\r\n");
        WSACleanup();
        return;
    }

    SendMessageToServer(g_userNickname);
    std::string greeting;
    if (RecieveMessageFromServer(greeting) == 0)
    {
        AppendText(g_hEditRecv, "Can't connect to the server, name " + g_userNickname + " already in use.\r\n");
    }
    else
    {
        AppendText(g_hEditRecv, greeting + "\r\n");
        g_workerThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)WorkerThread, NULL, 0, NULL);
    }
}

DWORD WINAPI WorkerThread(LPVOID lpParam)
{
    std::string message;
    int iResult;
    while (!g_cancellationToken)
    {
        iResult = RecieveMessageFromServer(message);
        if (iResult > 0)
        {
            if (message.length() >= g_userNickname.length() &&
                message.substr(0, g_userNickname.length()) == g_userNickname)
            {
                message.replace(0, g_userNickname.length(), "You");
            }
            AppendText(g_hEditRecv, message + "\r\n");
            OutputDebugStringA(message.c_str());
        }
    }
    return 0;
}

void DisconnectFromServer()
{
    if (g_connectSocket != INVALID_SOCKET) {
        g_cancellationToken = true;
        shutdown(g_connectSocket, SD_BOTH);
        if (WaitForSingleObject(g_workerThread, 5000) != WAIT_OBJECT_0)
        {
            TerminateThread(g_workerThread, 1);
        }
        CloseHandle(g_workerThread);
        closesocket(g_connectSocket);
        g_connectSocket = INVALID_SOCKET;
        g_workerThread = INVALID_HANDLE_VALUE;
        AppendText(g_hEditRecv, "Disconnected from server.\r\n");
    }
    WSACleanup();
}

void SendMessageToServer(std::string message)
{
    if (g_connectSocket == INVALID_SOCKET) {
        AppendText(g_hEditRecv, "Not connected to server.\r\n");
        return;
    }

    message = magicNumberString + message + magicNumberString;
    int iResult = send(g_connectSocket, message.c_str(), message.length(), 0);
    if (iResult == SOCKET_ERROR) {
        AppendText(g_hEditRecv, "Send failed.\r\n");
        WSACleanup();
        return;
    }
}

int RecieveMessageFromServer(std::string& message)
{
    char buffer[BUFFER_SIZE];
    int iResult = recv(g_connectSocket, buffer, BUFFER_SIZE, 0);
    if (iResult <= magicNumberString.length() * 2 ||
        iResult <= 0)
    {
        return 0;
    }

    std::string bufferStr(buffer, iResult);
    if (bufferStr.substr(0, 4) != magicNumberString ||
        bufferStr.substr(bufferStr.length() - 4, 4) != magicNumberString)
    {
        return 0;
    }

    message = bufferStr.substr(4, bufferStr.length() - 8);
    return iResult;
}

void AppendText(HWND hwnd, const std::string& text)
{
    size_t textSize;
    wchar_t* wtext = new wchar_t[BUFFER_SIZE];
    mbstowcs_s(&textSize, wtext, BUFFER_SIZE, text.c_str(), _TRUNCATE);

    wchar_t* windowText = new wchar_t[BUFFER_SIZE];
    int windowTextLength = GetWindowTextLength(hwnd);
    GetWindowText(hwnd, windowText, BUFFER_SIZE);

    wcscat_s(wtext, BUFFER_SIZE, windowText);
    SetWindowText(hwnd, wtext);

    delete[] windowText;
    delete[] wtext;
}
