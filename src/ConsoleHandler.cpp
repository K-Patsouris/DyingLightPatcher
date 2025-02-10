#include "ConsoleHandler.h"
#include "Common.h"
#include "Containers.h"
#include "logger.h"
#include "FileManager.h"

#include <atomic>		//atomic<bool> active
#include <windows.h>	//All console stuff
#include <winuser.h>	//messageboxes
#include <conio.h>		//_getch()
#include <iostream>		//std::cout and std::cin
#include <iomanip>		//timestamps
#include <ctime>		//timestamps
#include <sstream>		//std::stringstream as std::put_time handler


using std::cout;
using std::cin;
using std::flush;

using MessageColor = MessageQueue::MessageColor;
using msg_type = std::pair<string, MessageColor>;
using queue_type = deque<msg_type>;

constexpr string PREFIX_EMPTY{ "   " };
constexpr string PREFIX_POINT{ " > " };


std::atomic<bool> active{ false };

const array<string, 8> base{			//Base message for each line
	"Diff directory: ",					//0
	".pak directory: ",					//1
	"Parse without committing",			//2
	"Parse and commit",					//3
	"Commit parsed files",				//4
	"Reset Program",					//5
	"Clear Console",					//6
	"Close",							//7
};
array<string, base.size()> prefixes{	//Prefix for each line's message. Used for selection indicator.
	PREFIX_POINT,
	PREFIX_EMPTY,
	PREFIX_EMPTY,
	PREFIX_EMPTY,
	PREFIX_EMPTY,
	PREFIX_EMPTY,
	PREFIX_EMPTY,
	PREFIX_EMPTY,
};	
array<string, base.size()> suffixes{};	//Suffix for each line's message. Used for dirs.
szt pos{ 0 };							//Position of selected line. Used to set pointy prefix and many other things like cleansing text.
string infoline{};						//Extra line at the bottom for info etc

const array<string, 10> INFOLINE_MSGS{
	"The directory containing the diffs. Press Enter to change.",							//0
	"The directory containing the target .pak files. Press Enter to change.",				//1
	"Press Enter to generate the parsed files, without committing them to their .pak file.",//2
	"Press Enter to generate the parsed files and commit them to their .pak file.",			//3
	"Press Enter to commit previously generated parsed files to their .pak file.",			//4
	"Press Enter to reset the directories and the loaded/generated files.",					//5
	"Press Enter to clear the console.",													//6
	"Press Enter to close the program.",													//7
	"New directory:",																		//8
	"Press any key to exit...",																//9
};
enum InfolineIdxs : szt {
	NewDirIdx = base.size(),
	CloseIdx,
};

queue_type message_queue{};			//Message queue for additional messages like logger outputs
MessageQueue* mqPtr = nullptr;
FileManager file_manager{};

void (*GetMQ)(queue_type& q) noexcept = [](queue_type& q) noexcept { q.clear(); };




