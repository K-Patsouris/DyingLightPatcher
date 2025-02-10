#pragma once
#include "Common.h"

#include <filesystem>

using namespace std::filesystem;

class FileManager {
public:

	bool SetDiffPath(const string& str) noexcept;
	bool SetTargetPath(const string& str) noexcept;

	bool ParseWithoutCommit() noexcept;
	bool Commit() noexcept;
	bool ParseAndCommit() noexcept;

	void ToFiles() noexcept;

	void Reset() noexcept;

private:
	vector<path> diffs{};
	vector<path> targets{};
	vector<std::pair<string, string>> parsed{};

	vector<path>& GetPathVec(bool diff) noexcept;
	bool SetPath(const string& str, bool diff) noexcept;

	bool JustParse() noexcept;
	

};

