#include "StringParser.h"

#include <algorithm>

namespace StringParser {

	using namespace StringUtils;
	using namespace MiscUtils;

	using Cache = Parser::Cache;

	static constexpr const char* NOOP_TAG{ "noop" };
	static constexpr const char* INSERT_TAG{ "insert" };
	static constexpr const char* RENAME_TAG{ "rename" };
	static constexpr const char* REDEFINE_TAG{ "redefine" };
	static constexpr const char* DELETE_TAG{ "delete" };

	
	//Node
	using Node = Parser::Node;
	using Flag = Node::Flag;
	using NodeFlags = Node::NodeFlags;
	using NodeVector = Node::NodeVector;
	using NodeIterator = Node::NodeIterator;
	using NodeCIterator = Node::NodeCIterator;

	namespace helpers {
		[[nodiscard]] bool ValidateBraces(const string& str) noexcept {
			szt line{ 1u };
			szt pos{ 0u };
			int64 opens{ 0 };

			while (pos < str.length()) {
				if (str[pos] == '\n')
					++line;
				else if (str[pos] == '{')
					++opens;
				else if (str[pos] == '}')
					--opens;
				if (opens < 0) {
					logger.Error("No matching opening <{> for closing <}> in line {}"sv, line);
					return false;
				}

				++pos;
			}

			if (opens > 0) {
				logger.Error("Expected closing <}> in line {}"sv, line);
				return false;
			}

			return true;
		}
		[[nodiscard]] bool ValidateParens(const string& str) noexcept {
			szt line{ 1u };
			szt pos{ 0u };
			bool open{ false };

			while (pos < str.length()) {
				if (str[pos] == '\n')
					++line;
				else if (str[pos] == '(') {
					if (open) {
						logger.Error("Invalid opening <(> in line {}"sv, line);
						return false;
					}
					open = true;
				}
				else if (str[pos] == ')') {
					if (!open) {
						logger.Error("Invalid closing <)> in line {}"sv, line);
						return false;
					}
					open = false;
				}
				++pos;
			}

			if (open) {
				logger.Error("Expected closing <)> in line {}"sv, line);
				return false;
			}

			return true;
		}
		//Scr utils
		//Return true if successfully read the thing starting from and including the current char. Leaves ts.index to point to last char of thing. No change if failure.
		[[nodiscard]] bool ReadIdentifier(const string& str, traversal_state& ts) noexcept {
			if (ts.index >= str.length() || !IsWordChar(str[ts.index])) {
				return false;
			}

			while (++ts.index < str.length() && IsIdentifierChar(str[ts.index])) {}

			--ts.index;
			return true;
		}
		//Expects ts.index pointing to the first digit and leaves ts.index pointing to the last digit
		[[nodiscard]] bool ReadInt(const string& str, traversal_state& ts) noexcept {
			if (ts.index >= str.length() || (!IsNumberChar(str[ts.index]) && str[ts.index] != '-')) {
				return false;
			}

			while (++ts.index < str.length() && IsNumberChar(str[ts.index])) {}

			--ts.index;
			return true;
		}
		//Expects ts.index pointing to the first digit of the integral part, and leaves ts.index pointing to the last digit of the fractinal part
		[[nodiscard]] bool ReadFloat(const string& str, traversal_state& ts) noexcept {
			if (ts.index >= str.length() || (!IsNumberChar(str[ts.index]) && str[ts.index] != '-')) { //Integral part
				return false;
			}

			szt backup{ ts.index };
			//	Skip integral part		Decimal separator							Fractional part
			if (!SkipNumber(str, ts) || str[ts.index] != '.' || ++ts.index >= str.length() || !IsNumberChar(str[ts.index])) {
				ts.index = backup;
				return false;
			}
			//Skip Fractional Part		allow for str to end with it
			while (++ts.index < str.length()) {
				if (!IsNumberChar(str[ts.index])) {
					break;
				}
			}

			--ts.index;
			return true;
		}
		//Expects ts.index pointing to the opening '"', and leaves ts.index pointing to the closing '"'
		[[nodiscard]] bool ReadString(const string& str, traversal_state& ts) noexcept {
			if (ts.index >= str.length() || str[ts.index] != '"') {
				return false;
			}

			szt close{ str.find('"', ts.index + 1) }; //Closing '"'
			if (close >= str.length()) {
				return false;
			}

			for (szt i{ ts.index + 1 }; i < close; ++i) {
				if (str[i] == '\n') { //Don't allow multi-line strings
					return false;
				}
			}

			ts.index = close;
			return true;
		}
		[[nodiscard]] bool FindScrSub(const string& str, traversal_state& ts, string& subsig) {
			//ts.index = 0u;
			subsig.clear();
			
		//logger.Info("0, l:{} i:{} s:<{}>"sv, str.length(), ts.index, str.substr(ts.index, 4));
			if (str.length() <= ts.index || str.substr(ts.index, 4u) != "sub ")
				return false; //First 4 chars not spelling "sub "
		//logger.Info("1");
			ts.index += 3u; //"sub "'s ' ' index
			if (!SkipSpace(str, ts) || !IsWordChar(str[ts.index]))
				return false; //str only contains whitespace after "sub " or first char after it is not _a-zA-Z
			szt aux{ ts.index };
			--ts.index;
		//logger.Info("2");
			if (!SkipIdentifier(str, ts))
				return false;
		//logger.Info("3");
			subsig = "sub " + str.substr(aux, ts.index - aux);
			if ((str[ts.index] == ' ' && !SkipSpace(str, ts)))
				return false;
		//logger.Info("4");
			if (str[ts.index] != '(')
				return false; //No '(' after "sub X"
		//logger.Info("5");
			if (!SkipSpace(str, ts))
				return false; //str only contains whitespace after "sub X("
		//logger.Info("6");
			if (str[ts.index] != ')')
				return false; //No ')' after "sub X("
		//logger.Info("7");

			return true;
		}
		[[nodiscard]] bool IsValidFloatArg(const string& arg) noexcept {
			bool dotfound{ false };
			for (const char c : arg) {
				if (!IsNumberChar(c)) {
					if (!dotfound && c == '.') {
						dotfound = true;
					}
					else {
						return false;
					}
				}
			}
			return dotfound && arg.front() != '.' && arg.back() != '.'; //Implicit not-empty check
		}
		[[nodiscard]] bool IsValidIntArg(const string& arg) noexcept {
			for (const char c : arg) {
				if (!IsNumberChar(c)) {
					return false;
				}
			}
			return !arg.empty();
		}
		//Expects str to end in ')'
		[[nodiscard]] bool FormatAndValidateFuncSignature(string& str) noexcept {
			szt openpos{ str.find('(') };
			if (openpos >= str.length()) {
				logger.Error("Function signature missing arguments: <{}>", str);
				return false;
			}

			try {
				string sig = str.substr(0u, openpos); //Not including the '('
				RemoveLeadingAndTrailingWhitespace(sig);
				if (sig.length() <= 0 || !IsWordChar(sig[0]) || !allIdentifierChar(sig)) {
					logger.Error("Invalid function signature: <{}> of <{}>", sig, str);
					return false;
				}

				sig += '(';
				
				const string argsS{
					[&] { string temp = str.substr(openpos + 1u, str.length() - openpos - 2u); //Last char of str is expected to be ')'
					RemoveLeadingAndTrailingWhitespace(temp);
					return temp; }()
				};
				//szt argstartpos{ 0 };
				traversal_state ts{ .index = 0 };
				while (ts.index < argsS.length()) {

					szt argendpos{ 0 };
					//String
					if (argsS[ts.index] == '"') {
						argendpos = argsS.find('"', ts.index + 1);
						if (argendpos >= argsS.length()) {
							logger.Error("String arguments must be enclosed in <\"> (in function <{}>)"sv, str);
							return false;
						}
						string arg{ argsS.substr(ts.index, argendpos - ts.index + 1) };
						if (!allNotNewline(arg.substr(1, arg.length() - 2))) {
							logger.Error("String arguments must not change line (argument <{}> in function <{}>)"sv, arg, str);
							return false;
						}
						sig += arg + ',';
						++argendpos;
					}

					//Anything operable (vars, floats, and ints)
					else if (IsNumberChar(argsS[ts.index]) || argsS[ts.index] == '-' || IsWordChar(argsS[ts.index])) {
						argendpos = argsS.find(',', ts.index + 1); //No need for bound-checks. argsS ends before ')' so it's ok if ',' is not found and we take the rest of it here.
						string arg{ argsS.substr(ts.index, argendpos - ts.index) };
						RemoveSpace(arg);
						traversal_state ts_arg{ .index = 0 };
						while (ReadIdentifier(arg, ts_arg) || ReadFloat(arg, ts_arg) || ReadInt(arg, ts_arg)) {
							if (ts_arg.index + 2 >= arg.length() || !IsMathOpChar(arg[ts_arg.index + 1])) {
								break;
							}
							else {
								ts_arg.index += 2;
							}
						}
						if (ts_arg.index != arg.length() - 1) { //Fully read
							logger.Error("Argument <{}> of function <{}> is incomplete"sv, arg, str);
							return false;
						}
						sig += arg + ',';
					}

					else if (argsS[ts.index] == '[') {
						argendpos = argsS.find(']', ts.index + 1);
						if (argendpos > argsS.length()) {
							logger.Error("Array arguments must be enclosed in <[]> (in function <{}>)"sv, str);
							return false;
						}

						vector<string> arrayargs{ Split(argsS.substr(ts.index + 1u, argendpos - ts.index - 1u), ',', [](string& piece) { RemoveLeadingAndTrailingWhitespace(piece); }) };
						if (arrayargs.empty()) {
							logger.Error("Empty array function arguments are not allowed (in function <{}>)"sv, str);
							return false;
						}
						sig += '[';

						bool floatArr{ IsValidFloatArg(arrayargs[0]) };
						for (const auto& arg : arrayargs) {
							if (floatArr) {
								if (!IsValidFloatArg(arg)) {
									logger.Error("Float arguments must be <.> separated with both an integral and a fractional part (array element <{}> in function <{}>)"sv, arg, str);
									return false;
								}
							}
							else {
								if (!IsValidIntArg(arg)) {
									logger.Error("Int arguments must only contain 0-9 (array element <{}> in function <{}>)"sv, arg, str);
									return false;
								}
							}
							sig += arg + ',';
						}
						if (sig.back() == ',')
							sig.pop_back();
						sig += ']';
						++argendpos;
					}
					else {
						logger.Error("Invalid function arguments (in function <{}>)"sv, str);
						return false;
					}

					//Find first char of next arg
					bool commaFound{ false };
					while (argendpos < argsS.length()) {
						if (argsS[argendpos] == ' ') {
							++argendpos;
						}
						else if (argsS[argendpos] == ',') {
							if (commaFound) {
								logger.Error("Empty arguments are not allowed (in function <{}>)"sv, str);
								return false;
							}
							commaFound = true;
							++argendpos;
						}
						else if (!commaFound) {
							logger.Error("Function arguments must be comma separated (in function <{}>)"sv, str);
							return false;
						}
						else {
							break;
						}
					}
					ts.index = argendpos; //Index of first non ' ', non ',' char after last arg
				}

				if (sig.back() == ',')
					sig.pop_back();
				sig += ')';
				str = std::move(sig);
				return true;
			}
			catch (...) {
				logger.Error("Unspecified exception trying to validate function signature <{}>", str);
				return false;
			}
		}
		//Expects str to start in "use " end in ')'
		[[nodiscard]] bool FormatAndValidateUseSignature(string& str) noexcept {
			szt openpos{ str.find('(') };
			if (openpos >= str.length()) {
				logger.Error("Use statements cannot forgo the empty <()>: <{}>", str);
				return false;
			}

			try {
				string sig = str.substr(4, openpos - 4); //Starting from the char after "use "'s ' ' and not including the '('
				RemoveLeadingAndTrailingWhitespace(sig);
				if (sig.empty() || !IsWordChar(sig[0]) || !allIdentifierChar(sig)) {
					logger.Error("Use statements must specify an identifier (in statement <{}>) read <{}>", str, sig);
					return false;
				}

				for (szt i{ openpos + 1 }; i < str.length() - 1; ++i) {
					if (str[i] != ' ') {
						logger.Error("Use statements cannot have arguments (in statement <{}>)", str);
						return false;
					}
				}

				str = std::move("use " + sig + "()");
				return true;
			}
			catch (...) {
				logger.Error("Unspecified exception trying to validate use statement signature <{}>", str);
				return false;
			}
		}
		//Expects str to end in ')'
		[[nodiscard]] bool FormatAndValidateSubDeclSignature(string& str) noexcept {
			szt openpos{ str.find('(') };
			if (openpos >= str.length()) {
				logger.Error("Function signature missing arguments: <{}>", str);
				return false;
			}

			try {
				string sig = str.substr(0u, openpos); //Not including the '('
				RemoveLeadingAndTrailingWhitespace(sig);
				if (sig.substr(0, 4) != "sub " || !IsWordChar(sig[4])) { //'(' exists so str.length() >= 5 so [4] is valid
					logger.Error("Invalid sub signature: <{}> of <{}>", sig, str);
					return false;
				}
				for (szt i{ 5 }; i < sig.length(); ++i) {
					if (!IsIdentifierChar(sig[i])) {
						logger.Error("Invalid sub signature identifier: <{}> of <{}>", sig, str);
						return false;
					}
				}

				sig += '(';
				
				auto formatter = [](string& s) { RemoveLeadingAndTrailingWhitespace(s); s += ','; }; //Add a trailing char to work with ReadX. Make it comma form convenience since we need one.
				vector<string> argsV = Split(str.substr(openpos + 1, str.length() - openpos - 2), ',', RemoveLeadingAndTrailingWhitespace);
				for (const auto& arg : argsV) {
					if (arg.front() == 'i') {
						//"inx abc = "
						if (arg.substr(0, 4) != "int ") {
							logger.Error("Syntax error: Invalid parameter type (parameter <{}> of declaration <{}>)"sv, arg, str);
							return false;
						}
						traversal_state ts{ .index = 3 };
						if (!SkipSpace(arg, ts) || !IsWordChar(arg[ts.index])) {
							logger.Error("Syntax error: Invalid parameter identifier (parameter <{}> of declaration <{}>)"sv, arg, str);
							return false;
						}
						szt aux{ ts.index };
						if (!SkipIdentifier(arg, ts)) {
							logger.Error("Syntax error: Parameters must have default values (parameter <{}> of declaration <{}>)"sv, arg, str);
							return false;
						}
						sig += "int " + arg.substr(aux, ts.index - aux) + " = ";

						//"int abc = 123, "
						if (arg[ts.index] != '=' && (!SkipSpace(arg, ts) || arg[ts.index] != '=')) {
							logger.Error("Syntax error: Parameters must have default values (parameter <{}> of declaration <{}>)"sv, arg, str);
							return false;
						}
						if (!SkipSpace(arg, ts)) {
							logger.Error("Syntax error: Parameters must have default values (parameter <{}> of declaration <{}>)"sv, arg, str);
							return false;
						}
						aux = ts.index;
						if (!ReadInt(arg, ts) || ts.index != arg.length() - 1) {
							logger.Error("Syntax error: Expected end of parameter (parameter <{}> of declaration <{}>)"sv, arg, str);
							return false;
						}
						sig += arg.substr(aux) + ", ";
					}
					else if (arg.front() == 'f') {
						//"float abc = "
						if (arg.substr(0, 6) != "float ") {
							logger.Error("Syntax error: Invalid parameter type (parameter <{}> of declaration <{}>)"sv, arg, str);
							return false;
						}
						traversal_state ts{ .index = 5 };
						if (!SkipSpace(arg, ts) || !IsWordChar(arg[ts.index])) {
							logger.Error("Syntax error: Invalid parameter identifier (parameter <{}> of declaration <{}>)"sv, arg, str);
							return false;
						}
						szt aux{ ts.index };
						if (!SkipIdentifier(arg, ts)) {
							logger.Error("Syntax error: Parameters must have default values (parameter <{}> of declaration <{}>)"sv, arg, str);
							return false;
						}
						sig += "float " + arg.substr(aux, ts.index - aux) + " = ";

						//"float abc = 123, "
						if (arg[ts.index] != '=' && (!SkipSpace(arg, ts) || arg[ts.index] != '=')) {
							logger.Error("Syntax error: Parameters must have default values (parameter <{}> of declaration <{}>)"sv, arg, str);
							return false;
						}
						if (!SkipSpace(arg, ts)) {
							logger.Error("Syntax error: Parameters must have default values (parameter <{}> of declaration <{}>)"sv, arg, str);
							return false;
						}
						aux = ts.index;
						if (!ReadFloat(arg, ts) || ts.index != arg.length() - 1) {
							logger.Error("Syntax error: Expected end of parameter (parameter <{}> of declaration <{}>)"sv, arg, str);
							return false;
						}
						sig += arg.substr(aux) + ", ";
					}
					else {
						logger.Error("Syntax error: Invalid parameter type (parameter <{}> of declaration <{}>)"sv, arg, str);
						return false;
					}
				}
				
				if (sig.back() != '(') {
					sig.pop_back();
					sig.pop_back();
				}
				sig += ')';
				str = std::move(sig);
				return true;
			}
			catch (...) {
				logger.Error("Unspecified exception trying to validate function signature <{}>", str);
				return false;
			}
		}
		//Expects str to end in ')'
		[[nodiscard]] bool FormatAndValidateIncludeSignature(string& str) noexcept {
			if (str.length() < 13) { //!include("a") min length
				//too short
				return false;
			}

			if (str.substr(0, 8) != "!include") {
				logger.Error("Invalid !include line"sv);
				return false;
			}
			traversal_state ts{ .index = 7 }; //'e'
			if (!SkipSpace(str, ts) || str[ts.index] != '(') {
				logger.Error("Missing <(> of !include line"sv);
				return false;
			}
			string sig{ "!include(" };

			if (!SkipSpace(str, ts) || str[ts.index] != '"') {
				logger.Error("Missing string of !include line"sv);
				return false;
			}
			szt aux{ ts.index }; //Opening '"'
			if (!ReadString(str, ts)) {
				logger.Error("Invalid string of !include line"sv);
				return false;
			}
			sig += str.substr(aux, ts.index - aux + 1);

			if (!SkipSpace(str, ts) || str[ts.index] != ')') {
				logger.Error("Missing <)> of !include line"sv);
				return false;
			}
			sig += ')';
			str = std::move(sig);
			return true;
		}
		//Expects str to end in ')'
		[[nodiscard]] bool FormatAndValidateVarDeclSignature(string& str) noexcept {
			//Identifier
			traversal_state ts{ .index = 0 };
			szt aux{ 0 };
			if (!ReadIdentifier(str, ts)) {
				logger.Error("Invalid variable declaration identifier"sv);
				return false;
			}
			string sig{ str.substr(aux, ts.index - aux + 1) };

			//Type
			int type;
			uint32 vecSz{ 0 };
			if (sig == "VarFloat") {
				type = 'flt';
			}
			else if (sig == "VarInt") {
				type = 'int';
			}
			else if (sig == "VarString") {
				type = 'str';
			}
			else if (sig.substr(0, 6) == "VarVec") {
				type = 'vec';
				string amount{ sig.substr(6) };
				if (amount.empty() || !allNumberChar(amount)) {
					//no amount or fancy number we don't want to feed to std::stoul. Simple ints only.
					logger.Error("Invalid vector variable declaration identifier. Proper format is VarVec[1-9][0-9]*"sv);
					return false;
				}
				try {
					vecSz = std::stoul(amount.c_str(), nullptr);
				}
				catch (const std::invalid_argument& ex) {
					logger.Error("Syntax error at line {}: <{}> cannot be parsed to an integer: ({})", ts.line, amount, ex.what());
					return false;
				}
				if (vecSz == 0) {
					logger.Error("Vector variable declarations of 0 element vectors are not allowed"sv);
					return false;
				}
			}
			else {
				logger.Error("Unrecognized variable declaration type"sv);
				return false;
			}

			//String
			if (!SkipSpace(str, ts) || str[ts.index] != '(' || !SkipSpace(str, ts)) {
				logger.Error("Variable declaration incomplete (missing <(>?)"sv);
				return false;
			}
			aux = ts.index;
			if (!ReadString(str, ts)) {
				logger.Error("Variable declaration missing name string or name string invalid"sv);
				return false;
			}
			sig += '(' + str.substr(aux, ts.index - aux + 1) + ", ";

			//Value
			if (!SkipSpace(str, ts) || str[ts.index] != ',' || !SkipSpace(str, ts)) {
				logger.Error("Variable declaration missing value parameter"sv);
				return false;
			}
			switch (type) {
			case 'flt':
				aux = ts.index;
				if (!ReadFloat(str, ts) && !ReadInt(str, ts)) { //Likely dev oversight, but I came across at least 1 VarFloat with int argument in og varlist, so gotta handle it :))
					logger.Error("VarFloat declarations can only have a float or int as second parameter"sv);
					return false;
				}
				sig += str.substr(aux, ts.index - aux + 1);
				break;
			case 'int':
				aux = ts.index;
				if (!ReadInt(str, ts)) {
					logger.Error("VarInt declarations can only have an int as second parameter"sv);
					return false;
				}
				sig += str.substr(aux, ts.index - aux + 1);
				break;
			case 'str':
				aux = ts.index;
				if (!ReadString(str, ts)) {
					logger.Error("VarString declarations can only have a string as second parameter"sv);
					return false;
				}
				sig += str.substr(aux, ts.index - aux + 1);
				break;
			case 'vec':
			{
				if (str[ts.index] != '[') {
					logger.Error("Vector variable declarations must have a float vector as second parameter"sv);
					return false;
				}
				aux = ts.index;
				ts.index = str.find(']', ts.index);
				if (ts.index >= str.length()) {
					logger.Error("Vector variable declarations must have a float vector as second parameter (missing <]>)"sv);
					return false;
				}
				sig += '[';
				vector<string> elems{ Split(str.substr(aux + 1, ts.index - aux - 1), ',', RemoveLeadingAndTrailingWhitespace) };
				if (elems.size() != vecSz) {
					logger.Error("Vector variable element count mismatch: expected {}, read {}"sv, vecSz, elems.size());
					return false;
				}
				for (const auto& elem : elems) {
					if (elem.empty()) {
						logger.Error("The vector parameter of vector variable declarations must only have float elements (read empty element)"sv);
						return false;
					}
					traversal_state elem_ts{ .index = 0 };
					if ((!ReadFloat(elem, elem_ts) && !ReadInt(elem, elem_ts)) || (elem_ts.index != elem.length() - 1)) { //See float case
						logger.Error("The vector parameter of vector variable declarations must only have float or int elements"sv);
						return false;
					}
					sig += elem + ", ";
				}
				sig.pop_back(); //vecSz > 0 so loop will run at least once
				sig.pop_back();
				sig += ']';
				break;
			}
			default:
				logger.Error("How did you even get here? FormatAndValidateVarDeclSignature value param switch"sv);
				return false;
			}

			if (!SkipSpace(str, ts) || str[ts.index] != ')') {
				logger.Error("Variable declaration missing <)>"sv);
				return false;
			}

			sig += ')';
			str = std::move(sig);
			return true;
		}
		[[nodiscard]] string TypeStr(NodeFlags flags) {
			if (flags.Any(Flag::Import))			return "[Import] ";
			if (flags.Any(Flag::Export))			return "[Export] ";
			if (flags.Any(Flag::SubScope))			return "[SubScope] ";
			if (flags.Any(Flag::SubDeclaration))	return "[SubDeclaration] ";
			if (flags.Any(Flag::Use))				return "[Use] ";
			if (flags.Any(Flag::Function))			return "[Function] ";
			return "[INVALID] ";
		}
	}
	using namespace helpers;


