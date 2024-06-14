#include "ChatClient.h"
#include <string>
#include <iostream>

SOCKET ConnectSocket = INVALID_SOCKET;
HWND hEditSend;
HWND hEditRecv;

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nCmdShow)
{
    const wchar_t CLASS_NAME[] = L"ChatClientWindowClass";

    WNDCLASS wc = { };
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;

    RegisterClass(&wc);

    HWND hwnd = CreateWindowEx(
        0,
        CLASS_NAME,
        L"Chat Client",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 500, 400,
        NULL, NULL, hInstance, NULL
    );

    if (hwnd == NULL)
    {
        return 0;
    }

    ShowWindow(hwnd, nCmdShow);

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
        hEditRecv = CreateWindowW(L"EDIT", NULL,
            WS_VISIBLE | WS_CHILD | WS_BORDER | ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY,
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
            wchar_t buffer[BUFFER_SIZE];
            GetWindowText(hEditSend, buffer, BUFFER_SIZE);
            std::wstring ws(buffer);
            std::string message(ws.begin(), ws.end());
            SendMessageToServer(message);
            SetWindowText(hEditSend, L"");
        }
        else if (LOWORD(wParam) == 2) // Connect button
        {
            ConnectToServer();
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
}

void DisconnectFromServer()
{
    if (ConnectSocket != INVALID_SOCKET) {
        closesocket(ConnectSocket);
        ConnectSocket = INVALID_SOCKET;
        AppendText(hEditRecv, "Disconnected from server.\r\n");
    }
    WSACleanup();
}

void SendMessageToServer(const std::string& message)
{
    if (ConnectSocket == INVALID_SOCKET) {
        AppendText(hEditRecv, "Not connected to server.\r\n");
        return;
    }

    int iResult = send(ConnectSocket, message.c_str(), (int)message.length(), 0);
    if (iResult == SOCKET_ERROR) {
        AppendText(hEditRecv, "send failed.\r\n");
        closesocket(ConnectSocket);
        WSACleanup();
        return;
    }

    char recvbuf[BUFFER_SIZE];
    iResult = recv(ConnectSocket, recvbuf, BUFFER_SIZE, 0);
    if (iResult > 0) {
        recvbuf[iResult] = '\0';
        AppendText(hEditRecv, std::string("Server: ") + recvbuf + "\r\n");
    }
    else if (iResult == 0) {
        AppendText(hEditRecv, "Connection closed.\r\n");
    }
    else {
        AppendText(hEditRecv, "recv failed.\r\n");
    }
}

void AppendText(HWND hwnd, const std::string& text)
{
    /*int len = GetWindowTextLength(hwnd);
    SendMessage(hwnd, EM_SETSEL, (WPARAM)len, (LPARAM)len);
    SendMessage(hwnd, EM_REPLACESEL, 0, (LPARAM)text.c_str());*/
    std::wstring stemp = std::wstring(text.begin(), text.end());
    LPCWSTR sw = stemp.c_str();
    SetWindowText(hwnd, sw);
}
