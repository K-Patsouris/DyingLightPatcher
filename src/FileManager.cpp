#include "FileManager.h"
#include "Logger.h"
#include "StringParser.h"

#include "libzippp.h"

#include <fstream>
#include <sstream>

#include <algorithm>

using namespace libzippp;


//	FiloeManager public

bool FileManager::SetDiffPath(const string& str) noexcept { return SetPath(str, true); }

bool FileManager::SetTargetPath(const string& str) noexcept  { return SetPath(str, false); }

bool FileManager::ParseAndCommit() noexcept {
	if (!JustParse()) {
		logger.Error("Parsing failed and no changes were made"sv);
		return false;
	}
	else {
		if (!Commit()) {
			logger.Error("Commit failed"sv);
			return false;
		}
		return true;
	}
}
bool FileManager::ParseWithoutCommit() noexcept {
	if (!JustParse()) {
		logger.Error("Parsing failed and no changes were made"sv);
		return false;
	}
	else {
		logger.Info("Successfully parsed all files. Ready to commit!"sv);
		return true;
	}
}
bool FileManager::Commit() noexcept {
	if (parsed.empty() || targets.empty()) {
		logger.Error("Commit requested but nothing has been parsed yet. Request ignored."sv);
		return false;
	}

	try {
		vector<ZipArchive*> paks; //Open .pak archives as specified in targets
		auto freePaks = [&](bool msg = true) {
			for (auto pak : paks) {
				if (pak->close() != LIBZIPPP_OK && msg) {
					logger.Warning("Archive <{}> was not closed successfully and changes to it might not go through."sv, pak->getPath());
				}
				delete pak;
			}
		};
		paks.reserve(targets.size());
		for (const auto& target : targets) {
			ZipArchive* newpak = new ZipArchive{ target.string() };
			newpak->open(ZipArchive::Write);
			if (!newpak || !newpak->isOpen()) {
				logger.Error("Failed to open .pak file <{}>. Commit aborted with no changes made."sv, target.string());
				parsed.clear();
				if (newpak) { freePaks(false); }
				return false;
			}
			paks.push_back(newpak);
		}

		vector<string> temp_filenames{};
		temp_filenames.reserve(parsed.size());
		for (const auto& p : parsed) {
			bool handled{ false };
			for (auto pak : paks) {
				if (pak->hasEntry(p.first, false, false)) { //Case insensitive
					//Put parsed string in a temp file
					string temp_filename{ path{ p.first }.filename().string() + ".temp" };
					std::ofstream ofs{ temp_filename };
					if (!ofs.is_open()) {
						logger.Error("Failed to create temp file <{}>. Commit aborted but some files may have been patched."sv, temp_filename);
						freePaks();
						return false;
					}
					ofs << p.second;
					ofs.close();
					//Add the temp file to the archive
					if (!pak->addFile(p.first, temp_filename)) {
						logger.Error("Failed to add parsed <{}> to archive. Commit aborted but some files may have been patched."sv, path{ p.first }.filename().string());
						freePaks();
						return false;
					}
					temp_filenames.push_back(std::move(temp_filename));
					//Move on to the next parsed string
					handled = true;
					break;
				}
			}

			if (!handled) {
				logger.Error("Failed to find file <{}> in any of the archives. Commit aborted but some files may have been patched."sv);
				freePaks();
				return false;
			}
		}

		//Close and free archives
		freePaks();

		//Delete the temp files
		for (const auto& tmp : temp_filenames) {
			std::error_code ec{};
			if (!std::filesystem::remove(path{ tmp }, ec)) {
				if (ec.value() == 0) {
					logger.Warning("Failed to delete temp file <{}> after commiting with no specified error."sv, tmp);
				}
				else {
					logger.Warning("Failed to delete temp file <{}> after commiting with OS error <{}>."sv, tmp, ec.message());
				}
			}
		}

		//Reset after commit
		logger.Info("Successfully committed {} parsed files!"sv, parsed.size());
		Reset();
		return true;
	}
	catch (...) {
		logger.Error("Unspecified exception during commit. Commit aborted but some files may have been patched");
		return false;
	}
}

void FileManager::ToFiles() noexcept {
	for (const auto& p : parsed) {
		std::ofstream ofs{ "PARSED_" + path{ p.first }.filename().string() };
		if (!ofs.is_open()) {
			logger.Warning("Failed to dump parsed <{}> to file"sv, path{ p.first }.filename().string());
			continue;
		}
		ofs << p.second;
		ofs.close();
	}
}

void FileManager::Reset() noexcept {
	diffs.clear();
	targets.clear();
	parsed.clear();
}



//	FileManager private

