#include <iostream> 
#include <windows.h>
#include <wtsapi32.h>

#define COMMAND_AMOUNT	L"-amount"
#define COMMAND_WAIT	L"-wait"
#define COMMAND_HELP	L"-help"
#define COMMAND_HELP2	L"-?"

//Globals
DWORD g_InteractiveLimitBytes = 0;
DWORD g_NonInteractiveLimitBytes = 0;

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg)
	{
	case WM_CLOSE:
		DestroyWindow(hwnd);
		break;
	case WM_DESTROY:
		PostQuitMessage(0);
		break;
	default:
		return DefWindowProc(hwnd, msg, wParam, lParam);
	}
	return 0;
}

DWORD get_sessionid()
{
	DWORD* pdwSessionId = NULL;
	DWORD bytesreturned = 0;

	if (!WTSQuerySessionInformation(WTS_CURRENT_SERVER_HANDLE, WTS_CURRENT_SESSION, WTSSessionId, (LPWSTR*)&pdwSessionId, &bytesreturned))
	{
		wprintf_s(L"WTSQuerySesionInformation failed. Last error: %d. Aborting.\n", GetLastError());

		exit(-1);
	}

	//WTSFreeMemory(pdwSessionId);

	return *pdwSessionId;
}

LPWSTR get_user()
{
	LPWSTR pwszUsername = NULL;
	DWORD cbUsername = 0;

	if (!WTSQuerySessionInformation(WTS_CURRENT_SERVER_HANDLE, WTS_CURRENT_SESSION, WTSUserName, &pwszUsername, &cbUsername))
	{
		wprintf_s(L"WTSQuerySesionInformation failed. Last error: %d. Aborting.\n", GetLastError());

		exit(-1);
	}

	//WTSFreeMemory(pwszUsername);

	return pwszUsername;
}

LPWSTR get_domain()
{
	LPWSTR pwszDomain = NULL;
	DWORD cbDomain = 0;

	if (!WTSQuerySessionInformation(WTS_CURRENT_SERVER_HANDLE, WTS_CURRENT_SESSION, WTSDomainName, &pwszDomain, &cbDomain))
	{
		wprintf_s(L"WTSQuerySesionInformation failed. Last error: %d. Aborting.\n", GetLastError());

		exit(-1);
	}

	//WTSFreeMemory(pwszUsername);

	return pwszDomain;
}

void get_limits() {
	HKEY hKey;
	WCHAR szBuffer[MAX_PATH] = {};
	LPCWSTR pCurrentPos = NULL;
	DWORD dwBufferSize = sizeof(szBuffer);
	WCHAR szSharedSection[] = L"SharedSection";
	WCHAR szSharedHeapSize[10] = {};
	WCHAR szInteractiveSize[10] = {};
	WCHAR szNonInteractiveSize[10] = {};
	int i = 0;

	if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"SYSTEM\\CurrentControlSet\\Control\\Session Manager\\SubSystems", 0, KEY_READ, &hKey) != ERROR_SUCCESS) {

		wprintf_s(L"Unable to read from the registry. Last Error: %d. Aborting.\n", GetLastError());

		exit(-1);
	}


	if (RegQueryValueExW(hKey, L"Windows", 0, NULL, (LPBYTE)szBuffer, &dwBufferSize) != ERROR_SUCCESS) {

		wprintf_s(L"Unable to read from the registry. Last Error: %d. Aborting.\n", GetLastError());

		exit(-1);
	}

	//Find SharedSection
	pCurrentPos = wcsstr(szBuffer, szSharedSection);

	if (pCurrentPos == NULL)
	{
		wprintf_s(L"SharedSection not found in the registry. Aborting.");

		exit(-1);

	}

	//Advance to 1st value
	pCurrentPos = pCurrentPos + wcslen(szSharedSection);
	wcsncpy_s(szSharedHeapSize, 10, pCurrentPos, wcscspn(pCurrentPos, L","));

	//Advance to 2nd value
	pCurrentPos = pCurrentPos + wcscspn(pCurrentPos, L",") + 1;
	wcsncpy_s(szInteractiveSize, 10, pCurrentPos, wcscspn(pCurrentPos, L","));

	//Advance to 3rd value
	pCurrentPos = pCurrentPos + wcscspn(pCurrentPos, L",") + 1;
	wcsncpy_s(szNonInteractiveSize, 10, pCurrentPos, wcscspn(pCurrentPos, L" "));

	//Populate Globals
	g_InteractiveLimitBytes = wcstol(szInteractiveSize, NULL, 10) * 1024;
	g_NonInteractiveLimitBytes = wcstol(szNonInteractiveSize, NULL, 10) * 1024;

}



