#pragma once
#include "Common.h"
#include <algorithm>

//Integral or real with values in [min, max] with overflow protection. Prefer LimitedIntegral for integrals, for incremental operators. min <= val <= max
template <typename T, T min, T max> requires ((std::integral<T> || std::floating_point<T>) && (max >= min))
struct LimitedNumber {
public:
	LimitedNumber() : val(std::clamp<T>(static_cast<T>(0), min, max)) {}
	LimitedNumber(T val) : val(std::clamp<T>(val, min, max)) {}

	operator T() const { return this->val; }
	LimitedNumber& operator=(T new_value) { this->val = std::clamp(new_value, min, max); return *this; }
	LimitedNumber& operator+=(T extra) {
		if (val >= static_cast<T>(0)) [[likely]] //Must bound-check the extra amount, and for that we need val's sign
			this->val = std::clamp(this->val + std::clamp(extra, std::numeric_limits<T>::lowest(), (std::numeric_limits<T>::max() - this->val)), min, max); //clamp to [numeric::min - (numeric::max-val)] before adding
		else
			this->val = std::clamp(this->val + std::clamp(extra, (std::numeric_limits<T>::lowest() - this->val), std::numeric_limits<T>::max()), min, max); //clamp to [(numeric::min-val) - numeric::max] before adding
		return *this;
	}
	LimitedNumber& operator-=(T extra) { //Can't just redirect to += because for integrals it overflows if extra is the minimum. Both operators have to be defined.
		if (this->val >= static_cast<T>(0)) [[likely]] //Must bound-check the extra amount, and for that we need this->val's sign
			this->val = std::clamp(this->val - std::clamp(extra, std::numeric_limits<T>::lowest(), (std::numeric_limits<T>::max() - this->val)), min, max); //clamp to [numeric::min - (numeric::max-this->val)] before adding
		else
			this->val = std::clamp(this->val - std::clamp(extra, (std::numeric_limits<T>::lowest() - this->val), std::numeric_limits<T>::max()), min, max); //clamp to [(numeric::min-val) - numeric::max] before adding
		return *this;
	}
	T operator+(T other) const { return this->val + other; }
	T operator-(T other) const { return this->val - other; }
	T operator*(T other) const { return this->val * other; }
	T operator/(T other) const { return this->val / other; }
	bool operator==(T other) const { return this->val == other; }
	bool operator!=(T other) const { return this->val != other; }
	bool operator>=(T other) const { return this->val >= other; }
	bool operator<=(T other) const { return this->val <= other; }
	bool operator>(T other) const { return this->val > other; }
	bool operator<(T other) const { return this->val < other; }

	T Get() const { return this->val; } //To forcefully give the int32 and not the object to things that accept everything
	T Min() const { return min; }
	T Max() const { return max; }

protected:
	T val;
};
//Specialized to integrals, defining pre/postfix increment and decrement operators. Prefix returns T. min <= val <= max
template <typename T, T min, T max> requires (std::integral<T> && (max >= min))
struct LimitedIntegral : LimitedNumber<T, min, max> {
public:
	using LimitedNumber<T, min, max>::LimitedNumber; //Inherit constructors
	using LimitedNumber<T, min, max>::operator=; //and assignment operator
	LimitedIntegral& operator++() { if (this->val < max) [[likely]] ++(this->val); return *this; } //++limI;
	LimitedIntegral& operator--() { if (this->val > min) [[likely]] --(this->val); return *this; }
	T operator++(int) { T temp = this->val; this->operator++(); return temp; } //limI++;
	T operator--(int) { T temp = this->val; this->operator--(); return temp; }
	float Float() const { return static_cast<float>(this->val); }
private:
};

//Specialized to 0+, allowing for non-branching +=/-= operators. min <= val <= max
template <typename T, T max> requires (std::integral<T> && (max >= 0))
struct PositiveIntegral : LimitedIntegral<T, 0, max> {
public:
	using LimitedIntegral<T, 0, max>::LimitedIntegral; //Inherit constructors
	using LimitedIntegral<T, 0, max>::operator=; //and assignment operator


	PositiveIntegral& operator+=(const T& extra) { //Redefine to optimize them a bit now that we know numbers
		this->val = std::clamp<T>(this->val + std::clamp(extra, std::numeric_limits<T>::min(), (std::numeric_limits<T>::max() - this->val)), 0, max);
		return *this;
	}
	PositiveIntegral& operator-=(const T& extra) { //Can't just redirect to += because if extra is min() then it would overflow. Both operators have to be defined.
		this->val = std::clamp<T>(this->val - std::clamp(extra, std::numeric_limits<T>::min(), (std::numeric_limits<T>::max() - this->val)), 0, max);
		return *this;
	}
};


template<typename Enum> requires (std::is_enum_v<Enum>)
struct BitFlagsRaw {
private:
	using base_t = std::underlying_type_t<Enum>;
public:

	constexpr BitFlagsRaw() noexcept = default;
	constexpr BitFlagsRaw(const BitFlagsRaw&) noexcept = default;
	constexpr BitFlagsRaw(BitFlagsRaw&&) noexcept = default;
	constexpr BitFlagsRaw& operator=(const BitFlagsRaw&) noexcept = default;
	constexpr BitFlagsRaw& operator=(BitFlagsRaw&&) noexcept = default;
	constexpr ~BitFlagsRaw() noexcept = default;


	template<typename... Enums>
		requires(sizeof...(Enums) > 0 and (std::same_as<Enums, Enum> and ...))
	constexpr void Set(Enums... vals) noexcept {
		flags |= ((base_t{ 1 } << static_cast<base_t>(vals)) bitor ...);
	}

	template<typename... Enums>
		requires(sizeof...(Enums) > 0 and (std::same_as<Enums, Enum> and ...))
	constexpr void Unset(Enums... vals) noexcept {
		flags &= ~(static_cast<base_t>(base_t{ 1 } << static_cast<base_t>(vals)) bitor ...);
	}

	template<typename... Enums>
		requires(sizeof...(Enums) > 0 and (std::same_as<Enums, Enum> and ...))
	constexpr bool Any(Enums... vals) const noexcept {
		return base_t{ 0 } != (flags & ((base_t{ 1 } << static_cast<base_t>(vals)) bitor ...));
	}

	template<typename... Enums>
		requires(sizeof...(Enums) > 0 and (std::same_as<Enums, Enum> and ...))
	constexpr bool All(Enums... vals) const noexcept {
		const base_t mask = ((base_t{ 1 } << static_cast<base_t>(vals)) bitor ...);
		return (flags & mask) == mask;
	}

	constexpr bool Only(Enum val) const noexcept { return flags == static_cast<base_t>(val); }

	template<typename... Enums>
		requires(sizeof...(Enums) > 0 and (std::same_as<Enums, Enum> and ...))
	constexpr bool Only(Enums... vals) const noexcept {
		return flags == ((base_t{ 1 } << static_cast<base_t>(vals)) bitor ...);
	}

	constexpr bool None() const noexcept { return flags == base_t{ 0 }; }

	constexpr void Clear() noexcept { flags = base_t{ 0 }; }


private:
	base_t flags{};
};