	Node::Node(uint32 sID, uint32 nsID, uint32 cmpID, NodeFlags op, uint32 bkID, uint64 srcln) noexcept
		: sigID(sID)
		, newsigID(nsID)
		, comparesigID(cmpID)
		, flags(op)
		, ordersigID(bkID)
		, sourceline(srcln)
	{}

	Node& Node::AddAndReturnSubnode(uint32 sID, uint32 nsID, uint32 cmpID, NodeFlags op, uint32 bkID, uint64 srcln) noexcept {
		try { return subnodes.emplace_back(sID, nsID, cmpID, op, bkID, srcln); }
		catch (...) { logger.Critical("Unrecoverable error: Failed to construct-add node with signature ID {}"sv, sID); std::terminate(); }
	}
	Node& Node::AddAndReturnSubnode(const Node& node) noexcept {
		if (!PushBackNoEx(subnodes, node)) {
			logger.Critical("Unrecoverable error: Failed to copy-add node with signature ID {}"sv, node.GetSigID());
			std::terminate();
		}
		return subnodes.back();
	}
	Node& Node::AddAndReturnSubnode(Node&& node) noexcept {
		if (!PushBackNoEx(subnodes, std::move(node))) {
			logger.Critical("Unrecoverable error: Failed to move-add node with signature ID {}"sv, node.GetSigID());
			std::terminate();
		}
		return subnodes.back();
	}
	bool Node::AddSubnode(uint32 sID, uint32 nsID, uint32 cmpID, NodeFlags op, uint32 bkID, uint64 srcln) noexcept {
		try { subnodes.emplace_back(sID, nsID, cmpID, op, bkID, srcln); return true; }
		catch (...) { logger.Critical("Unspecified error: Failed to construct-add node with signature ID {}"sv, sID); return false; }
	}
	bool Node::AddSubnode(const Node& node) noexcept {
		if (!PushBackNoEx(subnodes, node)) {
			logger.Critical("Unspecified error: Failed to copy-add node with signature ID {}"sv, node.GetSigID());
			return false;
		}
		return true;
	}
	bool Node::AddSubnode(Node&& node) noexcept {
		if (!PushBackNoEx(subnodes, std::move(node))) {
			logger.Critical("Unspecified error: Failed to move-add node with signature ID {}"sv, node.GetSigID());
			return false;
		}
		return true;
	}

