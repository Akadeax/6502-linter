#include <iostream>
#include <string>
#include <vector>
#include <filesystem>
#include <functional>
#include <map>
#include <ranges>
#include <regex>

#include "Util.h"

using std::string, std::vector, std::cout;

struct FunctionData
{
	size_t startLine;
	string name;
	vector<string> instructions;
	vector<string> functionCalls;
	int highestTempUsed;
};

bool GatherInstructionsForFunction(const vector<string>& fullFile, FunctionData& data);
bool GatherHighestTempUsedForFunction(FunctionData& data, const std::string& tempFormatWithoutX);

int main(int argc, char* argv[])
{
	if (argc < 3)
	{
		cout << "Parameter syntax: <MainFilePath> <TempFormat ending in x, ex.: zp_temp_x>\n";
		return 1;
	}

	//cout << "Running on main file " << argv[1] << "...\n";

	// Resolve main file, convert into lines
	vector<string> mainAsLines{ GetFileAsLineVector(argv[1]) };

	if (mainAsLines.empty())
	{
		cout << "Couldn't read " << argv[1] << "!\n";
		return 1;
	}

	string mainSourceFolderPath{ std::filesystem::path(argv[1]).parent_path().string() };
	
	// Resolve TempFormat
	std::string tempFormat{ argv[2] };
	if (!tempFormat.ends_with("x"))
	{
		cout << "TempFormat does not end with x\n";
		return 1;
	}
	std::string tempFormatWithoutX{ tempFormat.substr(0, tempFormat.size() - 1) };


	// Step 1: Resolve all .includes
	for (size_t i{}; i < mainAsLines.size(); ++i)
	{
		if (!mainAsLines[i].starts_with(".include")) continue;

		vector<string> split{ Split(mainAsLines[i], ' ') };
		mainAsLines.erase(std::begin(mainAsLines) + static_cast<long long>(i));

		string includeFilePath{ split[1] };
		// Remove quotations around filename
		includeFilePath = includeFilePath.substr(1, includeFilePath.size() - 2);
		// If exe was called from different folder than maiFn is in, resolve folder path
		if (mainSourceFolderPath != "")
		{
			includeFilePath = std::format("{}/{}", mainSourceFolderPath, includeFilePath);
		}

		//cout << "resolving include " << includeFilePath << "...\n";

		vector<string> includeAsLines{ GetFileAsLineVector(includeFilePath) };

		if (includeAsLines.empty())
		{
			cout << "Couldn't read " << includeFilePath << "!\n";
			return 1;
		}

		mainAsLines.insert(
			std::begin(mainAsLines) + static_cast<long long>(i),
			std::begin(includeAsLines),
			std::end(includeAsLines)
		);
	}

	// Now all includes are resolved, and we basically have the entire 6502 project in one string array of "instructions"
	// Step 2: identify all procedures and macros and make a map entry for them

	std::map<string, FunctionData> functionMap{};

	for (size_t i{}; i < mainAsLines.size(); ++i)
	{
		const string& line{ mainAsLines[i] };

		if (line.starts_with(".proc") || line.starts_with(".macro"))
		{
			if (line.find("LINTEXCLUDE") != string::npos) continue;

			vector lines{ Split(line, ' ') };
			string functionName{ lines[1] };

			FunctionData data{ FunctionData{ i, functionName, {}, {}, -1 } };
			bool success{ GatherInstructionsForFunction(mainAsLines, data) };


			if (!success)
			{
				cout << "Failed to resolve instructions for " << functionName << "!\n";
				return 1;
			}

			functionMap[functionName] = data;
		}
	}

	// Now we have a list of all instructions in each procedure/macro
	// Step 3: find the highest temp used by every function

	bool anyLintErrors = false;

	for (std::pair<string, FunctionData> entry : functionMap)
	{
		FunctionData& dataRef{ functionMap[entry.first] };
		
		bool success{ GatherHighestTempUsedForFunction(dataRef, tempFormatWithoutX) };

		if (!success)
		{
			cout << "Failed to resolve highest temp value for " << entry.first << "!\n";
			return 1;
		}

		int highestTemp{ dataRef.highestTempUsed };
		std::string suggestedSuffix{ std::format("_T{}", highestTemp) };

		if (highestTemp >= 0 && !entry.first.ends_with(suggestedSuffix))
		{
			anyLintErrors = true;
			std::cout << std::format(
		"[LINT]: function {} uses temps up to {}{}. Either change suffix to {} or use ';LINTEXCLUDE'.\n",
				entry.first, tempFormatWithoutX, highestTemp, suggestedSuffix
			);
		}
		else if (highestTemp == -1 && std::regex_search(entry.first, std::regex{ "_T[0-9]+" }))
		{
			anyLintErrors = true;
			std::cout << std::format(
				"[LINT]: function {} has a suffix implying temporaries while not using any.\n",
				entry.first
			);
		}
	}

	// Step 4: Find out if any function calls occur that use more temps than a function is marked for
	// e.g. if a function _T2 calls a function that is _T4, the first function should be a _T4 instead
	for (std::pair<string, FunctionData> entry : functionMap)
	{
		if (entry.second.highestTempUsed == -1) continue;
		
		std::vector<FunctionData> calls{};
		
		for (const std::string& instruction : entry.second.instructions)
		{
			std::optional<std::pair<string, FunctionData>> functionCall{ std::nullopt };
			for (std::pair<string, FunctionData> innerFunc : functionMap)
			{
				if (instruction.find(innerFunc.first) != string::npos)
				{
					functionCall = innerFunc;
					break;
				}
			}

			if (!functionCall.has_value()) continue;

			if (functionCall->second.highestTempUsed != -1)
			{
				calls.emplace_back(functionCall->second);
			}
		}

		if (calls.empty()) continue;
		
		int ownHighest{ entry.second.highestTempUsed };
		auto lambda{ [ownHighest](const FunctionData& data)
		{
			return data.highestTempUsed <= ownHighest;
		} };
		std::erase_if(calls, lambda);

		if (calls.empty()) continue;
		
		std::stringstream listStream{};
		for(size_t i{}; i < calls.size(); ++i)
		{
			listStream << calls[i].name;
			
			if (i != calls.size() - 1)
			{
				listStream << ", ";
			}
		}

		int max{ -1 };
		for (const FunctionData& data : calls)
		{
			if (data.highestTempUsed >= max) max = data.highestTempUsed; 
		}
		
		std::cout << std::format(
			"[LINT] function {} calls {}. Either change suffix to _T{} or use ';LINTEXCLUDE'.\n",
			entry.first, listStream.str(), max
		);

		anyLintErrors = true;
	}

	if (anyLintErrors) return 1;
}



