#pragma once
#include "Common.h"

class ConsoleHandler {
public:
	static ConsoleHandler& GetSingleton() noexcept;

	void Start() const noexcept;

	void CharTest() const noexcept;

private:
	ConsoleHandler() noexcept;
	~ConsoleHandler() noexcept = default;




};