	bool Node::DeleteSubnode(const uint32 id) noexcept {
		if (NodeIterator iter{ Find(id) }; iter != subnodes.end()) {
			subnodes.erase(iter);
			return true;
		}
		return false;
	}
	bool Node::RenameSubnode(const uint32 id, const uint32 newid) noexcept {
		if (NodeIterator iter{ Find(id) }; iter != subnodes.end()) {
			iter->SetSigID(newid);
			return true;
		}
		return false;
	}

	void Node::SetSigID(const uint32 newid) noexcept { sigID = newid; }
	void Node::SetNewsigID(const uint32 newnewid) noexcept { newsigID = newnewid; }
	void Node::SetComparesigID(const uint32 newcompid) noexcept { comparesigID = newcompid; }
	bool Node::SetSubnodes(const NodeVector& newsubnodes) noexcept {
		try { subnodes = newsubnodes; return true; }
		catch (...) { return false; }
	}
	void Node::SetFlags(const NodeFlags newop) noexcept { flags = newop; }
	void Node::SetOrder(const uint32 neworder) noexcept { order = neworder; }
	void Node::SetOrderSigID(const uint32 newid) noexcept { ordersigID = newid; }
	void Node::SetSourceLine(const uint64 newsourceline) noexcept { sourceline = newsourceline; }

	[[nodiscard]] uint32 Node::GetSigID() const noexcept { return sigID; }
	[[nodiscard]] uint32 Node::GetNewsigID() const noexcept { return newsigID; }
	[[nodiscard]] uint32 Node::GetComparesigID() const noexcept { return comparesigID; }
	[[nodiscard]] const NodeVector& Node::GetSubnodes() const noexcept { return subnodes; }
	[[nodiscard]] szt Node::GetNumSubnodes() const noexcept { return subnodes.size(); }
	[[nodiscard]] NodeFlags Node::GetFlags() const noexcept { return flags; }
	[[nodiscard]] uint32 Node::GetOrder() const noexcept { return order; }
	[[nodiscard]] uint32 Node::GetOrdersigID() const noexcept { return ordersigID; }
	[[nodiscard]] uint64 Node::GetSourceLine() const noexcept { return sourceline; }

	[[nodiscard]] NodeVector Node::CopySubnodes() const { return subnodes; }
	[[nodiscard]] const NodeVector& Node::GetSubnodesRef() const noexcept { return subnodes; }

	[[nodiscard]] NodeIterator Node::Begin() noexcept { return subnodes.begin(); }
	[[nodiscard]] NodeCIterator Node::CBegin() const noexcept { return subnodes.cbegin(); }
	[[nodiscard]] NodeIterator Node::End() noexcept { return subnodes.end(); }
	[[nodiscard]] NodeCIterator Node::CEnd() const noexcept { return subnodes.cend(); }

	[[nodiscard]] NodeIterator Node::Find(const uint32 id) noexcept {
		try {
			for (NodeIterator iter{ subnodes.begin() }; iter != subnodes.end(); ++iter) {
				if ((*iter).sigID == id) {
					return iter;
				}
			}
			return subnodes.end();
		}
		catch (...) {
			return subnodes.end();
		}
	}
	[[nodiscard]] NodeIterator Node::Find(const vector<uint32>& idtree) noexcept {
		if (idtree.empty() || subnodes.empty())
			return subnodes.end();
		try {
			NodeIterator iter{ subnodes.begin() };
			for (const auto id : idtree) {
				if (NodeIterator iternew{ iter->Find(id) }; iternew == iter->End()) {
					return subnodes.end();
				}
				else {
					iter = iternew;
				}
			}
			return iter;
		}
		catch (...) {
			return subnodes.end();
		}
	}
	[[nodiscard]] NodeIterator Node::Find(vector<uint32>::const_iterator start, const vector<uint32>::const_iterator end) noexcept {
		if (subnodes.empty() || start == end)
			return subnodes.end();
		try {
			NodeIterator iter{ Find(*start) };
			if (iter == subnodes.end()) {
				return iter;
			}
			while (++start != end) {
				if (NodeIterator iternew{ iter->Find(*start) }; iternew == iter->End()) {
					return subnodes.end();
				}
				else {
					iter = iternew;
				}
			}
			return iter;
		}
		catch (...) {
			return subnodes.end();
		}
	}

	[[nodiscard]] string Node::ToString(szt depth, const Cache& string_cache) const {
		string indent(depth, '\t');
		string result{ indent + string_cache.Find(sigID) };

		if (subnodes.empty()) {
			if (!flags.Any(Flag::Import, Flag::Include, Flag::Vardecl)) result += ';';
			return result;
		}

		result += " {\n";
		for (const auto& child : subnodes) {
			result += child.ToString(depth + 1u, string_cache) + '\n';
		}
		result += indent + '}';

		return result;
	}
	[[nodiscard]] string Node::ToStringAttr(szt depth, const Cache& string_cache) const {
		string indent(depth, '\t');
		string result{ indent + string_cache.Find(sigID) };

		if (flags.Any(Flag::Noop))
			result += "[Noop]";
		if (flags.Any(Flag::Insert))
			result += "[Insert]";
		if (flags.Any(Flag::Rename))
			result += "[Rename]";
		if (flags.Any(Flag::Redefine))
			result += "[Redefine]";
		if (flags.Any(Flag::Delete))
			result += "[Delete]";

		if (flags.Any(Flag::Rename)) {
			result += string_cache.Find(newsigID);
		}

		if (subnodes.empty()) {
			if (!flags.Any(Flag::Import)) result += ';';
			return result;
		}

		result += " {\n";
		for (const auto& child : subnodes) {
			result += child.ToStringAttr(depth + 1u, string_cache) + '\n';
		}
		result += indent + '}';

		return result;
	}




	//Parser	public
	[[nodiscard]] bool Parser::SetDiff(const string& diff_str) noexcept {
		try {
			Locker locker{ lock };
			if (!DeduceFileInfo(diff_str.substr(0u, diff_str.find('\n')))) {
				logger.Error("Invalid packed file path or extension"sv);
				return false;
			}
			return SetFile(diff_str, true); //Includes the '\n'
		}
		catch (...) {
			logger.Error("Unknown exception while trying to set diff"sv);
			return false;
		}
	}

	[[nodiscard]] bool Parser::SetTarget(const string& target_str) noexcept {
		try {
			Locker locker{ lock };
			return SetFile(target_str, false);
		}
		catch (...) {
			logger.Error("Unknown exception while trying to set target"sv);
			return false;
		}
	}

	[[nodiscard]] string Parser::GetTargetPath() const { Locker locker{ lock }; return target_path; }

	[[nodiscard]] bool Parser::Parse(string& out) noexcept {
		try {
			Locker locker{ lock };
			if (diff.empty() || target.empty()) {
				logger.Error("Error: Attempted to parse but not both diff and target trees are generated"sv);
				return false;
			}

			switch (filetype) {
			case FileType::scr:
				if (!ParseScrLoot(out)) {
					logger.Error("Failed to parse <scr> file"sv);
					return false;
				}
				break;
			case FileType::def:
				if (!ParseDef(out)) {
					logger.Error("Failed to parse <def> file"sv);
					return false;
				}
				break;
			case FileType::loot:
				if (!ParseScrLoot(out)) { //Yes, same function. Loot and Scr are almost identical.
					logger.Error("Failed to parse <loot> file"sv);
					return false;
				}
				break;
			case FileType::varlist:
				if (!ParseVarlist(out)) {
					logger.Error("Failed to parse <varlist.scr>"sv);
					return false;
				}
				break;
			default:
				logger.Error("Invalid target filetype"sv);
				return false;
			}

			return true;
		}
		catch (...) {
			logger.Error("Unknown exception while trying to parse"sv);
			return false;
		}
	}


	void Parser::Reset() noexcept {
		try {
			Locker locker{ lock };
			ResetImpl();
		}
		catch (...) {
			logger.Error("Parser::Reset() failed but state was not affected"sv);
		}
	}

