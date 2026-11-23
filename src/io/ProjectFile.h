#pragma once

#include "io/Migrations.h"
#include "model/Validator.h"

#include <juce_data_structures/juce_data_structures.h>

#include <vector>

namespace io
{

// The native project file: the document as XML (docs/SCHEMA.md -> Loading,
// saving, canonical form).
//
// It holds the canonical writer, the load sequence and the atomic save.
class ProjectFile
{
public:
    // The document as canonical XML text, the only form Hums saves. Two
    // documents that read the same through the typed wrappers give the same
    // text, whatever their properties are held as (a number set in this
    // session, or its text from a loaded file) and in whatever order they
    // were set.
    //
    //   - one node per line, children indented by two spaces, in stored order;
    //     "\n" line endings; the text is ASCII, every other character is
    //     written as a numeric character reference
    //   - a node's properties in the order of model/Ids.h; a property Ids.h
    //     does not declare (data from a newer build) comes after them, in
    //     stored order
    //   - a property that holds the schema's default is left out
    //   - integers as plain decimal, doubles as juce::String (double) writes
    //     them (six significant digits [D14]), booleans as 0 / 1
    //   - unknown properties and unknown children are written as they are
    //     stored
    //
    // The node must be a Project node, and the document a valid one: the
    // writer does not check it. A value that is not in the text form of its
    // type is written as stored, never repaired; refusing it is the
    // Validator's work.
    static juce::String toCanonicalXml (const juce::ValueTree& project);

    //==========================================================================
    // File names.

    // Whether a project file may have this file's name (docs/SCHEMA.md ->
    // Loading, saving, canonical form). A name that begins with "~" is
    // reserved for files Hums makes and handles itself, and is neither loaded
    // nor saved to: the file is refused before it is looked at. Only the
    // name counts, not the directories on the way to it.
    //
    // The reservation is also what keeps such a name away from juce::File,
    // which takes a relative path that begins with "~" for a home directory
    // (getChildFile, getSiblingFile).
    static bool isNameAllowed (const juce::File& file);

    //==========================================================================
    // Loading.

    struct LoadResult
    {
        enum class Outcome
        {
            loaded,
            nameNotAllowed,    // the file's name begins with "~" (isNameAllowed)
            fileNotReadable,   // no such file, or it cannot be opened
            malformedXml,      // also where JUCE's parser carries on: an unknown entity, an attribute twice on one node,
                               // a closing tag of another name or cut short, something after the root (XmlWellFormedness);
                               // and a file whose bytes are no UTF-8 text
            notAProjectFile,   // XML, but its root is no Project, it has a DOCTYPE, it holds text between its nodes, or a
                               // node has an attribute and its "base64:" form, which JUCE reads as one property twice
            noSchemaVersion,   // schemaVersion is absent, or is no version number
            versionTooNew,     // written by a newer build
            migrationFailed,   // a migration step threw
            invalidDocument    // the Validator found violations
        };

        Outcome outcome = Outcome::loaded;

        // What went wrong, in words for the user; it names the first
        // offending node and property. Empty when the file was loaded.
        juce::String message;

        // The offending nodes and properties: everything the Validator found
        // for invalidDocument, the one schemaVersion for noSchemaVersion and
        // versionTooNew, nothing otherwise.
        std::vector<model::Validator::Violation> violations;

        bool wasLoaded() const { return outcome == Outcome::loaded; }
    };

    // Loads a project file. One sequence (docs/SCHEMA.md -> Loading): parse
    // the XML into a detached candidate tree, check its schemaVersion, run
    // the migrations on the candidate, run the Validator, and only then
    // install the candidate as the document and clear the undo history.
    //
    // A file whose name is not allowed (isNameAllowed) is refused before it
    // is opened.
    //
    // Any failure leaves `document`, `undoManager` and the file as they were.
    // Nothing is repaired, clamped or guessed [D17], and the file is never
    // written to.
    //
    // Installing makes `document` refer to the loaded tree (an assignment;
    // the previous tree is not changed, only let go). Listeners registered on
    // `document` itself get one valueTreeRedirected and no other callback, so
    // nobody sees a half-installed document. Every other handle to the
    // previous tree or to a node in it, a typed wrapper included, still
    // refers to the previous tree afterwards and has to be fetched anew.
    //
    // `steps` is a parameter so that a test can hand in steps of its own;
    // everything else loads with the steps of this build. The version this
    // call reads up to is the one `steps` reaches.
    static LoadResult load (const juce::File& file,
                            juce::ValueTree& document,
                            juce::UndoManager& undoManager,
                            const Migrations::Steps& steps = Migrations::steps());

    // The same sequence from the file's text.
    static LoadResult loadFromText (const juce::String& xmlText,
                                    juce::ValueTree& document,
                                    juce::UndoManager& undoManager,
                                    const Migrations::Steps& steps = Migrations::steps());

    //==========================================================================
    // Saving.

    struct SaveResult
    {
        enum class Outcome
        {
            saved,
            nameNotAllowed,    // the target's name begins with "~" (isNameAllowed)
            targetNotWritable, // the target is a directory, a file that is read-only, or a symbolic link that leads to no file
            writeFailed,       // the new text could not be written beside the target, or not be given the previous file's permissions
            backupFailed,      // the previous file could not be kept as <name>.bak
            replaceFailed      // the new file could not be moved over the target
        };

        Outcome outcome = Outcome::saved;

        // What went wrong, in words for the user. Empty when the file was
        // saved.
        juce::String message;

        bool wasSaved() const { return outcome == Outcome::saved; }
    };

    // Saves the document as canonical XML (docs/SCHEMA.md -> Loading, saving,
    // canonical form). The sequence:
    //
    //   1. the text is written to a temporary file in the target's directory,
    //      forced to disk and read back
    //   2. if the target exists, the temporary file is given its permission
    //      bits
    //   3. if the target exists, a copy of it becomes <file name>.bak beside
    //      it ("song.xml.bak" for "song.xml"), in place of an older one
    //   4. the temporary file is renamed over the target
    //
    // The target is never opened for writing and never removed: until the
    // rename of step 4 it is the previous file, byte for byte, and after it
    // the new one. A failure at any step leaves it as it was and removes the
    // temporary file. A target that is read-only is refused, although a
    // rename could replace it. A directory that is not there is not created.
    // A target whose name is not allowed (isNameAllowed) is refused before
    // anything is written, whether or not such a file is there.
    //
    // When `file` is a symbolic link, the target is the file the link leads
    // to, through however many links: that file is replaced, the temporary
    // file and the backup stand beside it, and the link stays. Its name must
    // be allowed too. A link that leads to no file, or round in a circle, is
    // refused. Nothing but the permission bits is carried over from the
    // previous file: its extended attributes (Finder tags and comments) and
    // its owner are not sure to be kept.
    //
    // `project` must be a Project node and a valid document; like the writer,
    // save does not check it.
    static SaveResult save (const juce::ValueTree& project, const juce::File& file);
};

} // namespace io
