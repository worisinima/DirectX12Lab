
#include "StringHelper.h"

bool StringHelper::IsContain(const std::string& stringBody, const char& delim)
{
	bool Ret = false;

	std::string::size_type idx = stringBody.find(delim);
	if (idx != std::string::npos)
		Ret = true;
	else
		Ret = false;

	return Ret;
}

bool StringHelper::IsContain(const std::string& stringBody, const std::string& goleStringBody)
{
	bool Ret = false;

	if(stringBody.find(goleStringBody) != std::string::npos)
		Ret = true;

	return Ret;
}

bool StringHelper::IsContain(const std::vector<std::string>& stringContainer, const std::string& goleStringBody)
{
	for (const std::string& data : stringContainer)
	{
		if(data == goleStringBody)
			return true;
	}
	return false;
}

void StringHelper::Split(
	const std::string& s,
	std::vector<std::string>& outSplitStrings,
	const char& delim
)
{
	std::istringstream iss(s);
	std::string temp;

	temp.clear();
	while (std::getline(iss, temp, delim))
	{
		outSplitStrings.emplace_back(std::move(temp));
	}

	return;
}

void StringHelper::Split(const std::string& s, std::vector<std::string>& OutStrings, const std::string& delim)
{
	std::string::size_type pos1, pos2;
	size_t len = s.length();
	pos2 = s.find(delim);
	pos1 = 0;

	while (std::string::npos != pos2)
	{
		OutStrings.emplace_back(s.substr(pos1, pos2 - pos1));
		pos1 = pos2 + delim.size();
		pos2 = s.find(delim, pos1);
	}

	if (pos1 != len)
		OutStrings.emplace_back(s.substr(pos1));
}

std::string StringHelper::Remove(std::string& s, const std::string& removeString)
{
	std::string::size_type n = removeString.length();
	for (std::string::size_type i = s.find(removeString); i != std::string::npos; i = s.find(removeString))
		s.erase(i, n);

	return s;
}