	void Parser::PrintTrees() const {
		Locker locker{ lock };

		if (diff.empty()) {
			logger.Error("PrintTrees: member <diff> has no nodes so it cannot be printed!"sv);
			return;
		}
		logger.Info("Outputting diff tree contents...\n"sv);
		vector<string> nodesStr(diff.size());
		for (szt i{ 0 }; i < diff.size(); ++i) {
			nodesStr[i] = diff[i].ToStringAttr(0, string_cache);
		}
		logger.ToFile("diff_tree.scr", Join(nodesStr, '\n'));
		
		if (target.empty()) {
			logger.Error("PrintTrees: member <target> has no nodes so it cannot be printed!"sv);
			return;
		}
		logger.Info("Outputting diff tree contents...\n"sv);
		nodesStr.resize(target.size());
		for (szt i{ 0 }; i < target.size(); ++i) {
			nodesStr[i] = target[i].ToStringAttr(0, string_cache);
		}
		logger.ToFile("target_tree.scr", Join(nodesStr, '\n'));
	}

	//Parser	private
	[[nodiscard]] bool Parser::SetFile(const string& str, bool isdiff) {
		string str_copy{ [isdiff, &str] { if (isdiff) return str.substr(str.find('\n')); else return str; }() };
		RemoveComments(str_copy);
		if (!TabToSpace(str_copy)) {
			logger.Error("TabToSpace failed for unspecified reasons"sv);
			return false;
		}

		switch (filetype) {
		case FileType::scr:
			if (!GenerateTreeScr(str_copy, isdiff)) {
				logger.Error("Failed to generate tree from <scr> file"sv);
				HandleResets(isdiff);
				return false;
			}
			break;
		case FileType::def:
			if (!GenerateTreeDef(str_copy, isdiff)) {
				logger.Error("Failed to generate tree from <def> file"sv);
				HandleResets(isdiff);
				return false;
			}
			break;
		case FileType::loot:
			if (!GenerateTreeLoot(str_copy, isdiff)) {
				logger.Error("Failed to generate tree from <loot> file"sv);
				HandleResets(isdiff);
				return false;
			}
			break;
		case FileType::varlist:
			if (!GenerateTreeVarlist((str_copy + '_'), isdiff)) { //To work with things like ParseAttribute which weren't made with the assumption that a file could end in an attribute
				logger.Error("Failed to generate tree from <varlist.scr>"sv);
				HandleResets(isdiff);
				return false;
			}
			break;
		default:
			logger.Error("Invalid target filetype"sv);
			return false;
		}

		return true;
	}

	[[nodiscard]] bool Parser::DeduceFileInfo(const string& firstline) {
		//Varlist handling
		auto pos = firstline.rfind('/');
		string temp{ (pos >= firstline.length() ? firstline : firstline.substr(pos + 1)) };
		RemoveTrailingWhitespace(temp); //Allow whitespace at end of line
		if (StrICmp(temp.c_str(), "varlist.scr")) {
			filetype = FileType::varlist;
			temp = firstline;
			RemoveLeadingAndTrailingWhitespace(temp);
			target_path = std::move(temp);
			return true;
		}
		//Normal case handling
		pos = firstline.rfind('.');
		if (pos >= firstline.length()) {
			filetype = FileType::INVALID_FILETYPE;
			logger.Error("Failed to find <.> in first line of diff"sv);
			return false;
		}
		temp = firstline.substr(pos + 1);
		RemoveTrailingWhitespace(temp); //Allow whitespace at end of line
		if (StrICmp(temp.c_str(), "def"))
			filetype = FileType::def;
		else if (StrICmp(temp.c_str(), "scr"))
			filetype = FileType::scr;
		else if (StrICmp(temp.c_str(), "loot"))
			filetype = FileType::loot;
		else {
			filetype = FileType::INVALID_FILETYPE;
			logger.Error("File extension <{}> unknown!"sv, temp);
			return false;
		}
		temp = firstline;
		RemoveLeadingAndTrailingWhitespace(temp);
		target_path = std::move(temp);
		return true;
	}

	[[nodiscard]] vector<Node>& Parser::GetVec(bool isdiff) noexcept { return (isdiff ? diff : target); }

	//Tree generation
	[[nodiscard]] bool Parser::GenerateTreeScr(const string& str, bool isdiff) {
		HandleResets(isdiff);

		if (!ValidateBraces(str) || !ValidateParens(str)) {
			logger.Error("Syntax error: brace or paren mismatch"sv);
			return false;
		}

		traversal_state ts{ .line = (isdiff ? 2u : 1u) };
		{
			//Handle import lines
			if (!GenerateImportNodes(str, ts, isdiff)) {
				logger.Error("Failed to parse import lines"sv);
				return false;
			}

			//Handle export lines
			if (!GenerateExportNodes(str, ts, isdiff)) {
				logger.Error("Failed to parse export lines"sv);
				return false;
			}

			//Find main node signature
			string subsig{};
			if (!FindScrSub(str, ts, subsig)) {
				logger.Error("Syntax error: Failed to find <sub X()> or signature incomplete"sv);
				return false;
			}
			if (!SkipSpace(str, ts)) {
				logger.Error("Syntax error: <{}> signature incomplete"sv, subsig);
				return false;
			}
			//Find main node flags
			NodeFlags flags{};
			if (str[ts.index] == '[' && !ParseAttributes(str, ts, flags)) {
				logger.Error("Syntax error: <{}> has invalid attributes"sv, subsig);
				return false;
			}
			if (flags.Any(Flag::Insert)) {
				logger.Error("Syntax error: <{}> cannot be appended"sv, subsig);
				return false;
			}
			if (flags.Any(Flag::Rename)) {
				logger.Error("Syntax error: <{}> cannot be renamed"sv, subsig);
				return false;
			}
			if (flags.Any(Flag::Delete)) {
				logger.Error("Syntax error: <{}> cannot be deleted"sv, subsig);
				return false;
			}
			uint32 sigID = string_cache.FindOrAdd(subsig);
			if (sigID == Cache::NULL_ID) {
				logger.Error("Failed to add <{}>'s signature to cache"sv, subsig);
				return false;
			}
			flags.Unset(Flag::Insert, Flag::Rename, Flag::Delete);
			if (flags.None()) { flags.Set(Flag::Noop); }
			
			//Find main node scope
			if (str[ts.index] != '{' && (!SkipSpaceNewline(str, ts) || str[ts.index] != '{')) { //Find '{'
				logger.Error("Syntax error: <{}> must have a scope"sv, subsig);
				return false;
			}
			if (!SkipSpaceNewline(str, ts)) { //Make sure '{' isn't the last useful char
				logger.Error("Syntax error: <{}> must have a complete scope"sv, subsig);
				return false;
			}
			if (str[ts.index] == '}') { //sub X(){}
				return true;
			}
			//Main node creation
			flags.Set(Flag::SubScope);
			//Node toAdd{ sigID, Cache::NULL_ID, sigID, flags, sigID, ts.line };
			if (!PushBackNoEx(GetVec(isdiff), { sigID, Cache::NULL_ID, sigID, flags, sigID, ts.line })) {
				logger.Error("Unexpected error when trying to store main node data. Possibly out of memory?"sv);
				return false;
			}
		}
		//Main node scope handling
		--ts.index; //Move a step back to work with GenerateScopeNodes
		if (!GenerateScopeNodes(GetVec(isdiff).back(), str, ts, isdiff)) {
			logger.Error("Failed to parse <{}>'s contents"sv, string_cache.Find(diff.back().GetSigID()));
			return false;
		}

		return true;
	}
	
	[[nodiscard]] bool Parser::GenerateTreeDef(const string& str, bool isdiff) {
		HandleResets(isdiff);

		if (!ValidateParens(str)) {
			logger.Error("Syntax error: paren mismatch"sv);
			return false;
		}

		traversal_state ts{ .line = (isdiff ? 2u : 1u) };
		if (!GenerateExportNodes(str, ts, isdiff)) {
			logger.Error("Failed to parse export lines"sv);
			return false;
		}

		return true;
	}

	[[nodiscard]] bool Parser::GenerateTreeLoot(const string& str, bool isdiff) {
		HandleResets(isdiff);

		if (!ValidateBraces(str) || !ValidateParens(str)) {
			logger.Error("Syntax error: brace or paren mismatch"sv);
			return false;
		}

		traversal_state ts{ .line = (isdiff ? 2u : 1u) };

		//Handle import lines
		if (!GenerateImportNodes(str, ts, isdiff)) {
			logger.Error("Failed to parse import lines"sv);
			return false;
		}

		//Handle export lines
		if (!GenerateExportNodes(str, ts, isdiff)) {
			logger.Error("Failed to parse export lines"sv);
			return false;
		}

		while (true) {
			//Signature
			szt aux{ ts.index };
			string subsig{};
			uint32 cmpID{ 0 };
			if (!isdiff) {
				ts.index = str.find(')', ts.index);
				subsig = str.substr(aux, ts.index - aux + 1); //"sub X(...)"
				if (!FormatAndValidateSubDeclSignature(subsig)) {
					logger.Error("Syntax error: Invalid sub declaration <{}> at line {}"sv, subsig, ts.line);
					return false;
				}
				string temp{ subsig.substr(0, subsig.find('(')) };
				cmpID = string_cache.FindOrAdd(temp);
				if (cmpID == Cache::NULL_ID) {
					logger.Error("Failed to add sub declaration <{}>'s compare sig to cache"sv, temp);
					return false;
				}
			}
			else {
				if (str.substr(aux, 4) != "sub ") {
					//bad sub 
					return false;
				}
				ts.index += 3;
				if (!SkipSpace(str, ts)) {
					//no space after sub
					return false;
				}
				szt tempsz{ ts.index };
				if (!ReadIdentifier(str, ts)) {
					//bad id
					return false;
				}
				subsig = "sub " + str.substr(tempsz, ts.index - tempsz + 1);
			}
			uint32 sID{ string_cache.FindOrAdd(subsig)};
			if (sID == Cache::NULL_ID) {
				logger.Error("Failed to add sub declaration <{}>'s signature to cache"sv, subsig);
				return false;
			}
			if (isdiff) {
				cmpID = sID;
			}

			//Attributes
			NodeFlags flags{};
			if (isdiff) {
				if (!SkipSpace(str, ts)) {
					//Can't skip after ')'
					return false;
				}
				if (str[ts.index] == '[' && !ParseAttributes(str, ts, flags)) {
					//Can't parse attributes
					return false;
				}
			}

			//Scope
			aux = ts.line;
			if (str[ts.index] != '{' && (!SkipSpaceNewline(str, ts) || str[ts.index] != '{')) {
				//Expected scope
				return false;
			}

			//Add node
			if (flags.Any(Flag::Delete, Flag::Insert, Flag::Rename)) {
				logger.Error("Invalid sub declaration <{}>. Sub declarations cannot be deleted, inserted, or renamed"sv, subsig);
			}
			flags.Set(Flag::SubDeclaration);
			if (!PushBackNoEx(GetVec(isdiff), { sID, Cache::NULL_ID, cmpID, flags, cmpID, ts.line })) {
				logger.Error("Unexpected error when trying to store sub declaration node data. Possibly out of memory?"sv);
				return false;
			}

			//Add children
			if (!GenerateScopeNodes(GetVec(isdiff).back(), str, ts, isdiff)) {
				logger.Error("Failed to parse <{}>'s contents (at line {})"sv, subsig, aux);
				return false;
			}

			//Find start of next sig after '}'
			if (!SkipSpaceNewline(str, ts)) {
				return true; //Last sub declaration. Not an error to find EOF.
			}
		} // /while
	}

