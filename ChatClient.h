// ChatClient.h
#pragma once
#include "resource.h"
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <string>

#pragma comment(lib, "Ws2_32.lib")

#define BUFFER_SIZE 4096

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
void ConnectToServer();
void DisconnectFromServer();
void SendMessageToServer(std::string message);
int RecieveMessageFromServer(std::string& message);
void AppendText(HWND hwnd, const std::string& text);

extern SOCKET g_connectSocket;
extern HWND g_hEditSend;
extern HWND g_hEditRecv;
