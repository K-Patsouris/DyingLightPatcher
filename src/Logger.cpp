#include "logger.h"
#include "Containers.h"

#include <atomic>
#include <mutex>
#include <fstream>

//#include <iostream>


namespace Logging {
	using std::atomic;
	using std::lock_guard;
	using std::mutex;
	using std::ofstream;
	using Output = Logger::Output;
	using Severity = Logger::Severity;
	using MessageColor = MessageQueue::MessageColor;
	using Locker = lock_guard<mutex>;

	atomic<bool> active{ false };
	mutex lock{};

	atomic<Output> target{ Output::queue_and_file };

	MessageQueue* mqPtr;
	void (*PushToQueue)(const string& msg, MessageColor color) noexcept = [](const string& msg, MessageColor color) noexcept {};

	string filename{ "differ.log" };
	ofstream filestream{};



	Logger& Logger::GetSingleton() noexcept {
		static Logger singleton{};
		return singleton;
	}

	bool Logger::SetFilename(const string& newname) const noexcept {
		try {
			Locker locker(lock);
			if (active.load(std::memory_order_relaxed))
				return false;
			filename = newname;
			return filename == newname;
		}
		catch (...) {
			return false;
		}
	}
	bool Logger::SetTargetOutput(Output newtarget) const noexcept { Locker locker(lock); target = newtarget; return target == newtarget; }
	bool Logger::Init() const noexcept {
		if (active.load(std::memory_order_relaxed))
			return false;
		try {
			Locker locker(lock);

			filestream.open(filename, std::ios::out | std::ios::trunc);

			mqPtr = &MessageQueue::GetSingleton();
			if (mqPtr) { PushToQueue = [](const string& msg, MessageColor color) noexcept { mqPtr->PushMessage(msg, color); }; }

			active.store(mqPtr && filestream.is_open());
			return active.load(std::memory_order_relaxed);
		}
		catch (...) {
			return false;
		}
	}
	bool Logger::Commit() const noexcept {
		if (!active.load(std::memory_order_relaxed))
			return false;
		try {
			Locker locker(lock);
			filestream.flush();
			return true;
		}
		catch (...) {
			return false;
		}
	}
	bool Logger::Close() const noexcept {
		if (!active.load(std::memory_order_relaxed))
			return false;
		try {
			Locker locker(lock);
			mqPtr = nullptr;
			filestream.close();
			active.store(false);
			return true;
		}
		catch (...) {
			return false;
		}
	}


	constexpr string GetPrefix(Severity severity) {
		switch (severity) { //Use spaces for spacing to be able to clear them easily. Tabs take some whatever amount of spaces in console so it's not easy to clear them by string.size() checking.
		case (Severity::info):			return "<INFO>     ";
		case (Severity::warning):		return "<WARNING>  ";
		case (Severity::error):			return "<ERROR>    ";
		case (Severity::critical):		return "<CRITICAL> ";
		case (Severity::no_severity):	return "";
		default:						return "<UNKNOWN>  ";
		}
	}
	constexpr MessageColor GetColor(Severity severity) {
		switch (severity) {
		case (Severity::info):			return MessageColor::Info;
		case (Severity::warning):		return MessageColor::Warning;
		case (Severity::error):			return MessageColor::Error;
		case (Severity::critical):		return MessageColor::Critical;
		case (Severity::no_severity):	return MessageColor::Base;
		default:						return MessageColor::Base;
		}
	}

	bool Logger::PushMessage(const string& newmsg, Severity severity) const noexcept {
		if (newmsg.length() <= 0u || !active.load(std::memory_order_relaxed))
			return false;
		try {
			string prefixed = GetPrefix(severity) + newmsg;
			if (target.load(std::memory_order_relaxed) & file) {
				filestream << prefixed << '\n';
			}
			if (target.load(std::memory_order_relaxed) & queue) {
				PushToQueue(prefixed, GetColor(severity));
			}
			return true;
		}
		catch (...) {
			return false;
		}
	}
	void Logger::PrintConsole(const string& msg) const noexcept { try { PushToQueue(msg, MessageColor::Base); } catch (...) { return; } }


	bool Logger::ToConsole(const string& text) const noexcept {
		if (!active.load(std::memory_order_relaxed)) return false;
		try { MessageQueue::GetSingleton().PushMessage(text); return true; }
		catch (...) { return false; }
	}
	bool Logger::ToFile(const string& filename, const string& text) const noexcept {
		if (!active.load(std::memory_order_relaxed))
			return false;
		try {
			ofstream fs(filename, std::ios::out | std::ios::trunc);
			fs << text;
			fs.close();
			return true;
		}
		catch (...) {
			return false;
		}
	}
	bool Logger::ToFileAndConsole(const string& filename, const string& text) const noexcept {
		if (!active.load(std::memory_order_relaxed))
			return false;
		try {
			MessageQueue::GetSingleton().PushMessage(text);
			ofstream fs(filename, std::ios::out | std::ios::trunc);
			fs << text;
			fs.close();
			return true;
		}
		catch (...) {
			return false;
		}
	}

}