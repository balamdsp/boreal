#include "BorealPresetManager.h"

#include "../Helpers/BorealConstants.h"

BorealPresetManager::BorealPresetManager (BorealAudioProcessor* inProcessor)
:   mCurrentPresetIsSaved (false),
    mCurrentPresetName ("Untitled"),
    mPresetDirectory (getDefaultPresetsDirectory()),
    mProcessor (inProcessor)
{
    loadPresetDirectorySettings();
    loadCollectionsDirectoriesSettings();
    ensurePresetDirectoryExists();
    storeLocalPreset();

    if (mCollectionsDirectories.isEmpty())
    {
        resetCollectionsDirectoriesToDefault();
    }
}

BorealPresetManager::~BorealPresetManager()
{
    savePresetDirectorySettings();
}

void BorealPresetManager::getXmlForPreset (XmlElement* inElement)
{

    inElement->setAttribute (PRESET_NAME_PARAMETER_ID, mCurrentPresetName);

    this->getSoundInformation (inElement);

    auto& parameters = mProcessor->getParameters();

    for (int i=0; i < parameters.size(); i++)
    {
        AudioProcessorParameterWithID* parameter =
        (AudioProcessorParameterWithID*) parameters.getUnchecked (i);

        inElement->setAttribute (parameter->paramID,
                                 parameter->getValue());
    }
}

bool BorealPresetManager::loadPresetFromXml (XmlElement* inElement)
{
    mCurrentPresetXml = inElement;
    bool present_was_loaded = false;

    bool sounds_were_loaded = this->setSoundInformation (inElement);

    if (sounds_were_loaded)
    {
        auto& parameters = mProcessor->getParameters();

        for (int i=0; i < mCurrentPresetXml->getNumAttributes(); i++)
        {
            const String paramId = mCurrentPresetXml->getAttributeName (i);
            const float value = static_cast<float> (mCurrentPresetXml->getDoubleAttribute (paramId));

        for (int j=0; j < parameters.size(); j++)
        {
            AudioProcessorParameterWithID* parameter =
            (AudioProcessorParameterWithID*) parameters.getUnchecked(j);

            if (paramId == parameter->paramID)
            {
                parameter->setValueNotifyingHost(value);
            }
        }
        }

        mCurrentPresetName = inElement->getStringAttribute (PRESET_NAME_PARAMETER_ID);

        present_was_loaded = true;
    }

    return present_was_loaded;
}

int BorealPresetManager::getNumberOfPresets()
{
    return mLocalPresets.size();
}

String BorealPresetManager::getPresetName (int inPresetIndex)
{
    if (isPositiveAndBelow (inPresetIndex, mLocalPresets.size()))
        return mLocalPresets[inPresetIndex].getFileNameWithoutExtension();
    return {};
}

File BorealPresetManager::getPresetFile (int inPresetIndex) const
{
    if (isPositiveAndBelow (inPresetIndex, mLocalPresets.size()))
        return mLocalPresets[inPresetIndex];
    return {};
}

void BorealPresetManager::createNewPreset()
{
    auto& parameters = mProcessor->getParameters();

    for (int i=0; i < parameters.size(); i++)
    {
        AudioProcessorParameterWithID* parameter =
        (AudioProcessorParameterWithID*) parameters.getUnchecked(i);

        const float defaultValue =
        parameter->getDefaultValue();

        parameter->setValueNotifyingHost (defaultValue);
    }

    mProcessor->clearAllSlots();

    mCurrentPresetIsSaved = false;
    mCurrentPresetName = "Untitled";
    mCurrentlyLoadedPreset = File();
}

void BorealPresetManager::savePreset()
{
    if (mCurrentlyLoadedPreset.existsAsFile())
    {
        MemoryBlock destinationData;
        mProcessor->getStateInformation (destinationData);
        mCurrentlyLoadedPreset.replaceWithData (destinationData.getData(),
                                                destinationData.getSize());
        mCurrentPresetIsSaved = true;
    }
    else
    {
        saveAsPreset (mCurrentPresetName);
    }
}

void BorealPresetManager::saveAsPreset (String inPresetName)
{
    const String legalName = File::createLegalFileName (inPresetName).trim();
    if (legalName.isEmpty() || legalName == "." || legalName == "..")
        return;

    File presetFile = File (mPresetDirectory).getChildFile (legalName + PRESET_FILE_EXTENSION);

    MemoryBlock destinationData;
    mProcessor->getStateInformation (destinationData);
    presetFile.replaceWithData (destinationData.getData(),
                                destinationData.getSize());

    mCurrentlyLoadedPreset = presetFile;
    mCurrentPresetIsSaved = true;
    mCurrentPresetName = legalName;

    storeLocalPreset();
}

