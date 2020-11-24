#pragma once

#include <string>
#include <iostream>
#include "Math/FMathLib.h"
#include "../String/StringHelper.h"

extern std::string gSystemPath;
extern unsigned int gAppWindowWidth;
extern unsigned int gAppWindowHeight;

#define LOG(m) DebugLog(m);
#define LOG_Warning(m) WarningLog(m);
#define LOG_Error(m) ErrorLog(m);
void DebugLog(std::string message);
void WarningLog(std::string message);
void ErrorLog(std::string message);

extern StringHelper gSHepler;

