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
// It holds the canonical writer and the load sequence. The atomic save
// follows.
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
    // Loading.

    struct LoadResult
    {
        enum class Outcome
        {
            loaded,
            fileNotReadable,   // no such file, or it cannot be opened
            malformedXml,
            notAProjectFile,   // XML, but its root is no Project, or it holds text between its nodes
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
};

} // namespace io