	[[nodiscard]] bool Parser::GenerateTreeVarlist(const string& str, bool isdiff) {
		HandleResets(isdiff);

		if (!ValidateParens(str)) {
			logger.Error("Syntax error: paren mismatch"sv);
			return false;
		}

		traversal_state ts{ .index = 0, .line = (isdiff ? 2u : 1u) };
		if (!GenerateVarlistNodes(str, ts, isdiff)) {
			logger.Error("Failed to parse varlist lines"sv);
			return false;
		}
		


		return true;
	}

	//Skips ' ' & newline chars if one is pointed at and starts reading signature. Leaves ts.index pointing to the first non ' ', non newline char after last import line
	[[nodiscard]] bool Parser::GenerateImportNodes(const string& str, traversal_state& ts, bool isdiff) noexcept {
		while (true) {
			if (ts.index >= str.length() || (IsSpaceNewline(str[ts.index]) && !SkipSpaceNewline(str, ts))) {
				logger.Error("Syntax error: scr files must specify a sub scope"sv);
				return false;
			}

			if (str.substr(ts.index, 7) != "import ") {
				return true;
			}

			//Signature
			ts.index += 6; //Index of ' '
			if (!SkipSpace(str, ts) || str[ts.index] != '"') {
				logger.Error("Syntax error at line {}: import lines need to specify a string"sv, ts.line);
				return false;
			}
			szt openq{ ts.index };
			ts.index = str.find('"', ts.index + 1);
			if (ts.index >= str.length()) {
				logger.Error("Syntax error at line {}: no closing <\"> for import line's string"sv, ts.line);
				return false;
			}
			if (!allNotNewline(str.substr(openq + 1, ts.index - openq - 1))) {
				logger.Error("Syntax error at line {}: string arguments cannot change lines"sv, ts.line);
				return false;
			}
			string importSig{ "import " + str.substr(openq, ts.index - openq + 1) };
			string importNewSig{ "" };
			uint32 sigID{ string_cache.FindOrAdd(importSig) };
			uint32 sigNewID{ Cache::NULL_ID };
			if (sigID == Cache::NULL_ID) {
				logger.Error("Failed to add <{}>'s signature to cache (at line {})"sv, importSig, ts.line);
				return false;
			}

			//Flags
			if (!SkipSpace(str, ts)) {
				logger.Error("Syntax error at line {}: incomplete import signature <{}>"sv, ts.line, importSig);
				return false;
			}
			NodeFlags flags{};
			if (isdiff) {
				if (!ParseAttributes(str, ts, flags)) {
					logger.Error("Syntax error at line {}: invalid <{}> import attributes"sv, ts.line, importSig);
					return false;
				}
				if (flags.Any(Flag::Rename)) {
					if (str[ts.index] != '"') {
						logger.Error("Syntax error at line {}: import declarations with [rename] attribute need a string to follow (while parsing <{}>)"sv, ts.line, importSig);
						return false;
					}
					openq = ts.index;
					ts.index = str.find('"', ts.index + 1);
					if (ts.index >= str.length()) {
						logger.Error("Syntax error at line {}: no closing <\"> for import declaration's rename string (while parsing <{}>)"sv, ts.line, importSig);
						return false;
					}
					if (!allNotNewline(str.substr(openq + 1, ts.index - openq - 1))) {
						logger.Error("Syntax error at line {}: string arguments cannot change lines (while parsing <{}>)"sv, ts.line, importSig);
						return false;
					}
					importNewSig = "import " + str.substr(openq, ts.index - openq + 1);
					sigNewID = string_cache.FindOrAdd(importNewSig);
					if (sigNewID == Cache::NULL_ID) {
						logger.Error("Failed to add <{}>'s rename string to cache (at line {})"sv, importSig, ts.line);
						return false;
					}
					if (!SkipSpace(str, ts)) {
						logger.Error("Syntax error at line {}: file must at least contain a sub scope (while parsing <{}>)"sv, ts.line, importSig);
						return false;
					}
					logger.Info("");
				}
			}

			if (!IsNewlineChar(str[ts.index])) {
				logger.Error("Syntax error at line {}: import declarations must fully occupy their line (while parsing <{}>)"sv, ts.line, importSig);
				return false;
			}
			++ts.line;

			//Import line end
			if (!isdiff || !flags.Only(Flag::Noop, Flag::Redefine)) {
				if (flags.Any(Flag::Delete, Flag::Insert, Flag::Redefine)) {
					logger.Error("Invalid import <{}>. Imports cannot be deleted, inserted, or redefined"sv, importSig);
				}
				flags.Set(Flag::Import);
				if (!PushBackNoEx(GetVec(isdiff), { sigID, sigNewID, sigID, flags, sigID, ts.line }) ) {
					logger.Error("Unexpected error when trying to store import node data. Possibly out of memory? (while parsing <{}>)"sv, importSig);
					return false;
				}
			}
		}

		return true;
	}
	//Skips ' ' & newline chars if one is pointed at and starts reading signature. Leaves ts.index pointing to the first non ' ', non newline char after last export line
	[[nodiscard]] bool Parser::GenerateExportNodes(const string& str, traversal_state& ts, bool isdiff) noexcept {
		while (true) {
			if (ts.index >= str.length() || (IsSpaceNewline(str[ts.index]) && !SkipSpaceNewline(str, ts))) {
				if (filetype == FileType::scr) {
					logger.Error("Syntax error: scr files must specify a sub scope"sv);
					return false;
				}
				else if (filetype == FileType::def) {
					return true;
				}
			}

			//Declaration
			if (str.substr(ts.index, 7) != "export "s) {
				return true;
			}
			ts.index += 6; //Index of ' '
			
			//sig strings
			string exportSig{};
			uint32 sigID{ Cache::NULL_ID };
			uint32 sigNewID{ Cache::NULL_ID  };
			uint32 sigCmpID{ Cache::NULL_ID };
			string typeDecl{};

			//Type
			if (!SkipSpace(str, ts)) {
				logger.Error("Syntax error at line {}: incomplete export declaration. Expected type."sv, ts.line);
				return false;
			}
			int32 type{ 0 };
			if (str.substr(ts.index, 4) == "int "s) {
				type = 'int';
				typeDecl = "export int ";
				ts.index += 3; //index of ' '
			}
			else if (str.substr(ts.index, 6) == "float "s) {
				type = 'flt';
				typeDecl = "export float ";
				ts.index += 5; //index of ' '
			}
			else if (str.substr(ts.index, 7) == "string "s) {
				type = 'str';
				typeDecl = "export string ";
				ts.index += 6; //index of ' '
			}
			else {
				logger.Error("Syntax error at line {}: only <int>, <float>, and <string> types can be exported"sv, ts.line);
				return false;
			}

			//Identifier
			if (!SkipSpace(str, ts) || !IsWordChar(str[ts.index])) {
				logger.Error("Syntax error at line {}: incomplete export declaration. Expected identifier"sv, ts.line);
				return false;
			}
			szt aux{ ts.index };
			--ts.index;
			if (!SkipIdentifier(str, ts)) {
				logger.Error("Unexpected error at line {}. This should not have been caused by this program."sv, ts.line);
				return false;
			}
			string id{ str.substr(aux, ts.index - aux) };
			exportSig = typeDecl + id;
			sigCmpID = string_cache.FindOrAdd(exportSig);
			if (sigCmpID == Cache::NULL_ID) {
				// bad cache
				return false;
			}

			auto readValue = [&](string& valueStr, const string& decor) {
				if (type == 'int') {
					aux = ts.index;
					if (!ReadInt(str, ts) && !ReadIdentifier(str, ts)) {
						logger.Error("Syntax error at line {}: expected an int or an identifier for <{}>'s {}value"sv, ts.line, id, decor);
						return false;
					}
					valueStr = str.substr(aux, ts.index - aux + 1);

					//Logical OR for int
					aux = ts.index;
					if (SkipSpace(str, ts) && str[ts.index] == '|') {
						while (true) {
							if (!SkipSpace(str, ts)) { //Find first char after '|'
								logger.Error("Syntax error at line {}: expected an int or an identifier for <{}>'s {}<|> operator"sv, ts.line, id, decor);
								return false;
							}
							szt start{ ts.index };
							if (!ReadInt(str, ts) && !ReadIdentifier(str, ts)) {
								logger.Error("Syntax error at line {}: expected an int or an identifier for <{}>'s {}<|> operator"sv, ts.line, id, decor);
								return false;
							}
							valueStr += "|" + str.substr(start, ts.index - start + 1);
							aux = ts.index;
							if (!SkipSpace(str, ts) || str[ts.index] != '|') { //Let SkipSpace failure be handled later
								ts.index = aux;
								break;
							}
						}
					}
					else {
						ts.index = aux;
					}
				}
				else if (type == 'flt') {
					aux = ts.index;
					if (!ReadFloat(str, ts) && !ReadIdentifier(str, ts)) {
						logger.Error("Syntax error at line {}: expected a float or an identifier for <{}>'s {}value"sv, ts.line, id, decor);
						return false;
					}
					valueStr = str.substr(aux, ts.index - aux + 1);
				}
				else if (type == 'str') {
					aux = ts.index;
					if (!ReadString(str, ts) && !ReadIdentifier(str, ts)) {
						logger.Error("Syntax error at line {}: expected a string or an identifier for <{}>'s {}value"sv, ts.line, id, decor);
						return false;
					}
					valueStr = str.substr(aux, ts.index - aux + 1);
				}
				else {
					logger.Error("WTF error at line {}: Here the program was, parsing export lines, when suddenly some type check sucked it in the netherworld. How did you do that?!"sv, ts.line);
					return false;
				}

				return true;
			};
			if (!isdiff) {
				//=
				exportSig += " = ";
				if (str[ts.index] != '=' && (!SkipSpace(str, ts) || str[ts.index] != '=')) {
					logger.Error("Syntax error at line {}: incomplete export declaration <{}>. Expected <=>"sv, ts.line, id);
					return false;
				}

				//Value
				if (!SkipSpace(str, ts)) {
					logger.Error("Syntax error at line {}: incomplete export declaration <{}>. Expected a value"sv, ts.line, id);
					return false;
				}
				string auxS{};
				if (!readValue(auxS, "")) {
					return false;
				}
				exportSig += auxS;
				sigID = string_cache.FindOrAdd(exportSig);
				if (sigID == Cache::NULL_ID) {
					logger.Error("Failed to add <{}>'s signature to cache (at line {})"sv, exportSig, ts.line);
					return false;
				}
			}
			else {
				sigID = sigCmpID;
			}

			//Flags
			NodeFlags flags{};
			if (isdiff) {
				if (str[ts.index] == ' ' && !SkipSpace(str, ts)) {
					logger.Error("Syntax error at line {}: expected <;> or attributes for <{}>"sv, ts.line, id);
					return false;
				}
				if (str[ts.index] == '[' && !ParseAttributes(str, ts, flags)) {
					logger.Error("Syntax error at line {}: invalid export <{}> attributes"sv, ts.line, id);
					return false;
				}
				string exportNewSig{};
				if (flags.Any(Flag::Redefine)) {
					if (!readValue(exportNewSig, "redefine ")) {
						return false;
					}
					sigNewID = string_cache.FindOrAdd(exportNewSig);
					if (sigNewID == Cache::NULL_ID) {
						logger.Error("Failed to add <{}>'s rename/redefine info to cache (at line {})"sv, exportSig, ts.line);
						return false;
					}
				}
			}

			//Export ending ';'
			if (str[ts.index] != ';' && (!SkipSpace(str, ts) || str[ts.index] != ';')) {
				logger.Error("Syntax error at line {}: expected attributes or terminating <;> for <{}>"sv, ts.line, id);
				return false;
			}

			//Export end of line
			bool finishedExport{ false };
			if (!SkipSpace(str, ts)) {
				if (filetype == FileType::def) {
					finishedExport = true;
					ts.index = str.length();
				}
				else {
					logger.Error("Syntax error at line {}: scr files must specify a sub scope (while parsing <{}>)"sv, ts.line, id);
					return false;
				}
			}
			if (!finishedExport && !IsNewlineChar(str[ts.index])) {
				logger.Error("Syntax error at line {}: export declarations must fully occupy their line (while parsing <{}>)"sv, ts.line, id);
				return false;
			}
			++ts.line;
			if (!isdiff || !flags.Only(Flag::Noop)) {
				if (flags.Any(Flag::Delete, Flag::Insert, Flag::Rename)) {
					logger.Error("Invalid export <{}>. Exports cannot be deleted, inserted, or renamed"sv, id);
				}
				flags.Set(Flag::Export);
				if (!PushBackNoEx(GetVec(isdiff), { sigID, sigNewID, sigCmpID, flags, sigCmpID, ts.line })) {
					logger.Error("Unexpected error when trying to store <{}>'s export node data. Possibly out of memory?"sv, id);
					return false;
				}
			}
		}

		return true;
	}
	//Skips ' ' & newline chars if one is pointed at and starts reading signature. Expects any one extra appended char and leaves ts.index pointing to it. 
	[[nodiscard]] bool Parser::GenerateVarlistNodes(const string& str, traversal_state& ts, bool isdiff) noexcept {
		while (true) {
			if (str.empty() || ts.index >= str.length() - 1 || (IsSpaceNewline(str[ts.index]) && !SkipSpaceNewline(str, ts))) {
				return true;
			}

			szt aux{ str.find(')', ts.index) };
			if (aux >= str.length()) {
				logger.Error("Syntax error at line {}: no closing <)> for varlist line"sv, ts.line);
				return false;
			}
			string sig{ str.substr(ts.index, aux - ts.index + 1) };

			int lineType{ (sig.front() == '!' ? 'inc' : (sig.front() == 'V' ? 'var' : 'bad')) };
			switch (lineType) {
			case 'inc':
				if (!FormatAndValidateIncludeSignature(sig)) {
					logger.Error("Syntax error at line {}: invalid !incldue line signature <{}>"sv, ts.line, sig);
					return false;
				}
				break;
			case 'var':
				if (!FormatAndValidateVarDeclSignature(sig)) {
					logger.Error("Syntax error at line {}: invalid variable declaration signature <{}>"sv, ts.line, sig);
					return false;
				}
				break;
			default:
				logger.Error("Syntax error at lie {}: invalid varlist line"sv);
				return false;
			}
			ts.index = aux;

			uint32 sID{ string_cache.FindOrAdd(sig) };
			if (sID == Cache::NULL_ID) {
				logger.Error("Failed to add <{}> to cache"sv, sig);
				return false;
			}

			//Attributes
			NodeFlags flags{};
			uint32 nsID{ Cache::NULL_ID };
			if (isdiff) {
				aux = ts.index;
				if (SkipSpace(str, ts) && str[ts.index] == '[') {
					if (!ParseAttributes(str, ts, flags)) { //Will work even if str ends in an attribute because we appended a '_'
						logger.Error("Syntax error at line {}: bad attributes of varlist line"sv, ts.line);
						return false;
					}
					flags.Unset(Flag::Redefine);
					if (flags.Any(Flag::Rename)) {
						if (lineType == 'inc') {
							szt open{ ts.index };
							if (!ReadString(str, ts)) {
								logger.Error("Syntax error at line {}: bad rename string of !include declaration"sv, ts.line);
								return false;
							}
							string newsig{ "!include(" + str.substr(open, ts.index - open + 1) + ')' };
							nsID = string_cache.FindOrAdd(newsig);
							if (nsID == Cache::NULL_ID) {
								logger.Error("Failed to add <{}> !include rename signature to cache"sv, newsig);
								return false;
							}
						}
						else {
							if (ts.index == str.length() - 1) { //Pointing to appended '_'
								logger.Error("Syntax error at line {}: varlist line has [rename] attribute but no rename signature follows"sv, ts.line);
								return false;
							}
							szt aux2{ ts.index };
							ts.index = str.find(')', ts.index);
							if (ts.index >= str.length()) {
								logger.Error("Syntax error at line {}: rename signature missing <)>"sv, ts.line);
								return false;
							}
							string newsig{ str.substr(aux2, ts.index - aux2 + 1) };
							if (!FormatAndValidateVarDeclSignature(newsig)) {
								logger.Error("Syntax error at line {}: invalid variable declaration <{}> rename signature"sv, ts.line, sig);
								return false;
							}
							nsID = string_cache.FindOrAdd(newsig);
							if (nsID == Cache::NULL_ID) {
								logger.Error("Failed to add <{}> declaration's rename signature to cache"sv, newsig);
								return false;
							}
						}
					}
					else {
						--ts.index; //Make sure we're not pointing at the '_'
					}
				}
				else {
					ts.index = aux;
				}
			}

			if (lineType == 'inc') {
				if (flags.Any(Flag::Delete, Flag::Insert, Flag::Redefine)) {
					logger.Error("Invalid include <{}>. Includes cannot be deleted, inserted, or redefined"sv, sig);
				}
			}
			else {
				if (flags.Any(Flag::Insert)) {
					logger.Error("Invalid variable declaration <{}>. Variable declarations cannot be inserted"sv, sig);
				}
			}
			flags.Set((lineType == 'inc' ? Flag::Include : Flag::Vardecl));
			if (!PushBackNoEx(GetVec(isdiff), { sID, nsID, sID, flags, sID, ts.line })) {
				logger.Error("Failed to store <{}> varlist node. Possibly out of memory?"sv);
				return false;
			}

			//End of file?
			traversal_state ts2{ ts };
			if (!SkipSpaceNewline(str, ts)) {
				logger.Error("Unexpected error while parsing <{}> at line {}. Reached end of file. Appended '_' missing?"sv, sig, ts.line);
				return false;
			}
			if (ts.index == str.length() - 1) { //'_' index
				return true; //End of file
			}

			//End of line
			ts = ts2;
			if (!SkipSpace(str, ts) || !IsNewlineChar(str[ts.index])) {
				logger.Error("Syntax error at line {}: varlist lines must fully occupy their line"sv, ts.line);
				return false;
			}
		}
	}
	//Skips ' ' & newline chars and starts reading signature. Leaves ts.index pointing to the closing '}'
	[[nodiscard]] bool Parser::GenerateScopeNodes(Node& parent_node, const string& str, traversal_state& ts, bool isdiff) noexcept {
		while (true) {
			if (!SkipSpaceNewline(str, ts)) {
				logger.Error("Syntax error: unexpected end of file"sv);
				return false;
			}

			if (str[ts.index] == '}') {
				return true; //End of scope
			}
			if (!IsIdentifierChar(str[ts.index])) {
				logger.Error("Syntax error at line {}: expected identifier start, instead read <{}>"sv, ts.line, str[ts.index]);
				return false;
			}

			//Signature
			bool isUseStatement{ false };
			szt aux{ str.find(')', ts.index) };
			if (aux >= str.length()) {
				logger.Error("Syntax error at line {}: invalid signature for child of <{}>"sv, ts.line, string_cache.Find(parent_node.GetSigID()));
				return false;
			}
			string sigStr{ str.substr(ts.index, aux - ts.index + 1u) };
			
			ts.index = aux;
			aux = ts.line; //sig line in sourcefile
			if (sigStr.substr(0, 4) == "use ") {
				if (!FormatAndValidateUseSignature(sigStr)) {
					logger.Error("Syntax error at line {}: invalid use statement signature <{}>"sv, aux, sigStr);
					return false;
				}
				isUseStatement = true;
			}
			else if (!FormatAndValidateFuncSignature(sigStr)) {
				logger.Error("Syntax error at line {}: invalid function signature <{}>"sv, aux, sigStr);
				return false;
			}
			
			uint32 sigID = string_cache.FindOrAdd(sigStr);
			if (sigID == Cache::NULL_ID) {
				logger.Error("Failed to add <{}>'s signature to cache"sv, sigStr);
				return false;
			}
			uint32 newsigID{ Cache::NULL_ID };

			//Flags
			if (!SkipSpace(str, ts)) {
				logger.Error("Syntax error at line {}: expected function attributes, function end, or start of function scope"sv, ts.line);
				return false;
			}
			NodeFlags flags{};
			if (isdiff) {
				if (!ParseAttributes(str, ts, flags)) {
					logger.Error("Syntax error at line {}: invalid function attributes"sv, ts.line);
					return false;
				}
				if (flags.Any(Flag::Rename)) {
					if (!IsIdentifierChar(str[ts.index])) {
						logger.Error("Syntax error at line {}: <{}> has [rename] attribute but no valid signature identifier follows"sv, ts.line, sigStr);
						return false;
					}
					szt endpos{ str.find(')', ts.index) };
					if (endpos >= str.length()) {
						logger.Error("Syntax error at line {}: <{}> has [rename] attribute but following signature is missing parens"sv, ts.line, sigStr);
						return false;
					}
					string newsigStr{ str.substr(ts.index, endpos - ts.index + 1u) };
					ts.index = endpos;
					if (!isUseStatement) {
						if (!FormatAndValidateFuncSignature(newsigStr)) {
							logger.Error("Syntax error at line {}: <{}> has [rename] attribute but no valid function signature follows"sv, ts.line, sigStr);
							return false;
						}
					}
					else if (!FormatAndValidateUseSignature(newsigStr)) {
						logger.Error("Syntax error at line {}: <{}> has [rename] attribute but no valid use statement signature follows"sv, ts.line, sigStr);
						return false;
					}
					newsigID = string_cache.FindOrAdd(newsigStr);
					if (newsigID == Cache::NULL_ID) {
						logger.Error("Failed to add <{}>'s rename signature to cache"sv, newsigStr);
						return false;
					}

				}
			}

			if (str[ts.index] != ';' && str[ts.index] != '{') {
				if (!SkipSpaceNewline(str, ts)) {
					logger.Error("Syntax error at line {}: unexpected end of file while parsing <{}>"sv, ts.line, sigStr);
					return false;
				}
				if (str[ts.index] != ';' && str[ts.index] != '{') {
					logger.Error("Syntax error at line {}: expected function end or start of new scope but read <{}> while parsing <{}>"sv, ts.line, str[ts.index], sigStr);
					return false;
				}
			}
			
			//End or scope
			if (isdiff && flags.None()) { flags.Set(Flag::Noop); }
			if (str[ts.index] == ';') {
				if (!flags.Only(Flag::Noop, Flag::Redefine)) {
					flags.Set(isUseStatement ? Flag::Use : Flag::Function);
					if (!parent_node.AddSubnode({ sigID, newsigID, sigID, flags, sigID, ts.line })) {
						logger.Error("Failed to add subnode <{}> to <{}>"sv, sigStr, string_cache.Find(parent_node.GetSigID()));
						return false;
					}
				}
				continue; //Child signature end
			}
			else if (str[ts.index] == '{') {
				if (isUseStatement) {
					logger.Error("Use statements cannot have scope (at line {})"sv, aux);
					return false;
				}
				flags.Set(isUseStatement ? Flag::Use : Flag::Function);
				Node& child_node = parent_node.AddAndReturnSubnode({ sigID, newsigID, sigID, flags, sigID, ts.line });
				if (!GenerateScopeNodes(child_node, str, ts, isdiff)) {
					logger.Error("Failed to parse children of <{}> at line {}"sv, string_cache.Find(child_node.GetSigID()), ts.line);
					return false;
				}
				continue; //Child children parsing end
			}
			else {
				logger.Error("Reached unreachable code in GenerateScopeNodes() while parsing <{}>"sv, sigStr);
				return false;
			}

		}
	}