namespace ConsoleUtils {
	enum class Key : int {
		//Numbers
		Num0			= 48,
		Num1			= 49,
		Num2			= 50,
		Num3			= 51,
		Num4			= 52,
		Num5			= 53,
		Num6			= 54,
		Num7			= 55,
		Num8			= 56,
		Num9			= 57,
		//Lower case
		a				= 97,
		b				= 98,
		c				= 99,
		d				= 100,
		e				= 101,
		f				= 102,
		g				= 103,
		h				= 104,
		i				= 105,
		j				= 106,
		k				= 107,
		l				= 108,
		m				= 109,
		n				= 110,
		o				= 111,
		p				= 112,
		q				= 113,
		r				= 114,
		s				= 115,
		t				= 116,
		u				= 117,
		v				= 118,
		w				= 119,
		x				= 120,
		y				= 121,
		z				= 122,
		//Uppe case
		A				= 65,
		B				= 66,
		C				= 67,
		D				= 68,
		E				= 69,
		F				= 70,
		G				= 71,
		H				= 72,
		I				= 73,
		J				= 74,
		K				= 75,
		L				= 76,
		M				= 77,
		N				= 78,
		O				= 79,
		P				= 80,
		Q				= 81,
		R				= 82,
		S				= 83,
		T				= 84,
		U				= 85,
		V				= 86,
		W				= 87,
		X				= 88,
		Y				= 89,
		Z				= 90,
		//Symbols
		Space			= 32,	//  <--There is a space here!
		Tilde			= 126,	// ~
		Exclam			= 33,	// !
		At				= 64,	// @
		Hash			= 35,	// #
		Dollar			= 36,	// $
		Percent			= 37,	// %
		Caret			= 94,	// ^
		Ampersand		= 38,	// &
		ParenOpen		= 40,	// (
		ParenClose		= 41,	// )
		Minus			= 45,	// -
		Equals			= 61,	// =
		Underscore		= 95,	// _
		Plus			= 43,	// + 
		BracketOpen		= 91,	// [
		BracketClose	= 93,	// ]
		BraceOpen		= 123,	// {
		BraceClose		= 125,	// }
		BSlash			= 92,	// \ dummy to not comment below line xd
		Semicolon		= 59,	// ;
		Colon			= 58,	// :
		Comma			= 44,	// ,
		SingleQuote		= 39,	// '
		DoubleQuote		= 34,	// "
		Dot				= 46,	// .
		FSlash			= 47,	// /
		//Controls
		Enter			= 13,
		Escape			= 27,
		Backspace		= 8,
		Precursor		= 224,				//Special precursor char that gets sent before the following keys with a single keypress
		Del				= Precursor + 83,	//Precursored
		Up				= Precursor + 72,	//Precursored
		Left			= Precursor + 75,	//Precursored
		Right			= Precursor + 77,	//Precursored
		Down			= Precursor + 80,	//Precursored
		Home			= Precursor + 71,	//Precursored
		End				= Precursor + 79,	//Precursored
		
	};

	CONSOLE_SCREEN_BUFFER_INFO CSBI;
	HANDLE STD_OUT;
	COORD COORD_ZERO;
	COORD COORD_INFOLINE;
	COORD COORD_QUEUE;

	bool InitConsoleInternals() noexcept {
		STD_OUT = GetStdHandle(STD_OUTPUT_HANDLE); //Exception safe?
		if (STD_OUT == nullptr) {
			logger.Error("Failed to get STD_OUTPUT_HANDLE"sv);
			return false;
		}
		if (!GetConsoleScreenBufferInfo(STD_OUT, &CSBI)) {
			logger.Error("Failed to init ConsoleScreenBufferInfo"sv);
			return false;
		}
		COORD_ZERO = CSBI.dwCursorPosition;
		COORD_INFOLINE = COORD_ZERO;
		COORD_INFOLINE.Y += static_cast<SHORT>(base.size()) + 1;
		COORD_QUEUE = COORD_INFOLINE;
		COORD_QUEUE.Y += 2;
		return true;
	}
	//None of these check for nullptr
	bool UpdateCSBI() noexcept {
		if (!GetConsoleScreenBufferInfo(STD_OUT, &CSBI)) {
			logger.Error("Failed to update ConsoleScreenBufferInfo"sv);
			return false;
		}
		return true;
	}

	bool SetCursorPosition(COORD coord) noexcept {
		if (!SetConsoleCursorPosition(STD_OUT, coord)) {
			logger.Error("Failed to change cursor position!"sv);
			return false;
		}
		return true;
	}
	bool PlaceCursorZero() noexcept { return SetCursorPosition(COORD_ZERO); }
	bool PlaceCursorInfoline() noexcept { return SetCursorPosition(COORD_INFOLINE); }
	bool PlaceCursorQueue() noexcept { return SetCursorPosition(COORD_QUEUE); }
	bool SetConsoleColor(MessageColor color) noexcept {
		if (!SetConsoleTextAttribute(STD_OUT, color)) {
			return false;
		}
		return true;
	}

	bool SetCursorVisibility(bool visible = false) noexcept {
		CONSOLE_CURSOR_INFO cursorInfo;
		if (!GetConsoleCursorInfo(STD_OUT, &cursorInfo)) {
			logger.Error("Could not obtain cursor info"sv);
			return false;
		}
		cursorInfo.bVisible = visible;
		if (!SetConsoleCursorInfo(STD_OUT, &cursorInfo)) {
			logger.Warning("Could not change cursor visibility for unspecified reasons"sv);
			return false;
		}
		return true;
	}
	bool ShowCursor() noexcept { return SetCursorVisibility(true); }
	bool HideCursor() noexcept { return SetCursorVisibility(false); }

