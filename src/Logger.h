#pragma once
#include "Common.h"

#include <format>


namespace Logging {
	class Logger {
	public:
		enum Severity {
			info = 0,
			warning = 1,
			error = 2,
			critical = 3,

			no_severity
		};
		enum Output {
			queue = 1 << 0,
			file = 1 << 1,
			queue_and_file = queue | file
		};

		static Logger& GetSingleton() noexcept;

		bool SetFilename(const string& newname) const noexcept;
		bool SetTargetOutput(Output newtarget) const noexcept;
		bool Init() const noexcept;
		bool Commit() const noexcept;
		bool Close() const noexcept;

	private:
		bool PushMessage(const string& newmsg, Severity severity) const noexcept;
		void PrintConsole(const string& msg) const noexcept;
		template <typename... Args>
		bool Log(Severity severity, string_view fmt, auto&&... args) const noexcept {
			try { return PushMessage(std::vformat(fmt, std::make_format_args(args...)), severity); }
			catch (const std::format_error& fmt_err) { PrintConsole(string{ "\nLogger raised <std::format_error>: " } + fmt_err.what()); return false; }
			catch (const std::bad_alloc& bad_alc) { PrintConsole(string{ "\nLogger raised <std::bad_alloc>: " } + bad_alc.what()); return false; }
			catch (const std::exception& exc) { PrintConsole(string{ "\nLogger raised <std::exception>: " } + exc.what()); return false; }
			catch (...) { PrintConsole("\nLogger raised unknown exception"); return false; }
		}
	public:
		bool Info(string_view fmt, auto&&... args) const noexcept { return Log(info, fmt, std::forward<decltype(args)>(args)...); }
		bool Warning(string_view fmt, auto&&... args) const noexcept { return Log(warning, fmt, std::forward<decltype(args)>(args)...); }
		bool Error(string_view fmt, auto&&... args) const noexcept { return Log(error, fmt, std::forward<decltype(args)>(args)...); }
		bool Critical(string_view fmt, auto&&... args) const noexcept { return Log(critical, fmt, std::forward<decltype(args)>(args)...); }
		bool NoSeverity(string_view fmt, auto&&... args) const noexcept { return Log(no_severity, fmt, std::forward<decltype(args)>(args)...); }

		bool ToConsole(const string& text) const noexcept;
		bool ToFile(const string& filename, const string& text) const noexcept;
		bool ToFileAndConsole(const string& filename, const string& text) const noexcept;

	private:

		Logger() noexcept = default;
		~Logger() noexcept  = default;

		Logger(const Logger&) = delete;
		Logger(Logger&&) = delete;
		Logger& operator=(const Logger&) = delete;
		Logger& operator=(Logger&&) = delete;
	};

	

}

using LogOutput = Logging::Logger::Output;
using LogSeverity = Logging::Logger::Severity;
inline const Logging::Logger& logger = Logging::Logger::GetSingleton();

