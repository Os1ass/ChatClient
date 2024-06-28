#include "ChatClient.h"
#include <string>
#include <iostream>
#include <codecvt>
#include <atomic>

SOCKET ConnectSocket = INVALID_SOCKET;
HANDLE workerThread  = INVALID_HANDLE_VALUE;
HWND hEditSend;
HWND hEditRecv;
BOOL cancellationToken;
std::string userNickname;

const BYTE magicNumber[4] = { 0xAA, 0xBB, 0xCC, 0xDD };
const std::string magicNumberString(reinterpret_cast<const char*>(magicNumber), sizeof(magicNumber));
// magicNumber + message + magicNumber
// identifiation for socket send/recv

DWORD WINAPI WorkerThread(LPVOID lpParam);

BOOL CALLBACK DlgProc(HWND hwndDlg, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) 
    {
    case WM_INITDIALOG:
        return TRUE;

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
                userNickname = buffer;
                delete[] buffer;
                EndDialog(hwndDlg, IDOK);
            } else 
            {
                MessageBox(hwndDlg, L"Please enter a nickname.", L"Error", MB_OK | MB_ICONERROR);
            }
            return TRUE;
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
    setlocale(LC_ALL, "RUSSIAN");
    const wchar_t CLASS_NAME[] = L"ChatClientWindowClass";
    cancellationToken = false;

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
        CW_USEDEFAULT, CW_USEDEFAULT, 500, 400,
        NULL, NULL, hInstance, NULL
    );

    if (hwnd == NULL)
    {
        return 0;
    }

    ShowWindow(hwnd, nCmdShow);

    ShowNicknameDialog(hwnd, hInstance);
    AppendText(hEditRecv, "Hello, " + userNickname);

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
        hEditRecv = CreateWindow(L"EDIT", NULL,
            WS_HSCROLL | WS_VSCROLL | WS_VISIBLE | WS_CHILD | 
            WS_BORDER | ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY,
            10, 30, 460, 250, hwnd, NULL, NULL, NULL);

        CreateWindow(L"STATIC", L"Enter Message:", WS_VISIBLE | WS_CHILD,
            10, 290, 460, 20, hwnd, NULL, NULL, NULL);
        hEditSend = CreateWindow(L"EDIT", NULL,
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
    }
    return 0;

    case WM_COMMAND:
    {
        if (LOWORD(wParam) == 1) // Send button
        {
            wchar_t* wbuffer = new wchar_t[BUFFER_SIZE];
            GetWindowText(hEditSend, wbuffer, BUFFER_SIZE);

            size_t bufferSize;
            char* buffer = new char[BUFFER_SIZE];
            wcstombs_s(&bufferSize, buffer, BUFFER_SIZE, wbuffer, _TRUNCATE);
            SendMessageToServer(std::string(buffer, strlen(buffer)));
            SetWindowText(hEditSend, L"");

            delete[] wbuffer;
            delete[] buffer;
        }
        else if (LOWORD(wParam) == 2) // Connect button
        {
            if (workerThread == INVALID_HANDLE_VALUE)
                ConnectToServer();
            else
                AppendText(hEditRecv, "You are already connected to server.\r\n");
        }
        else if (LOWORD(wParam) == 3) // Disconnect button
        {
            DisconnectFromServer();
        }
    }
    return 0;

    case WM_DESTROY:
        DisconnectFromServer();
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

void ConnectToServer()
{
    WSADATA wsaData;
    int iResult;

    iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (iResult != 0) {
        AppendText(hEditRecv, "WSAStartup failed.\r\n");
        return;
    }

    struct addrinfo* result = NULL, * ptr = NULL, hints;
    ZeroMemory(&hints, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    iResult = getaddrinfo("127.0.0.1", DEFAULT_PORT, &hints, &result);
    if (iResult != 0) {
        AppendText(hEditRecv, "getaddrinfo failed.\r\n");
        WSACleanup();
        return;
    }

    for (ptr = result; ptr != NULL; ptr = ptr->ai_next) {
        ConnectSocket = socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol);
        if (ConnectSocket == INVALID_SOCKET) {
            AppendText(hEditRecv, "Error at socket().\r\n");
            freeaddrinfo(result);
            WSACleanup();
            return;
        }

        iResult = connect(ConnectSocket, ptr->ai_addr, (int)ptr->ai_addrlen);
        if (iResult == SOCKET_ERROR) {
            closesocket(ConnectSocket);
            ConnectSocket = INVALID_SOCKET;
            continue;
        }
        break;
    }

    freeaddrinfo(result);

    if (ConnectSocket == INVALID_SOCKET) {
        AppendText(hEditRecv, "Unable to connect to server.\r\n");
        WSACleanup();
        return;
    }

    AppendText(hEditRecv, "Connected to server.\r\n");
    SendMessageToServer(userNickname);
    workerThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)WorkerThread, NULL, 0, NULL);
}

DWORD WINAPI WorkerThread(LPVOID lpParam)
{
    std::string message;
    int iResult;
    while (!cancellationToken)
    {
        iResult = RecieveMessageFromServer(message);
        if (iResult > 0)
        {
            AppendText(hEditRecv, message + "\r\n");
            OutputDebugStringA(message.c_str());
        }
    }
    return 0;
}

void DisconnectFromServer()
{
    if (ConnectSocket != INVALID_SOCKET) {
        cancellationToken = true;
        shutdown(ConnectSocket, SD_BOTH);
        if (WaitForSingleObject(workerThread, 5000) != WAIT_OBJECT_0)
        {
            TerminateThread(workerThread, 1);
        }
        CloseHandle(workerThread);
        closesocket(ConnectSocket);
        ConnectSocket = INVALID_SOCKET;
        workerThread = INVALID_HANDLE_VALUE;
        AppendText(hEditRecv, "Disconnected from server.\r\n");
    }
    WSACleanup();
}

void SendMessageToServer(std::string message)
{
    if (ConnectSocket == INVALID_SOCKET) {
        AppendText(hEditRecv, "Not connected to server.\r\n");
        return;
    }

    message = magicNumberString + message + magicNumberString;
    int iResult = send(ConnectSocket, message.c_str(), message.length(), 0);
    if (iResult == SOCKET_ERROR) {
        AppendText(hEditRecv, "Send failed.\r\n");
        WSACleanup();
        return;
    }
}

int RecieveMessageFromServer(std::string& message)
{
    char buffer[BUFFER_SIZE];
    int iResult = recv(ConnectSocket, buffer, BUFFER_SIZE, 0);
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
