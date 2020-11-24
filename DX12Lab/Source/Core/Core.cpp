
#include "Core.h"
#include "windows.h"

//在Main函数一开始就初始化它
std::string gSystemPath = "";
unsigned int gAppWindowWidth = 0;
unsigned int gAppWindowHeight = 0;
StringHelper gSHepler;

void DebugLog(std::string message)
{
#if defined(DEBUG) | defined(_DEBUG)
	std::cout << message << std::endl;
#endif
}

void WarningLog(std::string message)
{
#if defined(DEBUG) | defined(_DEBUG)
	SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), FOREGROUND_INTENSITY | FOREGROUND_GREEN | FOREGROUND_RED);
	std::cout << message << std::endl;
	SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), FOREGROUND_INTENSITY | FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
#endif
}

void ErrorLog(std::string message)
{
#if defined(DEBUG) | defined(_DEBUG)
	SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), FOREGROUND_INTENSITY | FOREGROUND_RED);
	std::cout << message << std::endl;
	SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), FOREGROUND_INTENSITY | FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
#endif
}