bool BorealPresetManager::loadPreset (int inPresetIndex)
{
    if (! isPositiveAndBelow (inPresetIndex, mLocalPresets.size()))
        return false;

    return loadPresetFile (mLocalPresets[inPresetIndex]);
}

bool BorealPresetManager::loadPresetFile (const File& presetFile)
{
    if (! presetFile.existsAsFile())
        return false;

    MemoryBlock presetBinary;

    if (! presetFile.loadFileAsData (presetBinary))
        return false;

    std::unique_ptr<XmlElement> xmlState = juce::AudioPluginInstance::getXmlFromBinary (presetBinary.getData(), (int) presetBinary.getSize());

    if (xmlState == nullptr)
    {
        jassertfalse;
        return false;
    }

    if (xmlState->hasTagName ("BOREAL_STATE"))
    {
        juce::StringArray missing;
        for (int i = 0; i < MorphEngine::numSlots; ++i)
        {
            const juce::String path = xmlState->getStringAttribute ("slot" + juce::String (i + 1));
            if (path.isEmpty() || juce::File (path).existsAsFile())
                continue;

            bool foundInCollections = false;
            for (const auto& collectionsDir : mCollectionsDirectories)
            {
                if (juce::File (juce::File (collectionsDir).getChildFile (path).getFullPathName()).existsAsFile())
                {
                    foundInCollections = true;
                    break;
                }
            }
            if (! foundInCollections)
                missing.add (path);
        }

        if (! missing.isEmpty())
        {
            AlertWindow aux ("Sounds not found", "", AlertWindow::NoIcon);
            aux.showMessageBox (AlertWindow::WarningIcon, "Sounds not found",
                                "The preset couldn't be loaded, the following sounds are not found:\n\n" + missing.joinIntoString ("\n"),
                                "Accept");
            return false;
        }

        mProcessor->setStateInformation (presetBinary.getData(), (int) presetBinary.getSize());
        mCurrentPresetIsSaved = true;
        mCurrentPresetName = presetFile.getFileNameWithoutExtension();
        mCurrentlyLoadedPreset = presetFile;
        return true;
    }

    bool hasLegacyBody = xmlState->hasAttribute (PRESET_NAME_PARAMETER_ID);
    for (int i = 0; i < MorphEngine::numSlots && ! hasLegacyBody; ++i)
        hasLegacyBody = xmlState->hasAttribute (SOUND_FILE_PATH_PARAMETER_ID + juce::String (i + 1));

    if (hasLegacyBody && loadPresetFromXml (xmlState.get()))
    {
        mCurrentPresetIsSaved = true;
        mCurrentPresetName = presetFile.getFileNameWithoutExtension();
        mCurrentlyLoadedPreset = presetFile;
        return true;
    }

    bool present_was_loaded = false;

    for (auto* subChild : xmlState->getChildIterator())
    {
        present_was_loaded = this->loadPresetFromXml (subChild);

        if (present_was_loaded)
        {
            mCurrentPresetIsSaved = true;
            mCurrentPresetName = presetFile.getFileNameWithoutExtension();
            mCurrentlyLoadedPreset = presetFile;
        }
    }

    return present_was_loaded;
}

void BorealPresetManager::getSoundInformation (XmlElement* inElement)
{
    for (int i = 0; i < MorphEngine::numSlots; i++)
    {

        String sound_file_path = mProcessor->getSlotFilePath (i);
        String sound_file_path_id = SOUND_FILE_PATH_PARAMETER_ID + String (i + 1);
        inElement->setAttribute (sound_file_path_id, sound_file_path);
    }
}

