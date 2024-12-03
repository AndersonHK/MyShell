#pragma once
#include "Globals.h"

class CommandsShell
{
public:
	static void fileRedirect(size_t index, const std::vector<std::string>& args);
	static void echo(size_t index, const std::vector<std::string>& args);
	static void ls(size_t index, const std::vector<std::string>& args);
	static void wc(size_t index, const std::vector<std::string>& args);
	static void cat(size_t index, const std::vector<std::string>& args);
	static void grep(size_t index, const std::vector<std::string>& args);
};