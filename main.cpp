#include <iostream>
#include <Windows.h>
#include <string>
#include <regex>
#include <vector>
#include <functional>

// TODO: some testing is needed in the timerProc, and in divideNconquer lambda
//       just to make sure I didn't miss and obvious issue

std::wstring wallpaperDir;
std::vector<std::pair<std::wstring,int>> wallpapers; // [[name,value],..] value is time in minutes
size_t currWallpaperidx = 0;

bool isRunning = true;
UINT_PTR timerID;
unsigned int debugTime = 5000; // every 5 seconds for debuging

BOOL WINAPI consoleHandlerRoutine(DWORD dwCtrlType)
{
	switch (dwCtrlType)
	{
	case CTRL_C_EVENT:
	case CTRL_CLOSE_EVENT:
	case CTRL_BREAK_EVENT:
	case CTRL_LOGOFF_EVENT:
	case CTRL_SHUTDOWN_EVENT:
		isRunning = false;
		return TRUE;

	default:
		return FALSE;
	}
}

void CALLBACK timerProc(HWND hwnd, UINT uMsg, UINT_PTR idEvent, DWORD dwTime)
{
	currWallpaperidx = (currWallpaperidx + 1) % wallpapers.size();
	std::wcout << "displaying " << wallpapers[currWallpaperidx].first << '\n';
	
	// NOTE: doesn't accept png paths for some reason
	SystemParametersInfoW(SPI_SETDESKWALLPAPER, 0,
		(PVOID)(wallpaperDir + L'\\' + wallpapers[currWallpaperidx].first).c_str(), SPIF_UPDATEINIFILE);

#ifdef NDEBUG
	SYSTEMTIME st;
	GetLocalTime(&st);
	const int currTimeMinutes = st.wHour * 60 + st.wMinute;

	size_t nextWallpaperidx = (currWallpaperidx + 1) % wallpapers.size();

	SetTimer(NULL, timerID,
		(wallpapers[nextWallpaperidx].second - currTimeMinutes) * 60000, &timerProc);

	std::wcout << "next wallpaper in " << wallpapers[nextWallpaperidx].second - currTimeMinutes << "m\n";
#endif
}

int main(int argc,char* argv[])
{
	/* ERROR CHECKING */
	// check wallpaper folder
	wallpaperDir = std::wstring(MAX_PATH,' ');
	GetModuleFileNameW(NULL, &wallpaperDir[0], MAX_PATH);
	wallpaperDir = wallpaperDir.substr(0, wallpaperDir.find_last_of('\\')) + L"\\wallpapers";

	DWORD attrs = GetFileAttributesW(wallpaperDir.c_str());
	if (attrs != FILE_ATTRIBUTE_DIRECTORY)
	{
		std::cout << "Wallpapers directory was not found, make sure to put wallpapers in a directory called \"wallpapers\" next to exe" << '\n';
		return -1;
	}

	// check if wallpaper is empty
	WIN32_FIND_DATAW fileData;
	HANDLE file = FindFirstFileW((wallpaperDir + L"\\*").c_str(), &fileData);
	
	if (file == INVALID_HANDLE_VALUE)
	{
		std::cout << "wallpaper folder is empty ¯\\_('-')_/¯" << '\n';
		return -1;
	}

	// check that all files match the pattern
	std::wregex regex(L"^([0-1][0-9]|[2][0-3])\\.[0-5][0-9]\\.jpg$");
	do
	{
		// get string
		std::wstring fileNameAsWstring = std::wstring(fileData.cFileName); // cFileName is WCHAR[] and won't compare to a wstring unless converted, typical win32 nonsense
		if (fileNameAsWstring == L"." || fileNameAsWstring == L"..")
			continue;

		if (std::regex_match(fileNameAsWstring, regex) == false)
		{
			std::wcout << L"file has invalid name: " << fileNameAsWstring << L" make sure that file name matches the pattern hh.mm and is jpg" << '\n';
			return -1;
		}

		// get wallpaper time as int
		size_t dotIdx[3];
		BYTE currIdx = 0;
		for(int i = 0; i < fileNameAsWstring.length(); i++)
		{
			if (fileNameAsWstring[i] == '.')
			{
				dotIdx[currIdx] = i;
				currIdx++;
			}
		}
		// convert to minutes
		int time = std::stoi(fileNameAsWstring.substr(0, dotIdx[0]))*60 + std::stoi(fileNameAsWstring.substr(dotIdx[0] + 1, dotIdx[1]));

		// NOTE: FindNextFileW will iterate files in alphabetic order, no need to sort
		wallpapers.push_back(std::pair<std::wstring,int> (fileNameAsWstring, time));

	} while (FindNextFileW(file, &fileData) != 0);



	// console
	SetConsoleCtrlHandler(&consoleHandlerRoutine, TRUE);
	if (argc > 1)
	{
		for (int i = 0; i < argc; i++)
		{
			if (strcmp(argv[i], "--no-console") == 0)
			{
				ShowWindow(GetConsoleWindow(), 0);
				break;
			}
		}
	}

	// figure the current wallpaper by finding the nearest one to current time
	SYSTEMTIME st;
	GetLocalTime(&st);
	
	const int currTimeMinutes = st.wHour*60 + st.wMinute;
	std::function<size_t(size_t, size_t, size_t)> devideNconquer = [&](size_t currIdx, size_t lowBound, size_t highBound) -> size_t
	{
		if (currIdx == lowBound || currIdx == highBound) return currIdx;

		if (wallpapers[currIdx].second > currTimeMinutes)
			return devideNconquer((currIdx+lowBound)/2, lowBound, currIdx);
		
		else if (wallpapers[currIdx].second < currTimeMinutes)
			return devideNconquer((currIdx+highBound)/2, currIdx, highBound);
		
		else
			return currIdx;
	};
	currWallpaperidx = devideNconquer((wallpapers.size()-1) / 2, 0, wallpapers.size()-1);
	
	// the first wallpaper is determined based on the nearest wallpaper time, this means that sometimes
	// the first wallpaper is actually the next one, consider this example where we have 2 wallpaper one
	// at 01:00 the second at 02:00 and the current time is 01:45, in this case the wallpaper with 02:00 will
	// be used because it's closer to current time even though 01:00 is supposed to be used.
	// that's why we have this check:
	if (wallpapers[currWallpaperidx].second > currTimeMinutes)
	{
		currWallpaperidx--;
		if (currWallpaperidx == -1) currWallpaperidx = wallpapers.size()-1;
	}

	// decrement to account for first line of timerProc
	currWallpaperidx--;

// NOTE: since this program doesn't have a window, the id we pass is ignored, instead a different id is returned
//       we can store that and use it to override timer length in the next call to SetTimer
#ifdef NDEBUG
	timerID = SetTimer(NULL, 0, 0, &timerProc);
#else
	timerID = SetTimer(NULL, 0, debugTime, &timerProc);
#endif

	MSG message;
	while (isRunning)
	{
		GetMessage(&message, NULL, 0, 0);
		DispatchMessage(&message);
	}

	KillTimer(NULL, timerID);
}