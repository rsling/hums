// What a save does to the backup of the save before (docs/SCHEMA.md ->
// Save). Finding 18 of the milestone 1 code review (23 Nov 2026): a save made
// the new <name>.bak before the rename that is the save, so a save whose
// rename failed had already put the older backup away; and a backup its owner
// had marked read-only was replaced without a word, where a read-only project
// file is refused. Since 24 Nov 2026 the backup is replaced only once the
// project is saved, and a read-only backup is refused.

#include <catch2/catch_test_macros.hpp>

#include "io/ProjectFile.h"
#include "model/Ids.h"

#include <functional>
#include <string>

namespace
{

namespace ids = model::ids;
using SaveOutcome = io::ProjectFile::SaveResult::Outcome;

// A directory of its own for one test, removed with everything in it when
// the test ends.
struct BackupTestDirectory
{
    BackupTestDirectory()
    {
        REQUIRE (directory.createDirectory().wasOk());
    }

    ~BackupTestDirectory()
    {
        // A test may have made a file read-only.
        directory.setReadOnly (false, true);
        directory.deleteRecursively();
    }

    juce::File file (const juce::String& name) const { return directory.getChildFile (name); }

    juce::StringArray fileNames() const
    {
        juce::StringArray names;

        for (const auto& found : directory.findChildFiles (juce::File::findFilesAndDirectories, false))
            names.add (found.getFileName());

        names.sort (false);

        return names;
    }

    // Named with a UUID. "The first name that is free" is the same name for
    // two tests that ask at the same moment (ctest -j), and each then took
    // the other's directory for its own.
    const juce::File directory { juce::File::getSpecialLocation (juce::File::tempDirectory)
                                     .getChildFile ("hums_save_backup_test_" + juce::Uuid().toString()) };
};

// The valid fixture under another project name.
juce::ValueTree projectNamed (const juce::String& name)
{
    const auto fixture = juce::File (HUMS_IO_FIXTURES_DIRECTORY).getChildFile ("berlin_test.xml");
    REQUIRE (fixture.existsAsFile());

    juce::ValueTree document;
    juce::UndoManager undoManager;
    REQUIRE (io::ProjectFile::load (fixture, document, undoManager).wasLoaded());
    document.setProperty (ids::name, name, nullptr);

    return document;
}

std::string canonical (const juce::ValueTree& project)
{
    return io::ProjectFile::toCanonicalXml (project).toStdString();
}

std::string textOf (const juce::File& file)
{
    return file.loadFileAsString().toStdString();
}

// A move that fails for one target and is the real one for every other. No
// file on disk makes one of the two renames of a save fail and not the other:
// what would stop the rename over the target (a lock, an access control
// entry) is seen before anything is written, or is copied to the backup with
// the file.
struct MoveThatFailsOver
{
    bool operator() (const juce::File& newFile, const juce::File& target)
    {
        if (target == targetThatFails)
        {
            ++timesFailed;
            return false;
        }

        return io::ProjectFile::moveFileOver (newFile, target);
    }

    juce::File targetThatFails;
    int timesFailed = 0;
};

// A file saved twice: it holds the second project, its backup the first.
struct SavedTwice
{
    SavedTwice()
    {
        REQUIRE (io::ProjectFile::save (first, file).wasSaved());
        REQUIRE (io::ProjectFile::save (second, file).wasSaved());
        REQUIRE (textOf (file) == canonical (second));
        REQUIRE (textOf (backup) == canonical (first));
    }

    const BackupTestDirectory directory;
    const juce::File file { directory.file ("song.xml") };
    const juce::File backup { directory.file ("song.xml.bak") };
    const juce::ValueTree first { projectNamed ("First") };
    const juce::ValueTree second { projectNamed ("Second") };
    const juce::ValueTree third { projectNamed ("Third") };
};

} // namespace

TEST_CASE ("A save whose rename fails leaves the backup of the save before as it was", "[io][ProjectFile][save][backup]")
{
    const SavedTwice saved;
    MoveThatFailsOver move { saved.file };

    const auto result = io::ProjectFile::save (saved.third, saved.file, std::ref (move));

    // The case in question: the save came as far as the rename and failed
    // there. Any other outcome is another case.
    INFO ("the save reported: " << result.message.toStdString());
    REQUIRE (move.timesFailed == 1);
    REQUIRE_FALSE (result.wasSaved());
    REQUIRE (result.outcome == SaveOutcome::replaceFailed);

    CHECK (textOf (saved.file) == canonical (saved.second));
    // Nothing was saved, so the backup is still that of the last save that
    // was.
    CHECK (textOf (saved.backup) == canonical (saved.first));
    CHECK (saved.directory.fileNames() == juce::StringArray { "song.xml", "song.xml.bak" });
}

TEST_CASE ("A save over a file whose backup is read-only is refused, and both stay as they were", "[io][ProjectFile][save][backup]")
{
    const SavedTwice saved;

    // The user protects the version they want to keep.
    REQUIRE (saved.backup.setReadOnly (true));
    REQUIRE_FALSE (saved.backup.hasWriteAccess());
    const auto modifiedBefore = saved.file.getLastModificationTime();

    const auto result = io::ProjectFile::save (saved.third, saved.file);

    CHECK_FALSE (result.wasSaved());
    CHECK (result.outcome == SaveOutcome::backupFailed);
    CHECK (result.message.contains (saved.backup.getFullPathName()));
    CHECK (result.message.contains ("read-only"));

    CHECK (textOf (saved.file) == canonical (saved.second));
    CHECK (saved.file.getLastModificationTime() == modifiedBefore);
    CHECK (textOf (saved.backup) == canonical (saved.first));
    CHECK_FALSE (saved.backup.hasWriteAccess());
    CHECK (saved.directory.fileNames() == juce::StringArray { "song.xml", "song.xml.bak" });
}

TEST_CASE ("A read-only backup is in no one's way where there is no file to save over", "[io][ProjectFile][save][backup]")
{
    const BackupTestDirectory directory;
    const auto file = directory.file ("song.xml");
    const auto backup = directory.file ("song.xml.bak");
    REQUIRE (backup.replaceWithText ("kept"));
    REQUIRE (backup.setReadOnly (true));

    const auto project = projectNamed ("First");
    const auto result = io::ProjectFile::save (project, file);

    // No previous file, so no backup is made and the one there is not asked.
    CHECK (result.outcome == SaveOutcome::saved);
    CHECK (textOf (file) == canonical (project));
    CHECK (textOf (backup) == "kept");
}

TEST_CASE ("When the backup cannot be replaced after the save, the project is saved and the save says so", "[io][ProjectFile][save][backup]")
{
    const SavedTwice saved;
    MoveThatFailsOver move { saved.backup };

    const auto result = io::ProjectFile::save (saved.third, saved.file, std::ref (move));

    INFO ("the save reported: " << result.message.toStdString());
    REQUIRE (move.timesFailed == 1);
    CHECK (result.outcome == SaveOutcome::savedWithoutBackup);
    CHECK (result.wasSaved());
    CHECK (result.message.contains (saved.backup.getFullPathName()));

    CHECK (textOf (saved.file) == canonical (saved.third));
    CHECK (textOf (saved.backup) == canonical (saved.first));
    CHECK (saved.directory.fileNames() == juce::StringArray { "song.xml", "song.xml.bak" });
}
