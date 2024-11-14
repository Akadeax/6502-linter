#include "Util.h"

#include <algorithm> 
#include <cctype>
#include <fstream>
#include <locale>
#include <sstream>

// trim from start (in place)
inline void LeftTrim(std::string& s)
{
    s.erase(s.begin(), std::ranges::find_if(s, [](unsigned char ch) {
        return !std::isspace(ch);
    }));
}

// trim from end (in place)
inline void RightTrim(std::string& s)
{
    s.erase(std::find_if(s.rbegin(), s.rend(), [](unsigned char ch) {
        return !std::isspace(ch);
    }).base(), s.end());
}

inline void Trim(std::string& s)
{
    LeftTrim(s);
    RightTrim(s);
}

std::vector<std::string> SplitIntoLines(const std::string& string)
{
	std::vector<std::string> lines{};
	std::stringstream ss{ string };

	std::string item;

	while (std::getline(ss, item, '\n'))
	{

        // Remove comments if not relevant to linter
        if (!item.ends_with(";LINTEXCLUDE"))
        {
			const size_t semicolonPos{ item.find_first_of(';') };
			item = item.substr(0, semicolonPos);
        }

		Trim(item);

		if (item.empty()) continue;

		lines.push_back(item);
	}

	return lines;
}

std::vector<std::string> Split(const std::string& string, char delim)
{
    std::vector<std::string> result;
    std::stringstream ss(string);
    std::string item;

    while (getline(ss, item, delim))
    {
        result.push_back(item);
    }

    return result;
}

std::vector<std::string> GetFileAsLineVector(const std::string& filePath)
{
    const std::ifstream fstream{ filePath };


    if (!fstream.is_open())
    {
        return std::vector<std::string>{};
    }

    std::stringstream buffer;
    buffer << fstream.rdbuf();

    std::vector<std::string> split{ SplitIntoLines(buffer.str()) };
    return split;
}