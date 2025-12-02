#pragma once

#include <format>
#include <fstream>
#include <string_view>

struct Logger {
	enum class Level {
		Debug,
		Info,
		Warning,
		Error,
	};

	Logger(std::string_view app_name);

	auto debug(std::string_view msg) -> void;
	auto info(std::string_view msg) -> void;
	auto warn(std::string_view msg) -> void;
	auto err(std::string_view msg) -> void;

	template<typename... Args>
	auto debug(std::format_string<Args...> fmt, Args &&...args) -> void
	{
		log(Level::Debug, std::format(fmt, std::forward<Args>(args)...));
	}

	template<typename... Args>
	auto info(std::format_string<Args...> fmt, Args &&...args) -> void
	{
		log(Level::Info, std::format(fmt, std::forward<Args>(args)...));
	}

	template<typename... Args>
	auto warn(std::format_string<Args...> fmt, Args &&...args) -> void
	{
		log(Level::Warning, std::format(fmt, std::forward<Args>(args)...));
	}

	template<typename... Args>
	auto err(std::format_string<Args...> fmt, Args &&...args) -> void
	{
		log(Level::Error, std::format(fmt, std::forward<Args>(args)...));
	}

	auto log(Level level, std::string_view msg) -> void;

private:
#ifndef __EMSCRIPTEN__
	std::ofstream m_fout;
#endif
};