bool BorealPresetManager::setSoundInformation (XmlElement* presetBody)
{
    bool sounds_were_loaded = true;

    std::vector<std::string> sound_file_paths_to_load;

    String error_message = "The preset couldn't be loaded, the following sounds are not found:\n";

    for (int i = 0; i < MorphEngine::numSlots; i++)
    {

        String sound_file_path_id = SOUND_FILE_PATH_PARAMETER_ID + String (i + 1);

        std::string sound_file_path = presetBody->getStringAttribute (sound_file_path_id).toStdString();

        if (!sound_file_path.empty())
        {

            File sound_file_path_check (sound_file_path);

            if (!sound_file_path_check.existsAsFile())
            {

                bool found_in_collections = false;

                for (const auto& collectionsDir : mCollectionsDirectories)
                {
                    const String fullPath = File (collectionsDir).getChildFile (sound_file_path).getFullPathName();
                    File altFile (fullPath);

                    if (altFile.existsAsFile())
                    {
                        sound_file_path = fullPath.toStdString();
                        found_in_collections = true;
                        break;
                    }
                }

                if (!found_in_collections)
                {
                    sounds_were_loaded = false;
                    error_message += String ("\n" + sound_file_path);
                    continue;
                }
            }

            sound_file_paths_to_load.push_back (sound_file_path);
        }
    }

    if (sounds_were_loaded)
    {

        this->mProcessor->clearAllSlots();

        for (size_t i = 0; i < sound_file_paths_to_load.size(); ++i)
        {
            this->mProcessor->loadSlot ((int) i, File (sound_file_paths_to_load[i]));
        }
    }
    else
    {
        AlertWindow aux ("Sounds not found", "", AlertWindow::NoIcon);
        aux.showMessageBox (AlertWindow::WarningIcon, "Sounds not found", error_message, "Accept");
    }

    return sounds_were_loaded;
}

bool BorealPresetManager::getIsCurrentPresetSaved()
{
    return mCurrentPresetIsSaved;
}

String BorealPresetManager::getCurrentPresetName()
{
    return mCurrentPresetName;
}

void BorealPresetManager::storeLocalPreset()
{
    mLocalPresets.clear();

    File dir (mPresetDirectory);
    if (dir.isDirectory())
    {
        for (auto& entry : RangedDirectoryIterator (dir, true, "*" + String (PRESET_FILE_EXTENSION)))
            mLocalPresets.add (entry.getFile());
        mLocalPresets.sort();
    }
}

String BorealPresetManager::getPresetDirectory() const
{
    return mPresetDirectory;
}

bool BorealPresetManager::setPresetDirectory (const String& inDirectory)
{
    if (inDirectory.isEmpty())
        return false;

    const File presetDirectory (inDirectory);

    if (! presetDirectory.isDirectory())
        return false;

    mPresetDirectory = presetDirectory.getFullPathName();
    savePresetDirectorySettings();
    storeLocalPreset();

    return true;
}

void BorealPresetManager::resetPresetDirectoryToDefault()
{
    mPresetDirectory = getDefaultPresetsDirectory();
    ensurePresetDirectoryExists();
    savePresetDirectorySettings();
    storeLocalPreset();
}

void BorealPresetManager::setCrtEnabled (bool enabled)
{
    mCrtEnabled = enabled;
    savePresetDirectorySettings();
}

void BorealPresetManager::setCrtStrength (int strength)
{
    mCrtStrength = jlimit (0, 2, strength);
    savePresetDirectorySettings();
}

File BorealPresetManager::getSettingsFile() const
{
    return File (mPresetDirectory).getParentDirectory().getChildFile ("settings.xml");
}

File BorealPresetManager::getBookmarkFile() const
{
    return File::getSpecialLocation (File::userApplicationDataDirectory)
        .getChildFile ("BalamDSP")
        .getChildFile (PLUGIN_NAME)
        .getChildFile ("settings.xml");
}

static void readBorealSettingsXml (const XmlElement& xml, String& presetDirectory,
                                   Array<String>& collectionsDirectories,
                                   bool& crtEnabled, int& crtStrength)
{
    const String dir = xml.getStringAttribute (PRESET_DIRECTORY_SETTINGS_KEY, "");
    if (dir.isNotEmpty() && File (dir).isDirectory())
        presetDirectory = dir;

    const String savedDirectories = xml.getStringAttribute (COLLECTIONS_DIRECTORIES_SETTINGS_KEY, "");
    if (savedDirectories.isNotEmpty())
    {
        StringArray dirArray;
        dirArray.addTokens (savedDirectories, ";", "");

        for (const auto& d : dirArray)
        {
            String trimmedDir = d.trim();
            if (trimmedDir.isNotEmpty() && File (trimmedDir).isDirectory())
                collectionsDirectories.addIfNotAlreadyThere (trimmedDir);
        }
    }

    crtEnabled = xml.getBoolAttribute (CRT_ENABLED_SETTINGS_KEY, crtEnabled);
    crtStrength = jlimit (0, 2, xml.getIntAttribute (CRT_STRENGTH_SETTINGS_KEY, crtStrength));
}

