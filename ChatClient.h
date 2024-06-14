// ChatClient.h
#pragma once
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <string>

#pragma comment(lib, "Ws2_32.lib")

#define DEFAULT_PORT "27015"
#define BUFFER_SIZE 512

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
void ConnectToServer();
void DisconnectFromServer();
void SendMessageToServer(const std::string& message);
void AppendText(HWND hwnd, const std::string& text);

extern SOCKET ConnectSocket;
extern HWND hEditSend;
extern HWND hEditRecv;