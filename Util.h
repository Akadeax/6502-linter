#pragma once
#include <string>
#include <vector>
inline void LeftTrim(std::string& s);
inline void RightTrim(std::string& s);
inline void Trim(std::string& s);

std::vector<std::string> SplitIntoLines(const std::string& string);
std::vector<std::string> Split(const std::string& string, char delim);

std::vector<std::string> GetFileAsLineVector(const std::string& filePath);

