#pragma once

#include <torch/script.h> // One-stop header.
#include <utility>
#include "shared_plugin_helpers/shared_plugin_helpers.h"

#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)

#if defined(_WIN32) || defined(_WIN64)
inline const char* path_separator = R"(\)";
#else
inline const char* path_separator = "/";
#endif

inline const char* cmake_root_install_dir = TOSTRING(ROOT_INSTALL_DIR);
inline const char* cmake_user_name = TOSTRING(USER_NAME);
inline const char* cmake_images_folder = TOSTRING(DEFAULT_IMG_DIR);
inline const char* cmake_model_path = TOSTRING(DEFAULT_MODEL_DIR);
inline const char* cmake_processing_scripts_path = TOSTRING(DEFAULT_PROCESSING_SCRIPTS_DIR);
inline const char* cmake_preset_dir = TOSTRING(DEFAULT_PRESET_DIR);

static std::string GetPluginDirectory() {
    juce::File pluginPath = juce::File::getSpecialLocation(juce::File::currentExecutableFile);
    return pluginPath.getParentDirectory().getFullPathName().toStdString();

}

static std::string getUserDirectory() {
    // Get the user's home directory
    juce::File homeDirectory = juce::File::getSpecialLocation(juce::File::userHomeDirectory);
    return  homeDirectory.getFullPathName().toStdString();
}

static std::string getCurrentUserName() {
    auto user_dir = SystemStats::getFullUserName();
    return user_dir.toStdString();
}

static char* get_images_folder() {
    // Prepare folder path and replace placeholder with the current username
    std::string img_folder = std::string(cmake_images_folder); // Assuming cmake_images_folder is defined
    std::string cmake_user_name_ = std::string(cmake_user_name); // Assuming cmake_user_name is defined
    std::string current_user_name = getCurrentUserName(); // Retrieve current username

    // Replace cmake_user_name with current_user_name in img_folder
    size_t pos = img_folder.find(cmake_user_name_);
    if (pos != std::string::npos) {
        img_folder.replace(pos, cmake_user_name_.length(), current_user_name);
    }

    // Convert std::string back to char* for return (C-style string)
    char* result = new char[img_folder.length() + 1];
    std::strcpy(result, img_folder.c_str());

    return result; // Caller is responsible for freeing the allocated memory
}

static char* get_model_path() {
    // Prepare folder path and replace placeholder with the current username
    std::string model_path = std::string(cmake_model_path); // Assuming cmake_model_path is defined
    std::string cmake_user_name_ = std::string(cmake_user_name); // Assuming cmake_user_name is defined
    std::string current_user_name = getCurrentUserName(); // Retrieve current username

    // Replace cmake_user_name with current_user_name in model_path
    size_t pos = model_path.find(cmake_user_name_);
    if (pos != std::string::npos) {
        model_path.replace(pos, cmake_user_name_.length(), current_user_name);
    }

    // Convert std::string back to char* for return (C-style string)
    char* result = new char[model_path.length() + 1];
    std::strcpy(result, model_path.c_str());

    return result; // Caller is responsible for freeing the allocated memory
}

static char* get_processing_scripts_path() {
    // Prepare folder path and replace placeholder with the current username
    std::string processing_scripts_path = std::string(cmake_processing_scripts_path); // Assuming cmake_processing_scripts_path is defined
    std::string cmake_user_name_ = std::string(cmake_user_name); // Assuming cmake_user_name is defined
    std::string current_user_name = getCurrentUserName(); // Retrieve current username

    // Replace cmake_user_name with current_user_name in processing_scripts_path
    size_t pos = processing_scripts_path.find(cmake_user_name_);
    if (pos != std::string::npos) {
        processing_scripts_path.replace(pos, cmake_user_name_.length(), current_user_name);
    }

    // Convert std::string back to char* for return (C-style string)
    char* result = new char[processing_scripts_path.length() + 1];
    std::strcpy(result, processing_scripts_path.c_str());

    return result; // Caller is responsible for freeing the allocated memory
}

static char* get_preset_dir() {
    // Prepare folder path and replace placeholder with the current username
    std::string preset_dir = std::string(cmake_preset_dir); // Assuming cmake_preset_dir is defined
    std::string cmake_user_name_ = std::string(cmake_user_name); // Assuming cmake_user_name is defined
    std::string current_user_name = getCurrentUserName(); // Retrieve current username

    // Replace cmake_user_name with current_user_name in preset_dir
    size_t pos = preset_dir.find(cmake_user_name_);
    if (pos != std::string::npos) {
        preset_dir.replace(pos, cmake_user_name_.length(), current_user_name);
    }

    // Convert std::string back to char* for return (C-style string)
    char* result = new char[preset_dir.length() + 1];
    std::strcpy(result, preset_dir.c_str());

    return result; // Caller is responsible for freeing the allocated memory
}


// images_folder, default_model_path, default_processing_scripts_path, default_preset_dir
inline const char*  images_folder = get_images_folder();
inline const char*  default_model_path = get_model_path();
inline const char*  default_processing_scripts_path = get_processing_scripts_path();
inline const char*  default_preset_dir = get_preset_dir();
