#pragma once

#include "JuceHeader.h"

#include "../PluginProcessor.h"

#define PRESET_FILE_EXTENSION ".bpf"

class BorealPresetManager
{
public:

    BorealPresetManager (BorealAudioProcessor* inProcessor);
    ~BorealPresetManager();

    void getXmlForPreset (XmlElement* inElement);
    bool loadPresetFromXml (XmlElement* inElement);

    int getNumberOfPresets();
    String getPresetName (int inPresetIndex);
    File getPresetFile (int inPresetIndex) const;

    void createNewPreset();
    void savePreset();
    void saveAsPreset (String inPresetName);
    bool loadPreset (int inPresetIndex);
    bool loadPresetFile (const File& presetFile);

    void getSoundInformation (XmlElement* inElement);
    bool setSoundInformation (XmlElement* presetBody);

    bool getIsCurrentPresetSaved();
    String getCurrentPresetName();

    String getPresetDirectory() const;
    bool setPresetDirectory (const String& inDirectory);
    void resetPresetDirectoryToDefault();

    bool getCrtEnabled() const noexcept { return mCrtEnabled; }
    void setCrtEnabled (bool enabled);

    int getCrtStrength() const noexcept { return mCrtStrength; }
    void setCrtStrength (int strength);

    Array<String> getCollectionsDirectories() const;
    void addCollectionsDirectory (const String& inDirectory);
    void removeCollectionsDirectory (int inIndex);
    void resetCollectionsDirectoriesToDefault();

private:

    void storeLocalPreset();
    void loadPresetDirectorySettings();
    void savePresetDirectorySettings();
    void ensurePresetDirectoryExists();
    void loadCollectionsDirectoriesSettings();
    void saveCollectionsDirectoriesSettings();
    File getSettingsFile() const;
    File getBookmarkFile() const;

    String findSoundFilePath (const String& inRelativePath);

    bool mCurrentPresetIsSaved;

    File mCurrentlyLoadedPreset;

    Array<File> mLocalPresets;

    String mCurrentPresetName;
    String mPresetDirectory;
    Array<String> mCollectionsDirectories;
    bool mCrtEnabled = true;
    int mCrtStrength = 2;
    File mLastSettingsFile;

    XmlElement* mCurrentPresetXml;

    BorealAudioProcessor* mProcessor;
};
