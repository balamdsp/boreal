#pragma once

#include "JuceHeader.h"

const static std::string PLUGIN_NAME = ProjectInfo::projectName;
const static std::string PLUGIN_VERSION = ProjectInfo::versionString;

const static int GUI_REFRESH_TIMER = 100;
const static int XYPAD_UI_REFRESH_TIMER_CALLBACK = 33;
const static int GUI_REFRESH_TIMER_CALLBACK_SOUND = GUI_REFRESH_TIMER;
const static int SOUND_PLAYHEAD_REFRESH_TIMER = 33;

inline File getDefaultPluginDataDirectory()
{
    return File::getSpecialLocation (File::userApplicationDataDirectory)
        .getChildFile ("BalamDSP")
        .getChildFile (PLUGIN_NAME);
}

inline String getDefaultCollectionsDirectory()
{
    return getDefaultPluginDataDirectory().getChildFile ("Sounds").getFullPathName();
}

inline String getDefaultPresetsDirectory()
{
    return getDefaultPluginDataDirectory().getChildFile ("Presets").getFullPathName();
}

inline String getDefaultAnalyzerDirectory()
{
    return getDefaultPluginDataDirectory().getChildFile ("Analyzer").getFullPathName();
}

static const String PRESET_DIRECTORY_SETTINGS_KEY = "presetDirectory";
static const String COLLECTIONS_DIRECTORIES_SETTINGS_KEY = "collectionsDirectories";
static const String CRT_ENABLED_SETTINGS_KEY = "crtEnabled";
static const String CRT_STRENGTH_SETTINGS_KEY = "crtStrength";

const static String PRESET_NAME_PARAMETER_ID = "PresetName";
const static String SOUND_FILE_PATH_PARAMETER_ID = "MorphSoundFilePath";

namespace BorealZoom
{
    inline float uiScale = 1.0f;
}