	[[nodiscard]] bool Parser::IdentifyAttribute(const string& str, Flag& out) const noexcept {
		//Order by probable order of commonness
		if (StrICmp(str.c_str(), RENAME_TAG)) { out = Flag::Rename; return true; }
		if (StrICmp(str.c_str(), REDEFINE_TAG)) { out = Flag::Redefine; return true; }
		if (StrICmp(str.c_str(), INSERT_TAG)) { out = Flag::Insert; return true; }
		if (StrICmp(str.c_str(), DELETE_TAG)) { out = Flag::Delete; return true; }
		if (StrICmp(str.c_str(), NOOP_TAG)) { out = Flag::Noop; return true; }

		logger.Error("Unrecognized attribute <{}>"sv, str);
		return false;
	}
	//Expects ts.index pointing to '[', and leaves ts.index pointing to the first non ' ', non '\0' char after last ']'
	[[nodiscard]] bool Parser::ParseAttributes(const string& str, StringUtils::traversal_state& ts, NodeFlags& out) const noexcept {
		if (ts.index >= str.length())
			return false;

		out.Clear();
		while (str[ts.index] == '[') {
			szt endpos = str.find(']', ts.index);
			if (endpos >= str.length())
				return false;

			Node::Flag cur_op{ Flag::Noop };
			if (!IdentifyAttribute(str.substr(ts.index + 1u, endpos - ts.index - 1u), cur_op))
				return false;
			out.Set(cur_op);

			ts.index = endpos;
			if (!SkipSpace(str, ts)) { //Find first non ' ' char after last ']'
				return false; //str ends in a valid attribute
			}
		}

		return true; //Move past the sequence of attributes
	}

