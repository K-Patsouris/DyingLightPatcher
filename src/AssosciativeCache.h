#pragma once
#include "Common.h"
#include "logger.h"
#include <mutex>


template <typename Value, typename ID = uint64> requires (std::integral<ID>)
class AssosciativeCache {
public:
	using CacheMap = map<Value, ID>;
	using CacheIterator = CacheMap::iterator;
	using CacheCIterator = CacheMap::const_iterator;
	static constexpr ID NULL_ID{ std::numeric_limits<ID>::min() };


	[[nodiscard]] ID Find(const Value& val) const noexcept {
		if (CacheCIterator iter{ cache.find(val) }; iter != cache.end()) {
			return iter->second;
		}
		return ID{ NULL_ID };
	}
	[[nodiscard]] ID FindOrAdd(const Value& val) noexcept {
		if (CacheCIterator iter{ cache.find(val) }; iter != cache.end()) {
			return iter->second;
		}
		else {
			try {
				if (auto result{ cache.insert({ val, nextID }) }; result.second) {
					++nextID;
					return result.first->second;
				}
			}
			catch (...) {
				logger.Error("Cache FindOrAdd(const&<{}>) failed but state was preserved"sv, val);
			}
			return ID{ NULL_ID };
		}
	}
	[[nodiscard]] ID FindOrAdd(Value&& val) noexcept {
		if (CacheCIterator iter{ cache.find(val) }; iter != cache.end()) {
			return iter->second;
		}
		else {
			try {
				if (auto result{ cache.insert({ val, nextID }) }; result.second) {
					++nextID;
					return result.first->second;
				}
			}
			catch (...) {
				logger.Error("Cache FindOrAdd(&&<{}>) failed but state was preserved"sv, val);
			}
			return ID{ NULL_ID };
		}
	}
	
	[[nodiscard]] const Value& Find(const ID id) const noexcept {
		for (const auto& [value, id_] : cache) {
			if (id == id_) {
				return value;
			}
		}
		return cache.cbegin()->first;
	}

	bool Delete(const Value& val) noexcept {
		try {
			if (val == Value{}) [[unlikely]] {
				return false;
			}
			if (CacheCIterator iter = cache.find(val); iter != cache.end()) {
				cache.erase(iter);
				return true;
			}
		}
		catch (...) {
			logger.Error("Cache Delete(<{}) failed but state was preserved"sv, val);
		}
		return false;
	}
	bool Delete(const ID id) noexcept {
		if (id == NULL_ID) [[unlikely]] {
			return false;
		}
		for (CacheCIterator iter{ cache.begin() }; iter != cache.end(); ++iter) {
			if (iter->second == id) {
				cache.erase(iter);
				return true;
			}
		}
		return false;
	}

	szt Size() const noexcept { return cache.size(); }
	//Resets the cache to its construction state, leaving it with only 1 pair of {Value{}, NULL_ID}. Next ID assigned will be NULL_ID + 1.
	void Reset() noexcept {
		if (cache.size() < 2u) { //Nothing inside but the default pair
			return;
		}
		cache.erase(++cache.cbegin(), cache.cend());
		nextID = ID{ NULL_ID + 1 };
	}
	//Completely clears the cache, including the default constructed first element. Next ID assigned will be NULL_ID.
	void Clear() noexcept { cache.clear(); nextID = ID{ NULL_ID }; }

private:
	CacheMap cache{ {Value{}, NULL_ID} };
	ID nextID{ NULL_ID + 1 };
};
static_assert(sizeof(AssosciativeCache<string, uint64>) == 24u);


template <typename Value, typename ID = uint64> requires (std::integral<ID>)
class ThreadSafeAssosciativeCache {
public:
	using CacheMap = map<Value, ID>;
	using CacheIterator = CacheMap::iterator;
	using CacheCIterator = CacheMap::const_iterator;
	static constexpr ID NULL_ID{ std::numeric_limits<ID>::min() };

