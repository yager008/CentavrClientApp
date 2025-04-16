#include "framework.h"
#include "CentavrClient.h"
#include <wininet.h>  // For HTTP requests
#include <windows.h>
#include <stdio.h>  // For fopen_s, fwrite, fclose
#include <string>    // For string handling
#include <windowsx.h>  // For SS_MULTILINE constant
#include <iostream>
#include <thread>
#include <chrono>
#include <atomic>

#include <winsock2.h>
#include <iphlpapi.h>
#pragma comment(lib, "iphlpapi.lib")

#include <ws2tcpip.h>

#define MAX_LOADSTRING 100

// Global Variables:
HINSTANCE hInst;                                // current instance
WCHAR szTitle[MAX_LOADSTRING];                  // The title bar text
WCHAR szWindowClass[MAX_LOADSTRING];            // the main window class name
HWND hWndButton;                                // Handle for the button
HWND hWndTextOutput;                            // Handle for the text output

// Function prototypes
ATOM MyRegisterClass(HINSTANCE hInstance);
BOOL InitInstance(HINSTANCE hInstance, int nCmdShow);
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);
void SendHttpRequest(HWND hWnd);
void UpdateTextOutput(HWND hWnd, const std::string& text);

// Function to send HTTP GET request



std::string GetMACAddress() {
    IP_ADAPTER_INFO adapterInfo[16];
    DWORD buflen = sizeof(adapterInfo);

    if (GetAdaptersInfo(adapterInfo, &buflen) == NO_ERROR) {
        PIP_ADAPTER_INFO pAdapter = adapterInfo;
        char macAddr[18];
        sprintf_s(macAddr, "%02X-%02X-%02X-%02X-%02X-%02X",
            pAdapter->Address[0],
            pAdapter->Address[1],
            pAdapter->Address[2],
            pAdapter->Address[3],
            pAdapter->Address[4],
            pAdapter->Address[5]);

        return std::string(macAddr);
    }

    return "unknown";
}

#pragma comment(lib, "Ws2_32.lib")

std::string GetLocalIPAddress() {
    char hostname[256];
    if (gethostname(hostname, sizeof(hostname)) != 0) {
        return "unknown";
    }

    addrinfo hints = {};
    hints.ai_family = AF_INET; // Only IPv4
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    addrinfo* result = nullptr;
    if (getaddrinfo(hostname, nullptr, &hints, &result) != 0) {
        return "unknown";
    }

    char ipStr[INET_ADDRSTRLEN] = {};
    for (addrinfo* ptr = result; ptr != nullptr; ptr = ptr->ai_next) {
        sockaddr_in* sockaddr_ipv4 = reinterpret_cast<sockaddr_in*>(ptr->ai_addr);
        inet_ntop(AF_INET, &(sockaddr_ipv4->sin_addr), ipStr, sizeof(ipStr));
        break; // Use first result
    }

    freeaddrinfo(result);
    return std::string(ipStr);
}


void SendHttpRequest(HWND hWnd)
{

	WSADATA wsaData;
	WSAStartup(MAKEWORD(2, 2), &wsaData);


    HINTERNET hInternet, hConnect;
    DWORD dwBytesRead;
    DWORD dwTotalBytesRead = 0;
    DWORD dwBufferSize = 4096;  // Reading in chunks of 4KB
    BYTE* buffer = new BYTE[dwBufferSize];


	std::string ip = GetLocalIPAddress();
	std::string mac = GetMACAddress();
	std::string fullUrl = "http://localhost/CentavrJokes/main.php?ip=" + ip + "&mac=" + mac;
	const char* url = fullUrl.c_str();


    //const char* url = "https://dragonball-api.com/api/characters/1";  // URL to fetch JSON data
    //const char* url = "http://localhost/CentavrJokes/main.php";

    // Initialize WinINet
    hInternet = InternetOpenA("CentavrClient", INTERNET_OPEN_TYPE_DIRECT, NULL, NULL, 0);
    if (hInternet == NULL) {
        MessageBoxW(hWnd, L"Не удалось инициализировать интернет-соединение", L"Ошибка", MB_OK | MB_ICONERROR);
        return;
    }

    // Connect to the server (replace with your server URL)
    hConnect = InternetOpenUrlA(hInternet, url, NULL, 0, INTERNET_FLAG_RELOAD, 0);
    if (hConnect == NULL) {
        MessageBoxW(hWnd, L"Не удалось подключиться к серверу", L"Ошибка", MB_OK | MB_ICONERROR);
        InternetCloseHandle(hInternet);
        return;
    }

    // Start reading the response data
    while (InternetReadFile(hConnect, buffer, dwBufferSize, &dwBytesRead) && dwBytesRead > 0) {
        dwTotalBytesRead += dwBytesRead;
    }

    if (dwTotalBytesRead == 0) {
        MessageBoxW(hWnd, L"Ответ от сервера не содержит данных", L"Ошибка", MB_OK | MB_ICONERROR);
        delete[] buffer;
        InternetCloseHandle(hConnect);
        InternetCloseHandle(hInternet);
        return;
    }

    // Convert the buffer into a string for display
    std::string jsonResponse(reinterpret_cast<char*>(buffer), dwTotalBytesRead);

    // Clean up
    delete[] buffer;
    InternetCloseHandle(hConnect);
    InternetCloseHandle(hInternet);

    // Update the text output in the window
    UpdateTextOutput(hWnd, jsonResponse);
}

