#pragma once

#include <string>
#include <vector>
#include <iostream>
#include <sstream>

class StringHelper
{
public:

	StringHelper(){}

	//Is contain char
	bool IsContain(const std::string& stringBody, const char& delim);
	bool IsContain(const std::string& stringBody, const std::string& goleStringBody);
	bool IsContain(const std::vector<std::string>& stringContainer, const std::string& goleStringBody);

	//Split
	void Split(const std::string& s, std::vector<std::string>& outSplitStrings, const char& delim);
	void Split(const std::string& s, std::vector<std::string>& OutStrings, const std::string& delim);
	
	std::string Remove(std::string& s, const std::string& removeString);
};