void BorealPresetManager::loadPresetDirectorySettings()
{
    {
        const File bookmarkFile = getBookmarkFile();
        if (bookmarkFile.existsAsFile())
            if (std::unique_ptr<XmlElement> xml = XmlDocument::parse (bookmarkFile))
                readBorealSettingsXml (*xml, mPresetDirectory, mCollectionsDirectories,
                                       mCrtEnabled, mCrtStrength);
    }

    if (mPresetDirectory.isEmpty() || ! File (mPresetDirectory).isDirectory())
    {
        resetPresetDirectoryToDefault();
        return;
    }

    mLastSettingsFile = getSettingsFile();
    if (mLastSettingsFile.existsAsFile())
        if (std::unique_ptr<XmlElement> xml = XmlDocument::parse (mLastSettingsFile))
            readBorealSettingsXml (*xml, mPresetDirectory, mCollectionsDirectories,
                                   mCrtEnabled, mCrtStrength);
}

void BorealPresetManager::savePresetDirectorySettings()
{
    const File settingsFile = getSettingsFile();
    settingsFile.getParentDirectory().createDirectory();

    String directoriesString;
    for (int i = 0; i < mCollectionsDirectories.size(); ++i)
    {
        if (i > 0)
            directoriesString += ";";
        directoriesString += mCollectionsDirectories[i];
    }

    XmlElement xml ("BorealSettings");
    xml.setAttribute (PRESET_DIRECTORY_SETTINGS_KEY, mPresetDirectory);
    xml.setAttribute (COLLECTIONS_DIRECTORIES_SETTINGS_KEY, directoriesString);
    xml.setAttribute (CRT_ENABLED_SETTINGS_KEY, mCrtEnabled);
    xml.setAttribute (CRT_STRENGTH_SETTINGS_KEY, mCrtStrength);
    xml.writeTo (settingsFile);

    const File bookmarkFile = getBookmarkFile();
    if (bookmarkFile != settingsFile)
    {
        bookmarkFile.getParentDirectory().createDirectory();
        xml.writeTo (bookmarkFile);
    }

    if (mLastSettingsFile.existsAsFile()
        && mLastSettingsFile != settingsFile
        && mLastSettingsFile != bookmarkFile)
        mLastSettingsFile.deleteFile();

    mLastSettingsFile = settingsFile;
}

void BorealPresetManager::ensurePresetDirectoryExists()
{
    File (mPresetDirectory).createDirectory();
}

Array<String> BorealPresetManager::getCollectionsDirectories() const
{
    return mCollectionsDirectories;
}

void BorealPresetManager::addCollectionsDirectory (const String& inDirectory)
{
    if (inDirectory.isEmpty())
        return;

    const File dir (inDirectory);

    if (!dir.isDirectory())
        return;

    const String fullPath = dir.getFullPathName();

    for (const auto& existingDir : mCollectionsDirectories)
    {
        if (existingDir == fullPath)
            return;
    }

    mCollectionsDirectories.add (fullPath);
    saveCollectionsDirectoriesSettings();
}

void BorealPresetManager::removeCollectionsDirectory (int inIndex)
{
    if (inIndex < 0 || inIndex >= mCollectionsDirectories.size())
        return;

    mCollectionsDirectories.remove (inIndex);
    saveCollectionsDirectoriesSettings();
}

void BorealPresetManager::resetCollectionsDirectoriesToDefault()
{
    mCollectionsDirectories.clear();
    mCollectionsDirectories.add (getDefaultCollectionsDirectory());

    File (getDefaultCollectionsDirectory()).createDirectory();

    saveCollectionsDirectoriesSettings();
}

void BorealPresetManager::loadCollectionsDirectoriesSettings()
{

}

void BorealPresetManager::saveCollectionsDirectoriesSettings()
{
    savePresetDirectorySettings();
}

String BorealPresetManager::findSoundFilePath (const String& inRelativePath)
{

    for (const auto& collectionsDir : mCollectionsDirectories)
    {
        const String fullPath = File (collectionsDir).getChildFile (inRelativePath).getFullPathName();
        File soundFile (fullPath);

        if (soundFile.existsAsFile())
        {
            return fullPath;
        }
    }

    return "";
}