	//Tree parsing

	bool OrderNodesOfType(vector<Node>& vec, const vector<Node>& base, Flag type) {
		if (vec.empty() || base.empty()) {
			return true;
		}
		if (type < Flag::Import || type > Flag::Vardecl) {
			logger.Error("Bad order flag <{}>"sv, static_cast<uint32>(type));
			return false;
		}
		uint32 order{ 0 };
		map<uint32, uint32> baseorder{};
		//Get base order
		{
			auto baseFirstNode{ base.cend() };
			for (auto it{ base.cbegin() }; it != base.cend(); ++it) {
				if (it->Any(type)) {
					baseFirstNode = it;
					break;
				}
			}
			if (baseFirstNode != base.end()) {
				for (auto it{ baseFirstNode }; it != base.cend(); ++it) {
					if (it->Any(type)) {
						if (!(baseorder.insert({ it->GetOrdersigID(), order++ })).second) {
							logger.Warning("Failed to store base ordering to map. Possibly out of memory?");
							return false;
						}
					}
				}
			}
		}
		if (baseorder.empty()) {
			return true; //No nodes of type in target so don't order
		}

		auto firstNode{ vec.end() };
		for (auto it{ vec.begin() }; it != vec.end(); ++it) {
			if (it->Any(type)) {
				firstNode = it;
				break;
			}
		}
		if (firstNode == vec.end()) {
			return true; //Nothing to order
		}
		auto lastNode{ firstNode };
		order = 0;
		//Order the new nodes first and find last node
		for (auto node{ firstNode }; node != vec.end(); ++node) {
			if (!node->Any(type)) {
				break;
			}
			lastNode = node;
			if (!baseorder.contains(node->GetOrdersigID())) {
				node->SetOrder(order++);
			}
		}
		if (firstNode == lastNode) {
			return true; //No pointing ordering 1 node
		}
		//Order the remaining nodes in order of base but after new nodes
		for (auto node{ firstNode }; node <= lastNode; ++node) {
			if (auto it = baseorder.find(node->GetOrdersigID()); it != baseorder.end()) {
				node->SetOrder(it->second + order);
			}
		}
		//Sort
		auto postLast = lastNode;
		++postLast;
		std::sort(firstNode, postLast, [](const Node& lhs, const Node& rhs) noexcept { return lhs.GetOrder() < rhs.GetOrder(); });
		return true;
	}
	void SegregateNodesOfTypes(vector<Node>& vec, Flag firstType, Flag secondType) noexcept {
		if (vec.empty()) {
			return;
		}

		uint32 order{ 0 };
		//Put nodes of first type first
		for (auto& node : vec) {
			if (node.Any(firstType)) {
				node.SetOrder(order++);
			}
		}
		//Put nodes of second type second
		for (auto& node : vec) {
			if (node.Any(secondType)) {
				node.SetOrder(order++);
			}
		}
		//Put other nodes last
		for (auto& node : vec) {
			if (!node.Any(firstType, secondType)) {
				node.SetOrder(order++);
			}
		}

		std::sort(vec.begin(), vec.end(), [](const Node& lhs, const Node& rhs) noexcept { return lhs.GetOrder() < rhs.GetOrder();  });
		return;
	}

	bool Node::SegregateAndOrderSubnodes(const vector<Node>& otherNodes, const Cache& string_cache) noexcept {
		SegregateNodesOfTypes(subnodes, Flag::Use, Flag::Function);
		try {
			if (!OrderNodesOfType(subnodes, otherNodes, Flag::Use) || !OrderNodesOfType(subnodes, otherNodes, Flag::Function)) {
				logger.Error("Failed to order <{}>"sv, string_cache.Find(sigID));
				return false;
			}
			return true;
		}
		catch (...) {
			logger.Error("Exception ordering <{}>"sv, string_cache.Find(sigID));
			return false;
		}
	}

	bool OrderFunctionNode(Node& node, const Node& other, const Cache& string_cache) {
		if (node.GetNumSubnodes() == 0) {
			return true;
		}
		
		if (!node.SegregateAndOrderSubnodes(other.GetSubnodesRef(), string_cache)) {
			logger.Error("Failed to order funtion contents. Possibly out of memory?"sv);
			return false;
		}

		return true;
	}

	
	[[nodiscard]] bool Parser::ParseScrLoot(string& out) {
		if (diff.empty() || target.empty()) {
			logger.Error("Parsing error: parse requested but diff and target have not both been provided"sv);
			return false;
		}

		vector<Node> result{};
		result.reserve(target.size());
		set<szt> usedTargetIndexes{};
		bool importsDone{ false }, exportsDone{ false };
		for (const auto& dNode : diff) {
			//Append all import lines intact from target that weren't handled before moving to exports
			if (!importsDone && !dNode.Any(Flag::Import)) {
				for (szt i{ 0u }; i < target.size(); ++i) {
					if (!target[i].Any(Flag::Import)) {
						break;
					}
					else if (!usedTargetIndexes.contains(i) && !PushBackNoEx(result, target[i])) {
						logger.Error("Unexpected error when trying to store unmodified import node parsed data. Possibly out of memory? (while parsing <{}> at line {})"sv, CacheFindSig(target[i]), target[i].GetSourceLine());
						return false;
					}
					usedTargetIndexes.insert(i);
				}
				importsDone = true;
			}
			//Append all export lines intact from target that weren't handled before moving to sub scope
			if (!exportsDone && importsDone && !dNode.Any(Flag::Export)) {
				for (szt i{ 0u }; i < target.size(); ++i) {
					if (usedTargetIndexes.contains(i)) {
						continue;
					}
					else if (!target[i].Any(Flag::Export)) {
						break;
					}
					else {
						if (!PushBackNoEx(result, target[i])) {
							logger.Error("Unexpected error when trying to store unmodified export node parsed data. Possibly out of memory? (while parsing <{}> at line {})"sv, CacheFindSig(target[i]), target[i].GetSourceLine());
							return false;
						}
					}
					usedTargetIndexes.insert(i);
				}
				exportsDone = true;
			}

			//Delete is handled implicitly by not pushing anything back to result
			if (dNode.Any(Flag::Insert)) { //Insert
				if (!PushBackNoEx(result, dNode)) {
					logger.Error("Unexpected error when trying to store insert node parsed data. Possibly out of memory? (while parsing <{}> at line {})"sv, CacheFindSig(dNode), dNode.GetSourceLine());
					return false;
				}
			}
			else {
				szt tIdx{ 0u };
				bool handled{ false };
				for (const auto& tNode : target) {
					if (dNode.GetComparesigID() == tNode.GetComparesigID()) {
						if (usedTargetIndexes.contains(tIdx)) {
							logger.Error("Error in file <{}>: <{}> was already operated on"sv, target_path, CacheFindSig(tNode));
							return false;
						}
						if (!dNode.Any(Flag::Delete)) {
							Node rNode{};
							if (!ParseNode(dNode, tNode, rNode)) { //Rename / Redefine
								logger.Error("Parsing error: HANDLE BAD XDDD"sv);
								return false;
							}
							rNode.SetOrderSigID(dNode.GetOrdersigID());
							if (!PushBackNoEx(result, std::move(rNode))) {
								logger.Error("Unexpected error when trying to store node parsed data. Possibly out of memory? (while parsing <{}> at line {})"sv, CacheFindSig(dNode), dNode.GetSourceLine());
								return false;
							}
						}
						handled = true;
						usedTargetIndexes.insert(tIdx);
						break;
					}
					++tIdx;
				}
				if (!handled) {
					if (dNode.Any(Flag::Delete)) {
						logger.Warning("Parsing warning at line {}: node <{}> marked for deletion not found in target but this doesn't affect the output so parsing will continue"sv, dNode.GetSourceLine(), CacheFindSig(dNode));
					}
					else {
						logger.Error("Parsing error at line {}: node <{}> not found in targetSL"sv, dNode.GetSourceLine(), CacheFindSig(dNode));
						return false;
					}
				}
			}
		}
		//Append all sub declaration lines from target that weren't handled		Loot only thing
		for (szt i{ 0u }; i < target.size(); ++i) {
			if (target[i].Any(Flag::SubDeclaration) && !usedTargetIndexes.contains(i) && !PushBackNoEx(result, target[i])) {
				logger.Error("Unexpected error when trying to store unmodified sub declaration node parsed data. Possibly out of memory? (while parsing <{}> at line {})"sv, CacheFindSig(target[i]), target[i].GetSourceLine());
				return false;
			}
		}

		if (!OrderNodesOfType(result, target, Flag::Import) || !OrderNodesOfType(result, target, Flag::Export) || !OrderNodesOfType(result, target, Flag::SubDeclaration)) {
			logger.Error("Failed to order scr/loot contents. Possibly out of memory?"sv);
			return false;
		}

		vector<string> resultStr(result.size());
		for (szt i{ 0u }; i < result.size(); ++i) {
			resultStr[i] = result[i].ToString(0, string_cache);
		}
		out = Join(resultStr, '\n');
		return true;
	}

