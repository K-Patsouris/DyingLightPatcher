#pragma once
#include "Common.h"



namespace StringUtils {
	//string traversal state aggregate
	struct traversal_state final {
	public:
		szt index{ 0 };			//Character index of the string we're currently at
		szt line{ 0 };			//The line of the string we're currently on, aka total number of '\n' passed
	};

	//Char checks
	[[nodiscard]] bool IsWordChar(const char c) noexcept;
	[[nodiscard]] bool IsNumberChar(const char c) noexcept;
	[[nodiscard]] bool IsNewlineChar(const char c) noexcept;
	[[nodiscard]] bool IsMathOpChar(const char c) noexcept;
	[[nodiscard]] bool IsIdentifierChar(const char c) noexcept;
	[[nodiscard]] bool IsSpaceIdentifierChar(const char c) noexcept;
	[[nodiscard]] bool IsSpaceNewline(const char c) noexcept;
	[[nodiscard]] bool IsWhitespace(const char c) noexcept;

	//Misc lambda-like helpers
	[[nodiscard]] bool allWordChar(const string& str) noexcept;
	[[nodiscard]] bool allNumberChar(const string& str) noexcept;
	[[nodiscard]] bool allIdentifierChar(const string& str) noexcept;
	[[nodiscard]] bool allSpaceIdentifierChar(const string& str) noexcept;
	[[nodiscard]] bool allNotNewline(const string& str) noexcept;


	//Utils
	[[nodiscard]] vector<string> Split(const string& str, char delim, void(*formatter)(string&) = [](string&) {}, bool(*validator)(const string&) = [](const string&) { return true; });
	[[nodiscard]] string Join(const vector<string>& vec, char delim);
	[[nodiscard]] string Join(const vector<string>& vec1, const vector<string>& vec2, char delim);

	[[nodiscard]] bool StrICmp(const char* c1, const char* c2) noexcept;
	[[nodiscard]] bool SkipChars(const string& str, traversal_state& ts, bool(*func)(const char, szt&)) noexcept;
	[[nodiscard]] bool SkipSpace(const string& str, traversal_state& ts) noexcept;
	[[nodiscard]] bool SkipWhitespace(const string& str, traversal_state& ts) noexcept;
	[[nodiscard]] bool SkipNewline(const string& str, traversal_state& ts) noexcept;
	[[nodiscard]] bool SkipWhitespaceNewline(const string& str, traversal_state& ts) noexcept;
	[[nodiscard]] bool SkipSpaceNewline(const string& str, traversal_state& ts) noexcept;
	[[nodiscard]] bool SkipIdentifier(const string& str, traversal_state& ts) noexcept;
	[[nodiscard]] bool SkipNumber(const string& str, traversal_state& ts) noexcept;
	void RemoveLeadingWhitespace(string& str) noexcept;
	void RemoveTrailingWhitespace(string& str) noexcept;
	void RemoveLeadingAndTrailingWhitespace(string& str) noexcept;
	bool RemoveWhitespace(string& str) noexcept;
	bool RemoveSpace(string& str) noexcept;
	void RemoveComments(string& str) noexcept;
	bool TabToSpace(string& str) noexcept;
}

namespace MiscUtils {

	template<typename T>
	[[nodiscard]] bool PushBackNoEx(vector<T>& vec, const T& elem) noexcept {
		try { vec.push_back(elem); return true; }
		catch (...) { return false; }
	}
	template<typename T>
	[[nodiscard]] bool PushBackNoEx(vector<T>& vec, T&& elem) noexcept {
		try { vec.push_back(std::forward<T>(elem)); return true; }
		catch (...) { return false; }
	}

}