	[[nodiscard]] ID Find(const Value& val) const noexcept {
		try {
			Locker locker{ lock };
			if (CacheCIterator iter = cache.find(val); iter != cache.end()) {
				return iter->second;
			}
		}
		catch (std::system_error& sys_err) {
			logger.Error("SYSTEM_ERROR: {}\nCache Find(<{}>) failed but state was preserved"sv, sys_err.what(), val);
		}
		catch (...) {
			logger.Error("Cache Find(<{}>) failed for unspecified reasons but state was preserved"sv, val);
		}
		return ID{ NULL_ID };
	}
	[[nodiscard]] ID FindOrAdd(const Value& val) noexcept {
		try {
			Locker locker{ lock };
			if (CacheCIterator iter = cache.find(val); iter != cache.end()) {
				return iter->second;
			}
			else {
				if (auto result = cache.insert({ val, nextID }); result.second) {
					++nextID;
					return result.first->second;
				}
			}
		}
		catch (std::system_error& sys_err) {
			logger.Error("SYSTEM_ERROR: {}\nCache FindOrAdd (const& <{}>) failed but state was preserved"sv, sys_err.what(), val);
		}
		catch (...) {
			logger.Error("Cache FindOrAdd (const& <{}>) failed for unspecified reasons but state was preserved"sv, val);
		}
		return ID{ NULL_ID };
	}
	[[nodiscard]] ID FindOrAdd(Value&& val) noexcept {
		try {
			Locker locker{ lock };
			if (CacheCIterator iter = cache.find(val); iter != cache.end()) {
				return iter->second;
			}
			else {
				if (auto result = cache.insert({ std::move(val), nextID }); result.second) {
					++nextID;
					return result.first->second;
				}
			}
		}
		catch (std::system_error& sys_err) {
			logger.Error("SYSTEM_ERROR: {}\nCache FindOrAdd(&&<{}>) failed but state was preserved"sv, sys_err.what(), val);
		}
		catch (...) {
			logger.Error("Cache FindOrAdd(&&<{}>) failed for unspecified reasons but state was preserved"sv, val);
		}
		return ID{ NULL_ID };
	}
	
	[[nodiscard]] const Value& Find(const ID id) const noexcept {
		try {
			Locker locker{ lock };
			for (const auto& [value, id_] : cache) {
				if (id == id_) {
					return value;
				}
			}
		}
		catch (std::system_error& sys_err) {
			logger.Error("SYSTEM_ERROR: {}\nCache Find({}) failed but state was preserved"sv, sys_err.what(), id);
		}
		catch (...) {
			logger.Error("Cache Find({}) failed for unspecified reasons but state was preserved"sv, id);
		}
		return cache.cbegin()->first;
	}

	bool Delete(const Value& val) noexcept {
		try {
			Locker locker{ lock };
			if (CacheCIterator iter = cache.find(val); iter != cache.end()) {
				cache.erase(iter);
				return true;
			}
		}
		catch (std::system_error& sys_err) {
			logger.Error("SYSTEM_ERROR: {}\nCache Delete(<{}>) failed but state was preserved"sv, sys_err.what(), val);
		}
		catch (...) {
			logger.Error("Cache Delete(<{}>) failed for unspecified reasons but state was preserved"sv, val);
		}
		return false;
	}
	bool Delete(const ID id) noexcept {
		try {
			Locker locker{ lock };
			for (CacheCIterator iter{ cache.begin() }; iter != cache.end(); ++iter) {
				if (iter->second == id) {
					cache.erase(iter);
					return true;
				}
			}
		}
		catch (std::system_error& sys_err) {
			logger.Error("SYSTEM_ERROR: {}\nCache Delete() {} failed but state was preserved"sv, sys_err.what(), id);
		}
		catch (...) {
			logger.Error("Cache Delete() {} failed for unspecified reasons but state was preserved"sv, id);
		}
		return false;
	}

	szt Size() const noexcept {
		try {
			Locker locker{ lock };
			return cache.size();
		}
		catch (...) {
			logger.Error("Cache Size() failed for unspecified reasons"sv);
			return szt{};
		}
	}
	bool Clear() noexcept {
		try {
			Locker locker{ lock };
			cache.clear();
			nextID = ID{ NULL_ID };
			return true;
		}
		catch (std::system_error& sys_err) {
			logger.Error("SYSTEM_ERROR: {}\nCache clear failed but state was preserved"sv, sys_err.what());
			return false;
		}
		catch (...) {
			logger.Error("Cache clear failed for unspecified reasons but state was preserved"sv);
			return false;
		}
	}

private:
	using Locker = std::lock_guard<std::mutex>;
	mutable std::mutex lock{};
	CacheMap cache{ {""s, NULL_ID} };
	ID nextID{ NULL_ID + 1 };
};
static_assert(sizeof(ThreadSafeAssosciativeCache<string, uint64>) == 104u);