	Key ReadKey() noexcept {
		Key key = static_cast<Key>(_getch());
		if (key == Key::Precursor) {
			return static_cast<Key>(_getch() + static_cast<int>(Key::Precursor));
		}
		return key;
	}

	void PrintColoredText(const msg_type& msg, bool color_reset = false) {
		if (color_reset) { UpdateCSBI(); }
		SetConsoleColor(msg.second);
		cout << msg.first;
		if (color_reset) { SetConsoleColor(static_cast<MessageColor>(CSBI.wAttributes)); }
	}

	string GetTimeString() {
		std::time_t t = std::time(nullptr);
		tm* tmPtr = nullptr;
		if (localtime_s(tmPtr, &t) != 0) {
			return string{};
		}
		std::stringstream ss{};
		ss << std::put_time(tmPtr, "%T");
		const string result{ ss.str() };
		return result;
	}
	

	void ExtraLine(const string& msg) { //Prints to the empty line below infoline and returns cursor to old pos. For debug purposes.
		if (!UpdateCSBI()) { return; }
		COORD cur = CSBI.dwCursorPosition;
		COORD elc = COORD_INFOLINE;
		++elc.Y;
		if (!SetCursorPosition(elc)) { return; }
		cout << msg << string(20, ' ') << std::flush;
		SetCursorPosition(cur);
	}


	void Reset() noexcept {
		if (!active.load(std::memory_order_relaxed)) {
			return;
		}
		try {
			prefixes.fill(PREFIX_EMPTY);
			prefixes[0] = PREFIX_POINT;
			suffixes.fill("");
			pos = 0;
			infoline.clear();
		}
		catch (...) {
			return;
		}
	}

	void FlushAndClose() noexcept {
		logger.Close();
		std::terminate();
	}

	void CoutFailAndExit() noexcept {
		try {
			MessageBox(
				NULL,
				(LPCTSTR)L"Failed to stream to std::cout\nThe program will exit",
				(LPCTSTR)L"std::cout failure",
				MB_ICONERROR | MB_OK
			);
		}
		catch (...) {}
		FlushAndClose();
	}
	void StringCtorFailAndExit() noexcept {
		try {
			MessageBox(
				NULL,
				(LPCTSTR)L"Failed to construct string\nThe program will exit",
				(LPCTSTR)L"std::string constructor failure",
				MB_ICONERROR | MB_OK
			);
		}
		catch (...) {}
		FlushAndClose();
	}

