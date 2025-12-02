#include "Logger.h"

#include "Util.h"

#include <chrono>
#include <filesystem>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

#ifdef _WIN32
#	include <shlobj.h> // SHGetKnownFolderPath
#	include <windows.h>
#elif defined(__APPLE__)
#	include <pwd.h>
#	include <sys/types.h>
#	include <unistd.h>
#else
#	include <pwd.h>
#	include <unistd.h>
#endif

#ifndef __EMSCRIPTEN__
#	include <zlib.h>
#endif

#define FG_BLUE "\033[34m"
#define FG_RED "\033[31m"
#define FG_YELLOW "\033[33m"
#define FG_GRAY "\033[90m"
#define ANSI_RESET "\033[0m"

static std::filesystem::path get_log_path(std::string_view app_name)
{
#ifdef _WIN32
	PWSTR path = nullptr;
	SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &path);
	std::wstring wpath(path);
	CoTaskMemFree(path);
	return std::filesystem::path(wpath) / app_name / "logs";
#elif defined(__APPLE__)
	const char *home = getenv("HOME");
	if (!home)
		home = getpwuid(getuid())->pw_dir;
	return std::filesystem::path(home) / "Library" / "Logs" / app_name;
#else
	auto const *home { getenv("HOME") };
	if (!home)
		home = getpwuid(getuid())->pw_dir;
	return std::filesystem::path(home) / ".local" / "share" / app_name / "logs";
#endif
}

#ifndef __EMSCRIPTEN__
static int compress_file(std::filesystem::path const &input_path,
    std::filesystem::path const &output_path)
{
	size_t const chunk_size = 4096;

	std::ifstream in { input_path, std::ios::binary };
	if (!in)
		return 1;

	gzFile out { gzopen(output_path.string().c_str(), "wb") };
	if (!out)
		return 1;
	defer(gzclose(out));

	std::vector<char> buffer(chunk_size);
	while (in) {
		in.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
		std::streamsize bytes = in.gcount();
		if (bytes > 0)
			gzwrite(out, buffer.data(), static_cast<unsigned int>(bytes));
	}

	std::filesystem::remove(input_path);

	return 0;
}
#endif

Logger::Logger(std::string_view app_name)
{
#ifndef __EMSCRIPTEN__
	auto path { get_log_path(app_name) };
	auto const exists { std::filesystem::exists(path) };
	if (exists && !std::filesystem::is_directory(path)) {
		std::filesystem::remove_all(path);
	}
	if (!exists) {
		std::filesystem::create_directories(path);
	}

	int max { -1 };
	std::filesystem::directory_iterator iter(path);
	for (auto const &file : iter) {
		if (!file.is_regular_file())
			continue;

		auto name = file.path().filename().stem().string();
		constexpr std::string_view prefix = "log_";

		if (name.rfind(prefix, 0) != 0) {
			continue;
		}

		int v = std::stoi(name.substr(prefix.size()));
		if (v > max)
			max = v;

		auto ext = file.path().filename().extension().string();
		if (ext == ".txt") {
			auto np = file.path();
			np.replace_extension(ext + ".gz");
			compress_file(file.path(), np);
		}
	}
	max++;

	path /= std::format("log_{}.txt", max);
	m_fout = std::ofstream(path, std::ios::app | std::ios::out);
#endif // EMSCRIPTEN
}

auto Logger::debug(std::string_view msg) -> void
{
	log(Logger::Level::Debug, msg);
}
auto Logger::info(std::string_view msg) -> void
{
	log(Logger::Level::Info, msg);
}
auto Logger::warn(std::string_view msg) -> void
{
	log(Logger::Level::Warning, msg);
}
auto Logger::err(std::string_view msg) -> void
{
	log(Logger::Level::Error, msg);
}

static std::string get_current_time_string()
{
	auto now { std::chrono::system_clock::now() };
	auto now_c { std::chrono::system_clock::to_time_t(now) };
	std::tm tm { *std::gmtime(&now_c) };
	std::ostringstream oss;
	oss << std::put_time(&tm, "%Y-%m-%dT%H:%M:%SZ");
	return oss.str();
}

void Logger::log(Level level, std::string_view msg)
{
	auto time_str = get_current_time_string();
	std::string level_str;
	switch (level) {
	case Logger::Level::Debug:
		level_str = "DEBUG";
		break;
	case Logger::Level::Info:
		level_str = " INFO";
		break;
	case Logger::Level::Warning:
		level_str = " WARN";
		break;
	case Logger::Level::Error:
		level_str = "ERROR";
		break;
	default:
		std::unreachable();
	}

	auto const msg_file { std::format("{} [{}] {}", time_str, level_str, msg) };
#ifdef _WIN32
	auto const msg_stdout = msg_file;
#elif __EMSCRIPTEN__
	auto const msg_stdout = msg_file;
#else
	char const *color;
	switch (level) {
	case Logger::Level::Debug:
		color = FG_GRAY;
		break;
	case Logger::Level::Info:
		color = FG_BLUE;
		break;
	case Logger::Level::Warning:
		color = FG_YELLOW;
		break;
	case Logger::Level::Error:
		color = FG_RED;
		break;
	default:
		std::unreachable();
	}

	auto const msg_stdout { std::format(
		"{}{} [{}] {}" ANSI_RESET, color, time_str, level_str, msg) };
#endif

#ifndef __EMSCRIPTEN__
	m_fout << msg_file << std::endl;
#endif // EMSCRIPTEN
	std::println(std::cerr, "{}", msg_stdout);
}