void probe_heap(int lAmountKB, int iWait) {


	//Window Class
	WNDCLASSEX wc;
	WCHAR szClassName[260] = {};
	wc.cbSize = sizeof(WNDCLASSEX);
	wc.style = 0;
	wc.lpfnWndProc = WndProc;
	wc.cbClsExtra = 4096;
	wc.cbWndExtra = 0;
	wc.hInstance = GetModuleHandle(NULL);
	wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);
	wc.hCursor = LoadCursor(NULL, IDC_ARROW);
	wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
	wc.lpszMenuName = NULL;
	wc.hIconSm = LoadIcon(NULL, IDI_APPLICATION);

	//Total structure size
	int TotalSizeBytes = wc.cbSize + wc.cbClsExtra;

	//No. of iterations
	int x = 0, y = 0, cCount = 0;
	bool bLimitReached = false;

	if (lAmountKB != NULL)
		cCount = (lAmountKB * 1024) / TotalSizeBytes;
	else
		cCount = INT32_MAX;

	for (x = 0; x < cCount; x++)
	{
		swprintf_s(szClassName, 100, L"myWindowClass%d", x);

		wc.lpszClassName = szClassName;

		if (!RegisterClassEx(&wc))
		{

			//wprintf_s(L"Error RegisterClassEx failed. Error: %d\n", GetLastError());
						
			bLimitReached = true;

			break;

		}

	}

	//Wait if requested
	if (iWait != NULL)
	{
		wprintf_s(L"Sleeping for %d seconds...", iWait);

		Sleep(iWait * 1000);

		wprintf_s(L"done. \n");
	}

	//Release
	for (y = x; y > 0; y--)
	{
		swprintf_s(szClassName, 100, L"myWindowClass%d", y - 1);

		if (!UnregisterClassW(szClassName, NULL))
		{
			wprintf_s(L"Error UnregisterClassEx failed. Error: %d\n", GetLastError());

			break;
		}

	}

	//Reporting
	int iSessionID = get_sessionid();
	float CurrentUsage = 0.0f;

	if (iSessionID > 0) {

		wprintf_s(L"Allocated %d bytes (%d KB) from Interactive Desktop Heap for account %s\\%s\n", x * TotalSizeBytes, (x * TotalSizeBytes) / 1024, get_domain(), get_user());

		if (bLimitReached) {

			CurrentUsage = ((((x * TotalSizeBytes) / (float)g_InteractiveLimitBytes) * 100) - 100) * -1;

			wprintf_s(L"Interactive Desktop Heap is %.1f%% busy. Current limit: %d KB\n", CurrentUsage, g_InteractiveLimitBytes / 1024);
		}
	}
	else {

		wprintf_s(L"Allocated %d bytes (%d KB) from Non-Interactive Desktop Heap for account %s\n", x * TotalSizeBytes, (x * TotalSizeBytes) / 1024, get_domain(), get_user());

		if (bLimitReached) {

			CurrentUsage = ((((x * TotalSizeBytes) / (float)g_NonInteractiveLimitBytes) * 100) - 100) * -1;

			wprintf_s(L"Non-Interactive Desktop Heap is %.1f%% busy. Current limit: %d KB\n", CurrentUsage, g_NonInteractiveLimitBytes / 1024);
		}
	}


}

void print_help() {
    std::wcout << "  Usage: DesktopHeapDiag.exe [SWITCHES]\n" << std::endl;
	std::wcout << "\t" << COMMAND_AMOUNT << L"\t(optional) : amount of memory (in KB) to allocate from the Desktop Heap, rounded to the nearest multiple" << std::endl;
	std::wcout << "\t" << COMMAND_WAIT << L"\t(optional) : Delay (in seconds) to wait before releasing the memory" << std::endl;
	std::wcout << "\t" << COMMAND_HELP << L"\t(optional) : Prints this Help" << std::endl;
	std::wcout << "\n Examples: " << std::endl;
	std::wcout << "\t DesktopHeapDiag.exe" << std::endl;
	std::wcout << "\t   Probes the heap and displays the current usage\n" << std::endl;
	std::wcout << "\t DesktopHeapDiag.exe -amount 500" << std::endl;
	std::wcout << "\t   Allocates 500 KB of Desktop Heap\n" << std::endl;
	std::wcout << "\t DesktopHeapDiag.exe -amount 500 -wait 10" << std::endl;
	std::wcout << "\t   Allocates 500 KB of Desktop Heap and waits 10 seconds\n" << std::endl;

}


int wmain(int argc, LPWSTR* argv)
{
	std::wcout << "\n  DesktopHeapDiag - Desktop Heap Diagnostic Tool \n  https://github.com/leonardomsft/DesktopHeapDiag \n" << std::endl;

	get_limits();

	int lAmountKB = 0;
	int iWait = 0;

	for (int i = 1; i < argc; i++)
	{
		if (!wcscmp(COMMAND_AMOUNT, argv[i]))
			lAmountKB = wcstol(argv[++i], NULL, 10);
		else if (!wcscmp(COMMAND_WAIT, argv[i]))
			iWait = wcstol(argv[++i], NULL, 10);
		else if (!wcscmp(COMMAND_HELP, argv[i])) {
			print_help();
			return 0;
		}
		else if (!wcscmp(COMMAND_HELP2, argv[i])) {
			print_help();
			return 0;
		}
		else {
			std::wcerr << L"Option not recognized: " << argv[i] << std::endl;

			print_help();

			return -1;
		}
	}

	probe_heap(lAmountKB, iWait);

}