void UpdateTextOutput(HWND hWnd, const std::string& text)
{
    // Set the text to a static text control (like a label or a text box)
    SetWindowTextA(hWndTextOutput, text.c_str());
}

//
// wWinMain function
int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
                     _In_opt_ HINSTANCE hPrevInstance,
                     _In_ LPWSTR    lpCmdLine,
                     _In_ int       nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);


    // Initialize global strings
    LoadStringW(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
    LoadStringW(hInstance, IDC_CENTAVRCLIENT, szWindowClass, MAX_LOADSTRING);
    MyRegisterClass(hInstance);

    // Perform application initialization:
    if (!InitInstance(hInstance, nCmdShow))
    {
        return FALSE;
    }

    HACCEL hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_CENTAVRCLIENT));

    MSG msg;

    // Main message loop:
    while (GetMessage(&msg, nullptr, 0, 0))
    {
        if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    return (int) msg.wParam;
}

// Register the window class
ATOM MyRegisterClass(HINSTANCE hInstance)
{
    WNDCLASSEXW wcex;

    wcex.cbSize = sizeof(WNDCLASSEX);

    wcex.style          = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc    = WndProc;
    wcex.cbClsExtra     = 0;
    wcex.cbWndExtra     = 0;
    wcex.hInstance      = hInstance;
    wcex.hIcon          = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_CENTAVRCLIENT));
    wcex.hCursor        = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground  = (HBRUSH)(COLOR_WINDOW+1);
    wcex.lpszMenuName   = MAKEINTRESOURCEW(IDC_CENTAVRCLIENT);
    wcex.lpszClassName  = szWindowClass;
    wcex.hIconSm        = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

    return RegisterClassExW(&wcex);
}

// Initialize instance
BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
   hInst = hInstance; // Store instance handle in our global variable

   HWND hWnd = CreateWindowW(szWindowClass, szTitle, WS_OVERLAPPEDWINDOW,
      CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, nullptr, nullptr, hInstance, nullptr);

   if (!hWnd)
   {
      return FALSE;
   }

   // Create button
   HWND hWndButton = CreateWindowW(
      L"BUTTON",  // Predefined class for buttons
      L"Get Bible Info",  // Button text
      WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,  // Button style
      50, 50,  // Position of the button
      200, 30,  // Size of the button
      hWnd,     // Parent window
      (HMENU)1, // ID of the button
      hInstance,  // Instance handle
      NULL
   );

   // Create a static control for displaying text (the JSON output)

	hWndTextOutput = CreateWindowExW(
		WS_EX_CLIENTEDGE,
		L"EDIT",
		L"",
		WS_VISIBLE | WS_CHILD | ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY | WS_VSCROLL,
		50, 100,
		300, 200,
		hWnd,
		NULL,
		hInstance,
		NULL
	);


   if (!hWndButton || !hWndTextOutput)
   {
      return FALSE; // If button or text output creation fails, return FALSE
   }

   ShowWindow(hWnd, nCmdShow);
   UpdateWindow(hWnd);

   return TRUE;
}


INT_PTR CALLBACK LoginProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_INITDIALOG:
        return (INT_PTR)TRUE;

    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK)
        {
            TCHAR username[100], password[100];
            GetDlgItemText(hDlg, IDC_USERNAME, username, 100);
            GetDlgItemText(hDlg, IDC_PASSWORD, password, 100);

            // TODO: Replace this with actual login logic
            if (wcscmp(username, L"admin") == 0 && wcscmp(password, L"1234") == 0)
            {
                EndDialog(hDlg, IDOK);
            }
            else
            {
                MessageBox(hDlg, L"Invalid username or password", L"Login Failed", MB_ICONERROR);
            }
            return (INT_PTR)TRUE;
        }
        else if (LOWORD(wParam) == IDCANCEL)
        {
            EndDialog(hDlg, IDCANCEL);
            return (INT_PTR)TRUE;
        }
        break;
    }
    return (INT_PTR)FALSE;
}




#define IDT_HTTP_TIMER 1

// WndProc function
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {

    case WM_CREATE:
        SetTimer(hWnd, IDT_HTTP_TIMER, 2000, NULL); // 5 seconds
        break;
    case WM_TIMER:
        if (wParam == IDT_HTTP_TIMER) {
            SendHttpRequest(hWnd);
        }
        break;
    case WM_COMMAND:
        {
            int wmId = LOWORD(wParam);
            switch (wmId)
            {
            case IDM_ABOUT:
                DialogBox(hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, About);
                break;
            case IDM_EXIT:
                DestroyWindow(hWnd);
                break;
            case 1: // Handling the button press with ID 1
                SendHttpRequest(hWnd);

                break;
            default:
                return DefWindowProc(hWnd, message, wParam, lParam);
            }
        }
        break;
    case WM_PAINT:
        {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hWnd, &ps);
            EndPaint(hWnd, &ps);
        }
        break;
    case WM_DESTROY:
        KillTimer(hWnd, IDT_HTTP_TIMER);
        PostQuitMessage(0);
        break;
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

// About dialog function
INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    switch (message)
    {
    case WM_INITDIALOG:
        return (INT_PTR)TRUE;

    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
        {
            EndDialog(hDlg, LOWORD(wParam));
            return (INT_PTR)TRUE;
        }
        break;
    }
    return (INT_PTR)FALSE;
}
