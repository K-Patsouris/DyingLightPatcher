#pragma once
#include "Common.h"
#include <limits>
#include <mutex>
//#include <stdarg.h>


class MessageQueue {
public:
	static MessageQueue& GetSingleton();

	enum MessageColor : uint8 {
		Base = 7,
		Info = 3,
		Warning = 6,
		Error = 4,
		Critical = 12,
	};

	bool PushMessage(const string& msg, MessageColor color = Base) noexcept;

	bool CopyQueue(deque<std::pair<string, MessageColor>>& out) const noexcept;

	bool GetMessage(szt index, string& out) const noexcept;
	bool GetMessageString(string& out) const noexcept;

	bool IsNewerFirst() const noexcept;
	bool SetNewerFirst(bool and_reverse_if_needed = true) noexcept;
	bool SetOlderFirst(bool and_reverse_if_needed = true) noexcept;

	szt MaxLength() const noexcept;
	bool SetMaxLength(szt newmax, bool and_shorten_if_needed = true) noexcept;

	bool Clear() noexcept;

private:
	using Locker = std::lock_guard<std::mutex>;
	mutable std::mutex lock{};
	deque<std::pair<string, MessageColor>> mq;
	szt max_length{ 10 };
	bool newer_first{ false }; //false for newer messages going to the end

	MessageQueue();
	~MessageQueue() = default;
};

