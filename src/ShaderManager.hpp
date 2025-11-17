#pragma once
#include <string>
#include <vector>
#include <filesystem>
#include <amethyst/Log.hpp>
#include <fstream>
#include <amethyst/Utility.hpp>
#include <json/json.h>
namespace fs = std::filesystem;
class ShaderLibrary {
private:
	fs::path mRootPath;
	std::string name;
public:
	ShaderLibrary(fs::path path) : mRootPath(path) {}
	~ShaderLibrary() {}

	bool init() {
		try {
			std::string content = ReadTextFile(mRootPath / "shader.json");
			auto json = Json::Value();
			auto reader = Json::Reader::Reader();
			if (reader.parse(content, json, false)) {
				if (json.isNull()) {
					Log::Error("Error reading shader.json, json is null");
					return false;
				}
				auto name = json.get("name", "").asString();
				if (name.empty()) {
					Log::Error("Error reading shader.json, required key not found 'name'");
				}

				this->name = name;

				return true;
			}
			else {
				Log::Error("Error reading shader.json, failed parsing");
				return false;
			}
		}
		catch (const std::runtime_error e) {
			Log::Error("Error reading shader.json: {}", e.what());
			return false;
		}
	}

	bool rerouteIfExists(std::string& path) const {
		fs::path folder = mRootPath;
		fs::path file = path;

		fs::path fullPath = folder / file;
		if (fs::is_regular_file(fullPath)) {
			Log::Info("fileExists: {}", fullPath.generic_string().c_str());
			path = fullPath.generic_string();
			return true;
		}
		return false;
	}
};

class ShaderManager
{
public:
	std::vector<ShaderLibrary*> shaders;
	std::string mSourcePath;
	std::string mShadersPath;
	ShaderManager(std::string sourcePath, std::string shadersPath) : mSourcePath(sourcePath), mShadersPath(shadersPath)
	{
		loadShaders();
	}
	~ShaderManager() {}

	void loadShaders() {
		// Create the folder if not exists
		if (!fs::is_directory(fs::path(mShadersPath))) {
			fs::create_directories(mShadersPath);
		}

		// Create the shaderlist file if not exists
		auto& shaderListPath = fs::path(mShadersPath).append("shaderlist.txt");
		if (!fs::is_regular_file(shaderListPath)) {
			// Creates an empty file
			Log::Info("Creating shaderlist.txt");
			std::ofstream file(shaderListPath.c_str());
		}

		// Loop through all folders{
		for (const auto& entry : fs::directory_iterator(mShadersPath)) {
			if (entry.is_directory()) {
				const auto& path = entry.path();
				auto* shaderLib = _loadShader(path);
				if (shaderLib != nullptr) {
					shaders.push_back(shaderLib);
				}
			}
		}
	}

	bool handleLoadFile(std::string& path) {
		auto relPath = path.substr(mSourcePath.size() + 1);
		for (const auto* shad : shaders) {
			if (shad->rerouteIfExists(relPath))
				path = relPath;
			return true;
		}
		return false;
	}

private:
	ShaderLibrary* _loadShader(const fs::path& path) {
		if (fs::exists(path / "shader.json")) {
			auto folderName = path.filename().string();
			Log::Info("Loading shader: {}", folderName);
			auto* result = new ShaderLibrary(path.generic_string());
			if (result->init())
				return result;
		}
		return nullptr;
	}
};