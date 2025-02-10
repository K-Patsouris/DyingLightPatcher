#include "Containers.h"

#include <algorithm> //std::reverse

MessageQueue& MessageQueue::GetSingleton() {
	static MessageQueue singleton{};
	return singleton;
}

MessageQueue::MessageQueue() { mq.clear(); }

bool MessageQueue::CopyQueue(deque<std::pair<string, MessageColor>>& out) const noexcept {
	out.clear();
	try {
		Locker locker(lock);
		out = mq;
		return true;
	}
	catch (...) {
		return false;
	}
}

bool MessageQueue::PushMessage(const string& msg, MessageColor color) noexcept {
	try {
		Locker locker(lock);
		if (newer_first) {
			while (mq.size() >= max_length) { mq.pop_back(); }
			mq.push_front({ msg, color });
		}
		else {
			while (mq.size() >= max_length) { mq.pop_front(); }
			mq.push_back({ msg, color });
		}
		return true;
	}
	catch (...) {
		return false;
	}
}

bool MessageQueue::GetMessage(szt index, string& out) const noexcept {
	out.clear();
	if (mq.empty() || index >= mq.size()) {
		return false;
	}
	try {
		Locker locker(lock);
		if (newer_first) {
			out = mq[mq.size() - index - 1].first;
		}
		else {
			out = mq[index].first;
		}
		return true;
	}
	catch (...) {
		return false;
	}
}
bool MessageQueue::GetMessageString(string& out) const noexcept {
	out.clear();
	if (mq.empty()) {
		return false;
	}
	try {
		Locker locker(lock);
		for (const auto& msg : mq) { out += msg.first + '\n'; }
		out.pop_back();
		return true;
	}
	catch (...) {
		return false;
	}
}

bool MessageQueue::IsNewerFirst() const noexcept {
	try {
		Locker locker(lock);
		return newer_first;
	}
	catch (...) {
		return false;
	}
}
bool MessageQueue::SetNewerFirst(bool and_reverse_if_needed) noexcept {
	try {
		Locker locker(lock);
		if (!newer_first) {
			newer_first = true;
			if (and_reverse_if_needed) {
				std::reverse(mq.begin(), mq.end());
			}
		}
		return true;
	}
	catch (...) {
		return false;
	}
}
bool MessageQueue::SetOlderFirst(bool and_reverse_if_needed) noexcept {
	try {
		Locker locker(lock);
		if (newer_first) {
			newer_first = false;
			if (and_reverse_if_needed) {
				std::reverse(mq.begin(), mq.end());
			}
		}
		return true;
	}
	catch (...) {
		return false;
	}
}

szt MessageQueue::MaxLength() const noexcept {
	try {
		Locker locker(lock);
		return max_length;
	}
	catch (...) {
		return 0;
	}
}
bool MessageQueue::SetMaxLength(szt newmax, bool and_shorten_if_needed) noexcept {
	try {
		Locker locker(lock);
		while (mq.size() > newmax) {
			if (newer_first) { mq.pop_back(); }
			else { mq.pop_front(); }
		}
		max_length = newmax;
		return true;
	}
	catch (...) {
		return false;
	}
}

bool MessageQueue::Clear() noexcept {
	try {
		Locker locker(lock);
		mq.clear();
		return true;
	}
	catch (...) {
		return false;
	}
}