	bool ReadDir(szt infoline_idx, string& dir) noexcept {
		dir.clear();
		szt idx{ 0 };
		auto replace_cursor = [&] {
			COORD newcoord = COORD_INFOLINE;
			newcoord.X += static_cast<SHORT>(INFOLINE_MSGS[NewDirIdx].length() + idx);
			SetCursorPosition(newcoord);
		};
		auto clear = [&] {
			if (!SetCursorPosition(COORD_INFOLINE)) { return; }
			try {
				cout << string(INFOLINE_MSGS[NewDirIdx].length() + dir.length(), ' ');
			}
			catch (...) {
				CoutFailAndExit();
			}
		};
		auto print = [&] {
			if (!SetCursorPosition(COORD_INFOLINE)) { return; }
			try {
				cout << INFOLINE_MSGS[infoline_idx] << dir << std::flush;
			}
			catch (...) {
				CoutFailAndExit();
			}
		};
		auto addchar = [&](char c) {
			try {
				string temp{ dir.substr(0, idx) };
				dir = temp + c + dir.substr(idx++);
			}
			catch (...) {
				StringCtorFailAndExit();
			}
			clear();
			print();
			replace_cursor();
		};
		auto delchar = [&](bool backwards = true) {
			if (backwards && idx > 0) {
				try {
					string temp{ dir.substr(0, idx - 1) };
					clear();
					dir = temp + dir.substr(idx--);
				}
				catch (...) {
					StringCtorFailAndExit();
				}
				print();
				replace_cursor();
			}
			else if (!backwards && idx < dir.length()) {
				string temp{ dir.substr(0, idx) };
				clear();
				dir = temp + dir.substr(idx + 1);
				try {
					string temp{ dir.substr(0, idx) };
					clear();
					dir = temp + dir.substr(idx + 1);
				}
				catch (...) {
					StringCtorFailAndExit();
				}
				print();
				replace_cursor();
			}
		};
		auto move = [&](bool left = true) {
			if (left && idx > 0) {
				--idx;
				replace_cursor();
			}
			else if (!left && idx < dir.length()) {
				++idx;
				replace_cursor();
			}
		};
		auto fullmove = [&](bool to_start = true) {
			if (to_start) {
				idx = 0;
				replace_cursor();
			}
			else {
				idx = dir.length();
				replace_cursor();
			}
		};

		while (true) {
			switch (static_cast<Key>(ReadKey())) {
				//Numbers
			case (Key::Num0):	addchar('0'); break;
			case (Key::Num1):	addchar('1'); break;
			case (Key::Num2):	addchar('2'); break;
			case (Key::Num3):	addchar('3'); break;
			case (Key::Num4):	addchar('4'); break;
			case (Key::Num5):	addchar('5'); break;
			case (Key::Num6):	addchar('6'); break;
			case (Key::Num7):	addchar('7'); break;
			case (Key::Num8):	addchar('8'); break;
			case (Key::Num9):	addchar('9'); break;
				//Lower case
			case (Key::a):	addchar('a'); break;
			case (Key::b):	addchar('b'); break;
			case (Key::c):	addchar('c'); break;
			case (Key::d):	addchar('d'); break;
			case (Key::e):	addchar('e'); break;
			case (Key::f):	addchar('f'); break;
			case (Key::g):	addchar('g'); break;
			case (Key::h):	addchar('h'); break;
			case (Key::i):	addchar('i'); break;
			case (Key::j):	addchar('j'); break;
			case (Key::k):	addchar('k'); break;
			case (Key::l):	addchar('l'); break;
			case (Key::m):	addchar('m'); break;
			case (Key::n):	addchar('n'); break;
			case (Key::o):	addchar('o'); break;
			case (Key::p):	addchar('p'); break;
			case (Key::q):	addchar('q'); break;
			case (Key::r):	addchar('r'); break;
			case (Key::s):	addchar('s'); break;
			case (Key::t):	addchar('t'); break;
			case (Key::u):	addchar('u'); break;
			case (Key::v):	addchar('v'); break;
			case (Key::w):	addchar('w'); break;
			case (Key::x):	addchar('x'); break;
			case (Key::y):	addchar('y'); break;
			case (Key::z):	addchar('z'); break;
				//Upper case
			case (Key::A):	addchar('A'); break;
			case (Key::B):	addchar('B'); break;
			case (Key::C):	addchar('C'); break;
			case (Key::D):	addchar('D'); break;
			case (Key::E):	addchar('E'); break;
			case (Key::F):	addchar('F'); break;
			case (Key::G):	addchar('G'); break;
			case (Key::H):	addchar('H'); break;
			case (Key::I):	addchar('I'); break;
			case (Key::J):	addchar('J'); break;
			case (Key::K):	addchar('K'); break;
			case (Key::L):	addchar('L'); break;
			case (Key::M):	addchar('M'); break;
			case (Key::N):	addchar('N'); break;
			case (Key::O):	addchar('O'); break;
			case (Key::P):	addchar('P'); break;
			case (Key::Q):	addchar('Q'); break;
			case (Key::R):	addchar('R'); break;
			case (Key::S):	addchar('S'); break;
			case (Key::T):	addchar('T'); break;
			case (Key::U):	addchar('U'); break;
			case (Key::V):	addchar('V'); break;
			case (Key::W):	addchar('W'); break;
			case (Key::X):	addchar('X'); break;
			case (Key::Y):	addchar('Y'); break;
			case (Key::Z):	addchar('Z'); break;
				//Symbols
			case (Key::Space):			addchar(' '); break;
			case (Key::Tilde):			addchar('~'); break;
			case (Key::Exclam):			addchar('!'); break;
			case (Key::At):				addchar('@'); break;
			case (Key::Hash):			addchar('#'); break;
			case (Key::Dollar):			addchar('$'); break;
			case (Key::Percent):		addchar('%'); break;
			case (Key::Caret):			addchar('^'); break;
			case (Key::Ampersand):		addchar('&'); break;
			case (Key::ParenOpen):		addchar('('); break;
			case (Key::ParenClose):		addchar(')'); break;
			case (Key::Minus):			addchar('-'); break;
			case (Key::Equals):			addchar('='); break;
			case (Key::Underscore):		addchar('_'); break;
			case (Key::Plus):			addchar('+'); break;
			case (Key::BracketOpen):	addchar('['); break;
			case (Key::BracketClose):	addchar(']'); break;
			case (Key::BraceOpen):		addchar('{'); break;
			case (Key::BraceClose):		addchar('}'); break;
			case (Key::BSlash):			addchar('\\'); break;
			case (Key::Semicolon):		addchar(';'); break;
			case (Key::Colon):			addchar(':'); break;
			case (Key::Comma):			addchar(','); break;
			case (Key::SingleQuote):	addchar('\''); break;
			case (Key::DoubleQuote):	addchar('\"'); break;
			case (Key::Dot):			addchar('.'); break;
			case (Key::FSlash):			addchar('/'); break;
				//Controls
			case (Key::Backspace):	delchar(true); break;
			case (Key::Del):		delchar(false); break;
			case (Key::Left):		move(true); break;
			case (Key::Right):		move(false); break;
			case (Key::Home):		fullmove(true); break;
			case (Key::End):		fullmove(false); break;
			case (Key::Escape):		clear(); dir.clear(); return false;	//Cancel
			case (Key::Enter):		clear(); return true;				//Accept
			default: continue;
			}
		}
	}