	[[nodiscard]] bool Parser::ParseDef(string& out) {
		if (diff.empty() || target.empty()) {
			logger.Error("Parsing error: parse requested but diff and target have not both been provided"sv);
			return false;
		}

		vector<Node> result{};
		result.reserve(target.size());
		set<szt> usedTargetIndexes{};
		for (const auto& dNode : diff) {
			//Noop and Delete are handled implicitly by not pushing anything back to result
			if (dNode.Any(Flag::Insert)) { //Insert
				if (!PushBackNoEx(result, dNode)) {
					logger.Error("Unexpected error when trying to store insert node parsed data. Possibly out of memory? (while parsing <{}> at line {})"sv, CacheFindSig(dNode), dNode.GetSourceLine());
					return false;
				}
			}
			else {
				szt tIdx{ 0u };
				bool handled{ false };
				for (const auto& tNode : target) {
					if (dNode.GetComparesigID() == tNode.GetComparesigID()) {
						if (usedTargetIndexes.contains(tIdx)) {
							logger.Error("Error in file <{}>: <{}> was already operated on"sv, target_path, CacheFindSig(tNode));
							return false;
						}
						if (!dNode.Any(Flag::Delete)) {
							Node rNode{};
							if (!ParseNode(dNode, tNode, rNode)) { //Rename / Redefine
								logger.Error("Parsing error: HANDLE BAD XDDD"sv);
								return false;
							}
							rNode.SetOrderSigID(dNode.GetOrdersigID());
							if (!PushBackNoEx(result, std::move(rNode))) {
								logger.Error("Unexpected error when trying to store node parsed data. Possibly out of memory? (while parsing <{}> at line {})"sv, CacheFindSig(dNode), dNode.GetSourceLine());
								return false;
							}
						}
						handled = true;
						usedTargetIndexes.insert(tIdx);
						break;
					}
					++tIdx;
				}
				if (!handled) {
					if (dNode.Any(Flag::Delete)) {
						logger.Warning("Parsing warning at line {}: node <{}> marked for deletion not found in target but this doesn't affect the output so parsing will continue"sv, dNode.GetSourceLine(), CacheFindSig(dNode));
					}
					else {
						logger.Error("Parsing error at line {}: node <{}> not found in targetD"sv, dNode.GetSourceLine(), CacheFindSig(dNode));
						return false;
					}
				}
			}
		}
		//Append all export lines from target that weren't handled
		for (szt i{ 0u }; i < target.size(); ++i) {
			if (!usedTargetIndexes.contains(i) && !PushBackNoEx(result, target[i])) {
				logger.Error("Unexpected error when trying to store unmodified export node parsed data. Possibly out of memory? (while parsing <{}> at line {})"sv, CacheFindSig(target[i]), target[i].GetSourceLine());
				return false;
			}
		}


		if (!OrderNodesOfType(result, target, Flag::Export)) {
			logger.Error("Failed to order def contents. Possibly out of memory?"sv);
			return false;
		}

		vector<string> resultStr(result.size());
		for (szt i{ 0u }; i < result.size(); ++i) {
			resultStr[i] = result[i].ToString(0, string_cache);
		}
		out = Join(resultStr, '\n');
		return true;
	}

	[[nodiscard]] bool Parser::ParseVarlist(string& out) {
		if (diff.empty() || target.empty()) {
			logger.Error("Parsing error: parse requested but diff and target have not both been provided"sv);
			return false;
		}

		vector<Node> result{};
		result.reserve(target.size());
		set<szt> usedTargetIndexes{};
		for (const auto& dNode : diff) {
			//Noop and Delete are handled implicitly by not pushing anything back to result
			if (dNode.Any(Flag::Insert)) { //Insert
				if (!PushBackNoEx(result, dNode)) {
					logger.Error("Unexpected error when trying to store insert node parsed data. Possibly out of memory? (while parsing <{}> at line {})"sv, CacheFindSig(dNode), dNode.GetSourceLine());
					return false;
				}
			}
			else {
				szt tIdx{ 0u };
				bool handled{ false };
				for (const auto& tNode : target) {
					if (dNode.GetComparesigID() == tNode.GetComparesigID()) {
						if (usedTargetIndexes.contains(tIdx)) {
							logger.Error("Error in file <{}>: <{}> was already operated on"sv, target_path, CacheFindSig(tNode));
							return false;
						}
						if (!dNode.Any(Flag::Delete)) {
							Node rNode{};
							if (!ParseNode(dNode, tNode, rNode)) { //Rename / Redefine
								logger.Error("Parsing error: HANDLE BAD XDDD"sv);
								return false;
							}
							rNode.SetOrderSigID(dNode.GetOrdersigID());
							if (!PushBackNoEx(result, std::move(rNode))) {
								logger.Error("Unexpected error when trying to store node parsed data. Possibly out of memory? (while parsing <{}> at line {})"sv, CacheFindSig(dNode), dNode.GetSourceLine());
								return false;
							}
						}
						handled = true;
						usedTargetIndexes.insert(tIdx);
						break;
					}
					++tIdx;
				}
				if (!handled) {
					if (dNode.Any(Flag::Delete)) {
						logger.Warning("Parsing warning at line {}: node <{}> marked for deletion not found in target but this doesn't affect the output so parsing will continue"sv, dNode.GetSourceLine(), CacheFindSig(dNode));
					}
					else {
						logger.Error("Parsing error at line {}: node <{}> not found in targetV"sv, dNode.GetSourceLine(), CacheFindSig(dNode));
						return false;
					}
				}
			}
		}
		//Append all varlist lines from target that weren't handled
		for (szt i{ 0u }; i < target.size(); ++i) {
			if (target[i].Any(Flag::Include, Flag::Vardecl) && !usedTargetIndexes.contains(i) && !PushBackNoEx(result, target[i])) {
				logger.Error("Unexpected error when trying to store unmodified varlist node parsed data. Possibly out of memory? (while parsing <{}> at line {})"sv, CacheFindSig(target[i]), target[i].GetSourceLine());
				return false;
			}
		}

		SegregateNodesOfTypes(result, Flag::Include, Flag::Vardecl);
		if (!OrderNodesOfType(result, target, Flag::Include) || !OrderNodesOfType(result, target, Flag::Vardecl)) {
			logger.Error("Failed to order varlist contents. Possibly out of memory?"sv);
			return false;
		}

		vector<string> resultStr(result.size());
		for (szt i{ 0u }; i < result.size(); ++i) {
			resultStr[i] = result[i].ToString(0, string_cache);
		}
		out = Join(resultStr, '\n');
		return true;
	}

	[[nodiscard]] bool Parser::ParseNode(const Node& dNode, const Node& tNode, Node& rNode) {

		//Being here means dNode has Noop AND/OR Rename AND/OR Redefine but NOT Insert OR Delete, and that tNode is its match

		rNode.SetComparesigID(dNode.GetComparesigID());
		rNode.SetOrderSigID(dNode.GetOrdersigID());

		if (dNode.Any(Flag::Rename)) {
			rNode.SetSigID(dNode.GetNewsigID());
		}
		else {
			rNode.SetSigID(tNode.GetSigID());
		}

		if (dNode.Any(Flag::Redefine)) {
			if (dNode.Any(Flag::Export)) {
				string newsig{ string_cache.Find(tNode.GetComparesigID()) + " = " + string_cache.Find(dNode.GetNewsigID()) };
				uint32 id = string_cache.FindOrAdd(newsig);
				if (id == Cache::NULL_ID) {
					logger.Error("Failed to add line {}'s <{}>'s recreated signature to cache while trying to rename"sv, dNode.GetSourceLine(), CacheFindSig(dNode));
					return false;
				}
				rNode.SetSigID(id);
			}
			else if (dNode.Any(Flag::SubScope, Flag::SubDeclaration, Flag::Function)) {
				if (!rNode.SetSubnodes(dNode.GetSubnodes())) {
					logger.Error("Parser error at line {}: unexpectedly failed to redefine <{}>'s subnodes"sv, dNode.GetSourceLine(), CacheFindSig(dNode));
					return false;
				}
			}
			else {
				logger.Warning("Parser warning at line {}: import, use, and varlist declarations cannot be redefined. Operation skipped. Did you mean to rename?"sv, dNode.GetSourceLine());
			}
			rNode.SetFlags(dNode.GetFlags());
		}
		else {
			//No Redefine so handle dNode's children individualy like in ParseScrLoot
			set<szt> usedTargetIndexes{};
			for (NodeCIterator dChild{ dNode.CBegin() }; dChild != dNode.CEnd(); ++dChild) {
				if (dChild->Any(Flag::Insert)) {
					if (!rNode.AddSubnode(*dChild)) {
						logger.Error("Unexpected error when trying to store child insert node parsed data. Possibly out of memory? (while parsing <{}> at line {})"sv, CacheFindSig(*dChild), dChild->GetSourceLine());
						return false;
					}
				}
				else {
					szt tIdx{ 0u };
					bool handled{ false };
					for (NodeCIterator tChild{ tNode.CBegin() }; tChild != tNode.CEnd(); ++tChild) {
						if (dChild->GetComparesigID() == tChild->GetComparesigID()) {
							if (usedTargetIndexes.contains(tIdx)) {
								logger.Error("Error in file <{}>: <{}> was already operated on"sv, target_path, CacheFindSig(*tChild));
								return false;
							}
							if (!dChild->Any(Flag::Delete)) {
								Node rChild{};
								if (!ParseNode(*dChild, *tChild, rChild)) { //Rename / Redefine
									logger.Error("Parsing error: HANDLE BAD XDDD"sv);
									return false;
								}
								rChild.SetOrderSigID(dChild->GetOrdersigID());
								if (!rNode.AddSubnode(std::move(rChild))) {
									logger.Error("Unexpected error when trying to store child node parsed data. Possibly out of memory? (while parsing <{}> at line {})"sv, CacheFindSig(*dChild), dChild->GetSourceLine());
									return false;
								}
							}
							handled = true;
							usedTargetIndexes.insert(tIdx);
							break;
						}
						++tIdx;
					}
					if (!handled) {
						if (dChild->Any(Flag::Delete)) {
							logger.Warning("Parsing warning at line {}: node <{}> marked for deletion not found in target but this doesn't affect the output so parsing will continue"sv, dChild->GetSourceLine(), CacheFindSig(*dChild));
						}
						else {
							logger.Error("Parsing error at line {}: node <{}> not found in targetN"sv, dChild->GetSourceLine(), CacheFindSig(*dChild));
							return false;
						}
					}
				}
			}
			//Append all lines from tNode that weren't handled
			szt tIdx{ 0u };
			for (NodeCIterator tChild{ tNode.CBegin() }; tChild != tNode.CEnd(); ++tChild) {
				if (!usedTargetIndexes.contains(tIdx) && !rNode.AddSubnode(*tChild)) {
					logger.Error("Unexpected error when trying to store unmodified node parsed data. Possibly out of memory? (while parsing <{}> at line {})"sv, CacheFindSig(*tChild), tChild->GetSourceLine());
					return false;
				}
				++tIdx;
			}
			rNode.SetFlags(tNode.GetFlags());
		}



		return OrderFunctionNode(rNode, tNode, string_cache);
	}

	

	[[nodiscard]] const string& Parser::CacheFindSig(const Node& node) const noexcept { return string_cache.Find(node.GetSigID()); }
	[[nodiscard]] const string& Parser::CacheFind(uint32 id) const noexcept { return string_cache.Find(id); }



	void Parser::ResetImpl() noexcept {
		diff.clear();
		target.clear();
		string_cache.Reset();
	}
	void Parser::HandleResets(bool isdiff) noexcept {
		if (isdiff) {
			ResetImpl();
		}
		else {
			target.clear();
		}
	}

}



