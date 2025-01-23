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

inline const char* cmake_app_name = TOSTRING(APP_NAME);
//inline const char* cmake_user_name = TOSTRING(USER_NAME);
//inline const char* cmake_images_folder = TOSTRING(DEFAULT_IMG_DIR);
//inline const char* cmake_model_path = TOSTRING(DEFAULT_MODEL_DIR);
//inline const char* cmake_processing_scripts_path = TOSTRING(DEFAULT_PROCESSING_SCRIPTS_DIR);
//inline const char* cmake_preset_dir = TOSTRING(DEFAULT_PRESET_DIR);


inline juce::File get_appSupportFolder() {
    juce::File appSupportFolder;

    if (juce::SystemStats::getOperatingSystemType() & juce::SystemStats::OperatingSystemType::Linux) {
        appSupportFolder = juce::File::getSpecialLocation(juce::File::userHomeDirectory).getChildFile(".local/share");
    } else {
        appSupportFolder = juce::File::getSpecialLocation(juce::File::commonApplicationDataDirectory);
    }
    return appSupportFolder;
}

static char* get_images_folder() {
    auto appSupportFolder = get_appSupportFolder();
    // images folder is in the appSupportFolder/app_name/GUI/img
    juce::String images_folder = appSupportFolder.getFullPathName() + path_separator + juce::String(cmake_app_name) + path_separator + "GUI" + path_separator + "img";
    auto images_folder_str = images_folder.toStdString();
    return (char*)images_folder.toStdString().c_str();
}

static char* get_models_folder() {
    auto appSupportFolder = get_appSupportFolder();
    // images folder is in the appSupportFolder/app_name/TorchScripts/Models
    juce::String models_folder = appSupportFolder.getFullPathName() + path_separator + juce::String(cmake_app_name) + path_separator + "TorchScripts" + path_separator + "Models";
    auto models_folder_str = models_folder.toStdString();
    return (char*)models_folder.toStdString().c_str();
}

static char* get_processing_scripts_folder() {
    auto appSupportFolder = get_appSupportFolder();
    // images folder is in the appSupportFolder/app_name/TorchScripts/ProcessingScripts
    juce::String processing_scripts_folder = appSupportFolder.getFullPathName() + path_separator + juce::String(cmake_app_name) + path_separator + "TorchScripts" + path_separator + "ProcessingScripts";
    auto processing_scripts_folder_str = processing_scripts_folder.toStdString();
    return (char*)processing_scripts_folder.toStdString().c_str();
}

static char* get_preset_folder() {
    auto appSupportFolder = get_appSupportFolder();
    // images folder is in the appSupportFolder/app_name/Preset
    juce::String preset_folder = appSupportFolder.getFullPathName() + path_separator + juce::String(cmake_app_name) + path_separator + "Presets";
    auto preset_folder_str = preset_folder.toStdString();
    return (char*)preset_folder.toStdString().c_str();
}