bool FileManager::JustParse() noexcept {
	if (diffs.empty() || targets.empty()) {
		logger.Error("Both a folder of diff files and a folder of .pak files must be provided before parsing. Request ignored."sv);
		return false;
	}
	parsed.clear();
	parsed.reserve(diffs.size());

	try {
		vector<ZipArchive*> paks; //Open .pak archives as specified in targetsauto freePaks = [&](bool msg = true) {
		auto freePaks = [&](bool msg = true) {
			for (auto pak : paks) {
				if (pak->close() != LIBZIPPP_OK && msg) {
					logger.Warning("Archive <{}> was not closed successfully and changes to it might not go through."sv, pak->getPath());
				}
				delete pak;
			}
		};
		paks.reserve(targets.size());
		for (const auto& target : targets) {
			ZipArchive* newpak = new ZipArchive{ target.string() };
			newpak->open(ZipArchive::ReadOnly);
			if (!newpak || !newpak->isOpen()) {
				logger.Error("Failed to open .pak file <{}>. Parse aborted."sv, target.string());
				parsed.clear();
				if (newpak) { freePaks(false); }
				return false;
			}
			paks.push_back(newpak);
		}

		StringParser::Parser parser{};
		for (const auto& diff : diffs) {
			//Get and set diff file string
			std::ifstream ifs{ diff };
			if (!ifs.is_open()) {
				logger.Error("Failed to open diff <{}>. Parse aborted."sv, diff.string());
				parsed.clear();
				freePaks();
				return false;
			}
			std::stringstream ss{};
			ss << ifs.rdbuf();
			const string diff_str{ ss.str() };
			ifs.close();
			if (!parser.SetDiff(diff_str)) {
				logger.Error("Failed to set diff <{}>. Parse aborted."sv, diff.string());
				parsed.clear();
				freePaks();
				return false;
			}

			//Get and set target file string
			const string path_of_target{ parser.GetTargetPath() };
			ZipEntry target_entry{};
			for (const auto pak : paks) {
				target_entry = pak->getEntry(path_of_target);
				if (!target_entry.isNull()) {
					break;
				}
			}
			if (target_entry.isNull()) {
				logger.Error("Failed to locate target <{}> requested in diff <{}>. Parse aborted."sv, path_of_target, diff.string());
				parsed.clear();
				freePaks();
				return false;
			}
			if (!parser.SetTarget(target_entry.readAsText())) {
				logger.Error("Failed to set target <{}>. Parse aborted."sv, path_of_target);
				parsed.clear();
				freePaks();
				return false;
			}

			//Parse and store parse data to parsed
			std::pair<string, string> diff_data{ path_of_target, "" };
			if (!parser.Parse(diff_data.second)) {
				logger.Error("Failed to parse <{}>. Parse aborted."sv, diff.string());
				parsed.clear();
				freePaks();
				return false;
			}
			parsed.push_back(std::move(diff_data));
		}

		freePaks();
		return true;
	}
	catch (...) {
		parsed.clear();
		logger.Error("Unknown exception while trying to parse. Parse aborted."sv);
		return false;
	}
}

vector<path>& FileManager::GetPathVec(bool diff) noexcept  { return (diff ? diffs : targets); }

bool FileManager::SetPath(const string& str, bool diff) noexcept {
	try {
		path newpath = str;
		if (!exists(newpath)) {
			logger.Error("Path <{}> does not exist"sv, str);
			return false;
		}
		if (!is_directory(newpath)) {
			logger.Error("Path <{}> is not a directory"sv, str);
			return false;
		}
		if (is_empty(newpath)) {
			logger.Error("Path <{}> is empty"sv, str);
			return false;
		}

		if (diff) {
			diffs.clear();
			for (const auto& dirEntry : directory_iterator{ newpath }) {
				if (dirEntry.is_regular_file()) {
					diffs.push_back(dirEntry.path());
				}
			}
			if (!diffs.empty()) {
				logger.Info("Path <{}> contains {} (diff?) files!"sv, str, diffs.size());
				return true;
			}
			else {
				logger.Error("Path <{}> contains no files"sv, str);
				return false;
			}
		}
		else {
			targets.clear();
			for (const auto& dirEntry : directory_iterator{ newpath }) {
				if (dirEntry.is_regular_file() && dirEntry.path().extension() == ".pak") {
					targets.push_back(dirEntry.path());
				}
			}
			if (!targets.empty()) {
				logger.Info("Path <{}> contains {} .pak files!"sv, str, targets.size());
				return true;
			}
			else {
				logger.Error("Path <{}> contains no .pak files"sv, str);
				return false;
			}
		}
	}
	catch (...) {
		logger.Error("filesystem::path constructors unexpectedly failed"sv, str);
		return false;
	}
}





