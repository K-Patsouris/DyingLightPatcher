#include "Utils.h"
#include "logger.h"
#include <algorithm>


namespace StringUtils {

	//Char checks
	[[nodiscard]] bool IsWordChar(const char c) noexcept { return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c == '_'); }
	[[nodiscard]] bool IsNumberChar(const char c) noexcept { return c >= '0' && c <= '9'; }
	[[nodiscard]] bool IsWhitespace(const char c) noexcept { return (c == ' ') || (c == '\t'); } //\v=vertical tab	\f=formfeed(aka word pagebreak)
	[[nodiscard]] bool IsNewlineChar(const char c) noexcept { return c == '\r' || c == '\n'; }
	[[nodiscard]] bool IsMathOpChar(const char c) noexcept { return c == '-' || c == '+'; } //Add '/' and '*' if necessary
	[[nodiscard]] bool IsIdentifierChar(const char c) noexcept { return IsWordChar(c) || IsNumberChar(c); }
	[[nodiscard]] bool IsSpaceIdentifierChar(const char c) noexcept { return c == ' ' || IsWordChar(c) || IsNumberChar(c); }
	[[nodiscard]] bool IsSpaceNewline(const char c) noexcept { return c == ' ' || IsNewlineChar(c); }


	//Misc lambda-like helpers
	[[nodiscard]] bool allWordChar(const string& str) noexcept {
		for (const char c : str) if (!IsWordChar(c)) return false;
		return true;
	}
	[[nodiscard]] bool allNumberChar(const string& str) noexcept {
		for (const char c : str) if (!IsNumberChar(c)) return false;
		return true;
	}
	[[nodiscard]] bool allIdentifierChar(const string& str) noexcept {
		for (const char c : str) if (!IsIdentifierChar(c)) return false;
		return true;
	}
	[[nodiscard]] bool allSpaceIdentifierChar(const string& str) noexcept {
		for (const char c : str) if (!IsSpaceIdentifierChar(c)) return false;
		return true;
	}
	[[nodiscard]] bool allNotNewline(const string& str) noexcept {
		for (const char c : str) if (c == '\n') return false;
		return true;
	}


	//Utils
	[[nodiscard]] vector<string> Split (const string& str, char delim, void(*formatter)(string&), bool(*validator)(const string&)) {
		if (str.length() <= 0u)
			return vector<string>{};
		if (delim == '\0')
			return vector<string>{ str };

		vector<string> result{};
		uint64 pos = 0u;
		while (true) {
			string line{ str.substr(pos, str.find(delim, pos) - pos)};
			szt linelength = line.length();
			formatter(line);
			if (validator(line)) {
				result.push_back(line);
			}
			pos += linelength + 1u;
			if (pos >= str.size())
				break;
		}

		return result;
	}
	[[nodiscard]] string Join(const vector<string>& vec, char delim) {
		string result{};
		for (const auto& str : vec)
			result += str + delim;
		if (!result.empty())
			result.pop_back();
		return result;
	}
	[[nodiscard]] string Join(const vector<string>& vec1, const vector<string>& vec2, char delim) {
		string result{};
		if (vec1.size() != vec2.size())
			return result;
		for (szt i{ 0 }; i < vec1.size(); ++i) {
			result += vec1[i] + vec2[i] + delim;
		}
		if (!result.empty())
			result.pop_back();
		return result;
	}

	
	//Lambda-like helpers to feed SkipChars		Not publicly available
	bool helperCheckSpace(const char c, szt& linespassed) noexcept { linespassed += (IsNewlineChar(c)); return (c == ' '); }
	bool helperCheckWhitespace(const char c, szt& linespassed) noexcept { linespassed += (IsNewlineChar(c)); return IsWhitespace(c); }
	bool helperCheckNumber(const char c, szt& linespassed) noexcept { linespassed += (IsNewlineChar(c)); return IsNumberChar(c); }
	bool helperCheckNewline(const char c, szt& linespassed) noexcept { linespassed += (IsNewlineChar(c)); return IsNewlineChar(c); }
	bool helperCheckWhitespaceNewline(const char c, szt& linespassed) noexcept { linespassed += (IsNewlineChar(c)); return (IsWhitespace(c) || IsNewlineChar(c)); }
	bool helperCheckSpaceNewline(const char c, szt& linespassed) noexcept { linespassed += (IsNewlineChar(c)); return IsSpaceNewline(c); }
	bool helperCheckIdentifier(const char c, szt& linespassed) noexcept { linespassed += (IsNewlineChar(c)); return IsIdentifierChar(c); }

	[[nodiscard]] bool StrICmp(const char* c1, const char* c2) noexcept {
		while (*c1 && std::tolower(*c1) == std::tolower(*c2)) {
			++c1;
			++c2;
		}
		return (*c1 - *c2) == 0;
	}
	[[nodiscard]] bool SkipChars(const string& str, traversal_state& ts, bool(*func)(const char, szt&)) noexcept {
		if (ts.index >= str.npos || ts.index + 1u >= str.length())
			return false;

		szt startpos{ ts.index };
		szt linespassed{ ts.line };
		while (++startpos < str.length()) {
			if (!func(str[startpos], linespassed))
				break;
		}
		if (startpos >= str.length()) //str ends in a must-skip character
			return false;
		ts.index = startpos;
		ts.line = linespassed;
		return true;
	}
	[[nodiscard]] bool SkipSpace(const string& str, traversal_state& ts) noexcept { return SkipChars(str, ts, helperCheckSpace); }
	[[nodiscard]] bool SkipWhitespace(const string& str, traversal_state& ts) noexcept { return SkipChars(str, ts, helperCheckWhitespace); }
	[[nodiscard]] bool SkipNewline(const string& str, traversal_state& ts) noexcept { return SkipChars(str, ts, helperCheckNewline); }
	[[nodiscard]] bool SkipWhitespaceNewline(const string& str, traversal_state& ts) noexcept { return SkipChars(str, ts, helperCheckWhitespaceNewline); }
	[[nodiscard]] bool SkipSpaceNewline(const string& str, traversal_state& ts) noexcept { return SkipChars(str, ts, helperCheckSpaceNewline); }
	[[nodiscard]] bool SkipIdentifier(const string& str, traversal_state& ts) noexcept { return SkipChars(str, ts, helperCheckIdentifier); }
	[[nodiscard]] bool SkipNumber(const string& str, traversal_state& ts) noexcept { return SkipChars(str, ts, helperCheckNumber); }
	void RemoveLeadingWhitespace(string& str) noexcept {
		szt pos{ 0u };
		while (pos < str.length() && IsWhitespace(str[pos])) { ++pos; }
		str = str.substr(pos);
	}
	void RemoveTrailingWhitespace(string& str) noexcept {
		szt pos{ str.length() }, count{ pos };
		while (pos-- > 0u && IsWhitespace(str[pos])) { --count; }
		str = str.substr(0u, count);
	}
	void RemoveLeadingAndTrailingWhitespace(string& str) noexcept { RemoveLeadingWhitespace(str); RemoveTrailingWhitespace(str); }
	bool RemoveWhitespace(string& str) noexcept { try { std::erase_if(str, IsWhitespace); return true; } catch (...) { return false; } }
	bool RemoveSpace(string& str) noexcept { try { std::erase_if(str, [](const char c) { return c == ' '; }); return true; } catch (...) { return false; } }
	void RemoveComments(string& str) noexcept {
		//Block
		szt startpos{ 0u };
		while (true) {
			startpos = str.find("/*", startpos);
			if (startpos >= str.length()) {
				break;
			}
			szt endpos{ str.find("*/", startpos + 2) }; //Start from after '*'. find() won't throw, just return npos if offset bad.
			if (endpos >= str.length()) {
				str = str.substr(0u, startpos);
				break;
			}
			str = str.substr(0u, startpos) + str.substr(endpos + 2); //endpos + 2 can at most be str.length(), aka the null terminator, and substr works with it.
		}
		//Line
		startpos = 0;
		while (true) {
			startpos = str.find("//", startpos);
			if (startpos >= str.length())
				return;
			szt endpos{ str.find('\n', startpos) };
			if (endpos >= str.length()) {
				str = str.substr(0u, startpos);
				return;
			}
			str = str.substr(0u, startpos) + str.substr(endpos);
		}
	}
	bool TabToSpace(string& str) noexcept {
		try { std::replace(str.begin(), str.end(), '\t', ' '); return true; }
		catch (...) { return false; }
	}
}