	//Does not bound-check
	void UpdatePosition(bool increase) noexcept {
		prefixes[pos] = PREFIX_EMPTY;
		if (increase)	++pos;
		else			--pos;
		prefixes[pos] = PREFIX_POINT;
	}

	void CleanseLines(bool flush = true) noexcept {
		if (!SetCursorPosition(COORD_ZERO)) { return; }
		try {
			for (szt i{ 0 }; i < base.size(); ++i) cout << string(prefixes[i].size() + base[i].size() + suffixes[i].size(), ' ') << '\n';
			if (flush) { cout << std::flush; }
		}
		catch (...) {
			CoutFailAndExit();
		}
	}
	void CleanseInfoline(bool flush = true) noexcept {
		if (!SetCursorPosition(COORD_INFOLINE)) { return; }
		try {
			cout << string(INFOLINE_MSGS[pos].length() + (pos <= 1 ? suffixes[pos].length() : 0), ' ');
			if (flush) { cout << std::flush; }
		}
		catch (...) {
			CoutFailAndExit();
		}
	}
	void CleanseQueue(bool flush = true) noexcept {
		if (!SetCursorPosition(COORD_QUEUE)) { return; }
		try {
			for (const auto& msg : message_queue) { cout << string(msg.first.size(), ' ') << '\n'; }
			if (flush) { cout << std::flush; }
		}
		catch (...) {
			CoutFailAndExit();
		}
	}
	void CleanseAll(bool flush = true) noexcept { CleanseLines(false); CleanseInfoline(false); CleanseQueue(flush); }

	void PrintLines(bool flush = true) noexcept {
		if (!SetCursorPosition(COORD_ZERO)) { return; }
		try {
			for (szt i{ 0 }; i < base.size(); ++i) {
				PrintColoredText(msg_type{ string{ prefixes[i] + base[i] + suffixes[i] + '\n' }, MessageColor::Base });
			}
			if (flush) { cout << std::flush; }
		}
		catch (...) {
			CoutFailAndExit();
		}
	}
	void PrintInfolineMessage(szt i, bool flush = true) noexcept {
		if (!SetCursorPosition(COORD_INFOLINE)) { return; }
		try {
			PrintColoredText(msg_type{ INFOLINE_MSGS[i], MessageColor::Base });
			if (flush) { cout << std::flush; }
		}
		catch (...) {
			CoutFailAndExit();
		}
	}
	void PrintInfoline(bool flush = true) noexcept { PrintInfolineMessage(pos, flush); }
	void PrintQueue(bool flush = true) noexcept {
		GetMQ(message_queue);
		if (!SetCursorPosition(COORD_QUEUE)) { return; }
		try {
			for (const auto& msg : message_queue) {
				PrintColoredText(msg, true);
				cout << '\n';
			}
			if (flush) { cout << std::flush; }
		}
		catch (...) {
			CoutFailAndExit();
		}
	}
	void PrintAll(bool flush = true) noexcept { PrintLines(false); PrintInfoline(false); PrintQueue(flush); }