bool GatherInstructionsForFunction(const vector<string>& fullFile, FunctionData& data)
{
	//cout << "Gathering instructions for " << data.name << "...\n";

	// either .macro or .proc
	const string startingInstruction{ Split(fullFile[data.startLine], ' ')[0] };

	// depth refers to how deeply nested in functions we are, only count instructions for this specific function
	int depth{ 0 };

	for (size_t i{ data.startLine + 1 }; i < fullFile.size(); ++i)
	{
		const string& line{ fullFile[i] };

		if (line.starts_with(".proc") || line.starts_with(".macro"))
		{
			//std::cout << "Increasing depth!\n";
			++depth;
		}
		else if (line.starts_with(".endproc") || line.starts_with(".endmacro"))
		{
			//std::cout << "Decreasing depth!\n";
			--depth;

			// this proc/macro has ended, break
			if (depth == -1)
			{
				// invalid scope end check
				if (
					(line.starts_with(".endproc") &&  startingInstruction != ".proc") ||
					(line.starts_with(".endmacro") && startingInstruction != ".macro")
				)
				{
					cout << "A function (macro/procedure) scope has ended unexpectedly: " << line << '\n';
					data.instructions.clear();
					return false;
				}

				//cout << "Stopping recording for " << data.name << '\n';
				break;
			}
		}
		// depth 0 means we're at the depth of the current function we want to record
		// any other depth means we shouldn't be recording it for this specific function
		else if (depth == 0)
		{
			data.instructions.push_back(line);
			//cout << "Recording instruction: " << line << "\"\n";
		}
	}

	return true;
}


bool GatherHighestTempUsedForFunction(FunctionData& data, const std::string& tempFormatWithoutX)
{
	// Check all instructions for use of temp format
	for (const std::string& instruction : data.instructions)
	{
		std::vector positions{ FindAllSubstrInString(instruction, tempFormatWithoutX) };

		for (size_t pos : positions)
		{
			size_t tempIndexStart{ pos + tempFormatWithoutX.size() };

			int val;

			int firstChar{ instruction[tempIndexStart] - '0' }; // converts ASCII to digit
			if (firstChar < 0 || firstChar > 9) return false;

			// should we use format_xx or format_x (1 or 2 digit temp number)?
			int secondChar{ instruction[tempIndexStart + 1] - '0' }; // converts ASCII to digit
			// is secondChar an invalid digit?
			if (secondChar < 0 || secondChar > 9)
			{
				val = firstChar;
			}
			else
			{
				val = firstChar * 10 + secondChar;
			}

			if (val > data.highestTempUsed)
			{
				data.highestTempUsed = val;
			}
		}
	}
	
	//cout << data.name << "'s highest temp used is " << data.highestTempUsed << '\n';
	return true;
}