	void ReprintLines(bool flush = true) noexcept { CleanseLines(false); PrintLines(flush); }
	void ReprintInfoline(bool flush = true) noexcept { CleanseInfoline(false); PrintInfoline(flush); }
	void ReprintQueue(bool flush = true) noexcept { CleanseQueue(false); PrintQueue(flush); }
	void ReprintAll(bool flush = true) noexcept { CleanseAll(false); PrintAll(flush); }

}
using namespace ConsoleUtils;


ConsoleHandler& ConsoleHandler::GetSingleton() noexcept {
	static ConsoleHandler singleton{};
	return singleton;
}

ConsoleHandler::ConsoleHandler() noexcept {
	try {
		mqPtr = &MessageQueue::GetSingleton();
		if (!mqPtr) {
			return;
		}
		mqPtr->SetNewerFirst();
		mqPtr->SetMaxLength(20);
		GetMQ = [](queue_type& q) noexcept { mqPtr->CopyQueue(q); };
		GetMQ(message_queue);

		active.store(true);
		//mqPtr->PushMessage("ConsoleHandler made with mqPtr:" + to_string(reinterpret_cast<szt>(mqPtr)));
	}
	catch (...) {
		cout << "ConsoleHandler failed to initialize message queue and will not function";
	}
	
}

void ConsoleHandler::Start() const noexcept{
	if (!active.load(std::memory_order_relaxed) || !InitConsoleInternals()) { return; }
	if (!HideCursor()) { logger.Warning("Hiding the cursor is purely for visuals so the program will proceed. Enjoy your blinking!"sv); }

	//Loop
	while (true) {
		PrintAll();

		Key key = ReadKey();
		while ((key != Key::Enter) && (key != Key::Up) && (key != Key::Down)) { key = ReadKey(); }

		//Arrows
		if ((key == Key::Up && pos > 0) || (key == Key::Down && pos < base.size() - 1)) {
			CleanseAll(false);
			UpdatePosition(key == Key::Down);
		}
		//Enter
		else if (key == Key::Enter) {
			if (pos <= 1) { //Read dirs
				CleanseInfoline();
				PrintInfolineMessage(NewDirIdx);
				ShowCursor();
				try {
					string dir{};
					bool gotNewDir{ ReadDir(NewDirIdx, dir) };
					while (gotNewDir) {
						if ((pos == 0 ? file_manager.SetDiffPath(dir) : file_manager.SetTargetPath(dir))) {
							break;
						}
						ReprintQueue();
						PrintInfolineMessage(NewDirIdx);
						gotNewDir = ReadDir(NewDirIdx, dir);
					}
					HideCursor();
					CleanseAll(false);
					if (gotNewDir) { suffixes[pos] = std::move(dir); }
				}
				catch (...) {
					StringCtorFailAndExit();
				}
			}
			else if (pos == 2) { //Parse without commit
				CleanseAll(false);
				logger.NoSeverity(GetTimeString() + ": Initiating parse without commit...");
				file_manager.ParseWithoutCommit();
			}
			else if (pos == 3) { //Parse and commit
				CleanseAll(false);
				logger.NoSeverity(GetTimeString() + ": Initiating parse with commit...");
				file_manager.ParseAndCommit();
			}
			else if (pos == 4) { //Commit
				CleanseAll(false);
				logger.NoSeverity(GetTimeString() + ": Initiating commit...");
				file_manager.Commit();
			}
			else if (pos == 5) { //Reset Program
				CleanseAll(false);
				Reset();
				file_manager.Reset();
				logger.NoSeverity(GetTimeString() + ": Program has been reset.");
			}
			else if (pos == 6) { //Clear Console
				CleanseAll(false);
				mqPtr->Clear();
			}
			else if (pos == 7) { //Close
				FlushAndClose();
			}
		}
		logger.Commit();
	}
}

void ConsoleHandler::CharTest() const noexcept {
	cout << "\nPress a key (arrows and functin keys should be pressed twice)..." << std::endl;
	while (true) {
		cout << "\n\tkeycode:" << _getch() << std::endl;
	}
}

