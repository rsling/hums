#include "io/ProjectFile.h"

#include "model/DeclaredIdentifiers.h"
#include "model/Ids.h"
#include "model/PropertyReader.h"

#include <algorithm>
#include <cstddef>
#include <exception>
#include <limits>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

namespace io
{

namespace
{

    namespace ids = model::ids;

    //==========================================================================
    // What the writer knows about the properties of docs/SCHEMA.md -> Node
    // reference: the type each is written as, and its default if it has one.

    enum class ValueKind
    {
        integer, // int and int64 alike
        boolean,
        decimal, // a double
        text
    };

    struct PropertyRule
    {
        juce::Identifier nodeType;
        juce::Identifier property;
        ValueKind kind;

        // The schema's default in its canonical text, or nothing for a
        // property that is always written when it is there.
        std::optional<juce::String> defaultText;
    };

    // One line per row of the tables in docs/SCHEMA.md -> Node reference, in
    // their order. The defaults are the ones the typed wrappers return for an
    // absent property; ProjectFileTest holds the two together.
    const std::vector<PropertyRule>& propertyRules()
    {
        static const std::vector<PropertyRule> rules {
            { ids::Project, ids::schemaVersion, ValueKind::integer, std::nullopt },
            { ids::Project, ids::name, ValueKind::text, "" },
            { ids::Project, ids::ppq, ValueKind::integer, "960" },

            { ids::Performer, ids::id, ValueKind::text, std::nullopt },
            { ids::Performer, ids::name, ValueKind::text, std::nullopt },
            { ids::Performer, ids::deviceIdentifier, ValueKind::text, "" },
            { ids::Performer, ids::deviceName, ValueKind::text, "" },
            { ids::Performer, ids::channelFilter, ValueKind::integer, "0" },

            { ids::Player, ids::id, ValueKind::text, std::nullopt },
            { ids::Player, ids::name, ValueKind::text, std::nullopt },
            { ids::Player, ids::deviceIdentifier, ValueKind::text, "" },
            { ids::Player, ids::deviceName, ValueKind::text, "" },
            { ids::Player, ids::sendClock, ValueKind::boolean, "0" },

            { ids::Pad, ids::index, ValueKind::integer, std::nullopt },
            { ids::Pad, ids::inputPitch, ValueKind::integer, std::nullopt },
            { ids::Pad, ids::pitch, ValueKind::integer, std::nullopt },
            { ids::Pad, ids::label, ValueKind::text, "" },

            { ids::TempoEvent, ids::tick, ValueKind::integer, std::nullopt },
            { ids::TempoEvent, ids::bpm, ValueKind::decimal, std::nullopt },

            { ids::TimeSigEvent, ids::tick, ValueKind::integer, std::nullopt },
            { ids::TimeSigEvent, ids::numerator, ValueKind::integer, std::nullopt },
            { ids::TimeSigEvent, ids::denominator, ValueKind::integer, std::nullopt },

            { ids::Marker, ids::tick, ValueKind::integer, std::nullopt },
            { ids::Marker, ids::name, ValueKind::text, "" },

            { ids::Clip, ids::id, ValueKind::text, std::nullopt },
            { ids::Clip, ids::name, ValueKind::text, "" },
            { ids::Clip, ids::kind, ValueKind::text, std::nullopt },
            { ids::Clip, ids::loopLength, ValueKind::integer, std::nullopt },
            { ids::Clip, ids::quantise, ValueKind::text, "" },
            { ids::Clip, ids::colour, ValueKind::text, "" },

            { ids::Note, ids::tick, ValueKind::integer, std::nullopt },
            { ids::Note, ids::length, ValueKind::integer, std::nullopt },
            { ids::Note, ids::pitch, ValueKind::integer, std::nullopt },
            { ids::Note, ids::velocity, ValueKind::integer, "100" },
            { ids::Note, ids::offVelocity, ValueKind::integer, "64" },
            { ids::Note, ids::enabled, ValueKind::boolean, "1" },

            { ids::Event, ids::tick, ValueKind::integer, std::nullopt },
            { ids::Event, ids::kind, ValueKind::text, std::nullopt },
            { ids::Event, ids::a, ValueKind::integer, "0" },
            { ids::Event, ids::b, ValueKind::integer, "0" },
            { ids::Event, ids::data, ValueKind::text, "" },
            { ids::Event, ids::enabled, ValueKind::boolean, "1" },

            // The defaults of resetAt and skipMask depend on the Grid's
            // stepCount: see defaultTextOf.
            { ids::Grid, ids::stepLength, ValueKind::integer, "240" },
            { ids::Grid, ids::stepCount, ValueKind::integer, "16" },
            { ids::Grid, ids::resetAt, ValueKind::integer, std::nullopt },
            { ids::Grid, ids::skipMask, ValueKind::text, std::nullopt },

            { ids::Lane, ids::id, ValueKind::text, std::nullopt },
            { ids::Lane, ids::name, ValueKind::text, "" },
            { ids::Lane, ids::fixedPitch, ValueKind::integer, std::nullopt },

            { ids::Cell, ids::pitch, ValueKind::integer, "60" },
            { ids::Cell, ids::on, ValueKind::boolean, "0" },
            { ids::Cell, ids::velocity, ValueKind::integer, "100" },
            { ids::Cell, ids::gate, ValueKind::decimal, "0.5" },

            { ids::CcValue, ids::cc, ValueKind::integer, std::nullopt },
            { ids::CcValue, ids::value, ValueKind::integer, std::nullopt },

            { ids::Track, ids::id, ValueKind::text, std::nullopt },
            { ids::Track, ids::name, ValueKind::text, "" },
            { ids::Track, ids::colour, ValueKind::text, "" },
            { ids::Track, ids::playerId, ValueKind::text, "" },
            { ids::Track, ids::channel, ValueKind::integer, "1" },
            { ids::Track, ids::performerId, ValueKind::text, "" },
            { ids::Track, ids::mute, ValueKind::boolean, "0" },
            { ids::Track, ids::solo, ValueKind::boolean, "0" },

            { ids::Placement, ids::id, ValueKind::text, std::nullopt },
            { ids::Placement, ids::clipId, ValueKind::text, std::nullopt },
            { ids::Placement, ids::start, ValueKind::integer, std::nullopt },
            { ids::Placement, ids::length, ValueKind::integer, std::nullopt },
            { ids::Placement, ids::offset, ValueKind::integer, "0" },

            { ids::AutomationLane, ids::id, ValueKind::text, std::nullopt },
            { ids::AutomationLane, ids::name, ValueKind::text, "" },

            { ids::Knob, ids::cc, ValueKind::integer, std::nullopt },
            { ids::Knob, ids::label, ValueKind::text, "" },

            { ids::Slot, ids::scene, ValueKind::integer, std::nullopt },
            { ids::Slot, ids::clipId, ValueKind::text, std::nullopt },

            { ids::Session, ids::launchQuantise, ValueKind::integer, "3840" },

            { ids::Scene, ids::index, ValueKind::integer, std::nullopt },
            { ids::Scene, ids::name, ValueKind::text, "" },

            { ids::UiState, ids::activeTab, ValueKind::text, "arrangement" },
            { ids::UiState, ids::sharedEditPolicy, ValueKind::text, "updateAll" },
            { ids::UiState, ids::loopStart, ValueKind::integer, "0" },
            { ids::UiState, ids::loopEnd, ValueKind::integer, "0" },
            { ids::UiState, ids::loopEnabled, ValueKind::boolean, "0" },
            { ids::UiState, ids::exportLength, ValueKind::integer, "0" },
        };

        return rules;
    }

    // Nothing for a property the schema does not list for that node type:
    // one from a newer build, or anything on a node of an unknown type or a
    // view-state node.
    const PropertyRule* findRule (const juce::Identifier& nodeType, const juce::Identifier& property)
    {
        for (const auto& rule : propertyRules())
            if (rule.nodeType == nodeType && rule.property == property)
                return &rule;

        return nullptr;
    }

    //==========================================================================
    // Values.

    juce::String storedText (const juce::ValueTree& node, const juce::Identifier& property)
    {
        return node[property].toString();
    }

    // The property's value in the one text form the writer emits for its
    // type. A value that is not in the text form of its type is given back
    // as stored: the writer repairs nothing.
    juce::String canonicalText (const juce::ValueTree& node, const juce::Identifier& property, ValueKind kind)
    {
        if (kind == ValueKind::integer)
        {
            // "007" and "-0" are integers too; they are written as 7 and 0.
            const auto stored = model::PropertyReader::readInt64 (node, property);

            if (stored.isValid())
                return juce::String (stored.value);
        }

        if (kind == ValueKind::decimal)
        {
            const auto stored = model::PropertyReader::readDouble (node, property);

            if (stored.isValid())
                return juce::String (stored.value);
        }

        // A boolean in its text form is exactly "0" or "1" already, whether
        // it is held as a bool, a number or text.
        return storedText (node, property);
    }

    // The default the property has on this node, in canonical text, or
    // nothing if it has none.
    std::optional<juce::String> defaultTextOf (const juce::ValueTree& node, const PropertyRule& rule)
    {
        const auto isOnGrid = node.hasType (ids::Grid);

        if (isOnGrid && (rule.property == ids::resetAt || rule.property == ids::skipMask))
        {
            const auto stepCount = model::PropertyReader::readInt (node, ids::stepCount);

            if (stepCount.isMalformed())
                return std::nullopt;

            const auto columns = stepCount.isValid() ? stepCount.value : 16;

            // resetAt defaults to the step count, skipMask to "no column
            // skipped".
            if (rule.property == ids::resetAt)
                return juce::String (columns);

            return juce::String::repeatedString ("0", columns);
        }

        return rule.defaultText;
    }

    //==========================================================================
    // Nodes.

    // The node's property names: those Ids.h declares in its order, then the
    // others in stored order.
    std::vector<juce::Identifier> propertiesInCanonicalOrder (const juce::ValueTree& node)
    {
        struct Entry
        {
            int rank;
            juce::Identifier property;
        };

        std::vector<Entry> entries;

        for (int i = 0; i < node.getNumProperties(); ++i)
        {
            const auto property = node.getPropertyName (i);
            const auto position = model::DeclaredIdentifiers::positionOf (property);

            entries.push_back ({ position >= 0 ? position : std::numeric_limits<int>::max(), property });
        }

        // Stable, so that the undeclared ones keep their stored order.
        std::stable_sort (entries.begin(), entries.end(), [] (const Entry& first, const Entry& second) {
            return first.rank < second.rank;
        });

        std::vector<juce::Identifier> properties;

        for (const auto& entry : entries)
            properties.push_back (entry.property);

        return properties;
    }

    std::unique_ptr<juce::XmlElement> toXml (const juce::ValueTree& node)
    {
        auto element = std::make_unique<juce::XmlElement> (node.getType().toString());

        for (const auto& property : propertiesInCanonicalOrder (node))
        {
            const auto* rule = findRule (node.getType(), property);

            if (rule == nullptr)
            {
                element->setAttribute (property, storedText (node, property));
                continue;
            }

            const auto text = canonicalText (node, property, rule->kind);

            if (defaultTextOf (node, *rule) == text)
                continue;

            element->setAttribute (property, text);
        }

        for (const auto& child : node)
            element->addChildElement (toXml (child).release());

        return element;
    }

    //==========================================================================
    // Loading.

    using LoadResult = ProjectFile::LoadResult;
    using Outcome = ProjectFile::LoadResult::Outcome;

    juce::String describe (const model::Validator::Violation& violation)
    {
        auto where = violation.nodePath;

        if (violation.property.isNotEmpty())
            where << ", " << violation.property;

        return where + ": " + violation.reason;
    }

    LoadResult refusal (Outcome outcome, const juce::String& message)
    {
        LoadResult result;
        result.outcome = outcome;
        result.message = message;

        return result;
    }

    // A refusal that is about the Project's schemaVersion.
    LoadResult versionRefusal (Outcome outcome, const juce::String& reason)
    {
        const model::Validator::Violation violation { ids::Project.toString(), ids::schemaVersion.toString(), reason };

        auto result = refusal (outcome, "The project file was not loaded. " + describe (violation));
        result.violations.push_back (violation);

        return result;
    }

    // A ValueTree has nothing to hold XML text in, so a file with text
    // between its nodes cannot be loaded as it is, and dropping the text
    // would be a repair. Gives the first element that holds text directly,
    // or nullptr. (Whitespace between nodes is no text: JUCE's parser leaves
    // it out.)
    const juce::XmlElement* firstElementHoldingText (const juce::XmlElement& element)
    {
        for (const auto* child : element.getChildIterator())
        {
            if (child->isTextElement())
                return &element;

            if (const auto* found = firstElementHoldingText (*child))
                return found;
        }

        return nullptr;
    }

    // The name of the property a ValueTree makes of an attribute. JUCE reads
    // an attribute "base64:x" whose value is binary data in its own encoding
    // as the property x (juce_NamedValueSet.cpp, setFromXmlAttributes).
    juce::String propertyNameOf (const juce::String& attributeName, const juce::String& attributeValue)
    {
        const juce::String binaryPrefix ("base64:");

        if (attributeName.startsWith (binaryPrefix))
        {
            juce::MemoryBlock binaryData;

            if (binaryData.fromBase64Encoding (attributeValue))
                return attributeName.substring (binaryPrefix.length());
        }

        return attributeName;
    }

    // XML allows an attribute once per element. JUCE's parser keeps a second
    // one, and the ValueTree made of the element then has two properties of
    // one name: every reader sees the first, and the second comes to light
    // when a command removes the first. An attribute beside its "base64:"
    // form ends the same way, although that is well-formed XML. Gives the
    // refusal for the first element with either, or nothing.
    std::optional<LoadResult> refusalOfAnAttributeGivenTwice (const juce::XmlElement& element)
    {
        for (auto later = 1; later < element.getNumAttributes(); ++later)
        {
            const auto& laterName = element.getAttributeName (later);

            for (auto earlier = 0; earlier < later; ++earlier)
            {
                const auto& earlierName = element.getAttributeName (earlier);

                if (earlierName == laterName)
                    return refusal (Outcome::malformedXml,
                                    "The project file is not well-formed XML: a " + element.getTagName() + " node has the attribute " + laterName
                                        + " twice.");

                if (propertyNameOf (earlierName, element.getAttributeValue (earlier)) == propertyNameOf (laterName, element.getAttributeValue (later)))
                    return refusal (Outcome::notAProjectFile,
                                    "The file is not a Hums project: a " + element.getTagName() + " node has the attributes " + earlierName + " and "
                                        + laterName + ", which are one and the same property.");
            }
        }

        for (const auto* child : element.getChildIterator())
            if (auto found = refusalOfAnAttributeGivenTwice (*child))
                return found;

        return std::nullopt;
    }

    // Runs the steps that lie between the candidate's version and the one
    // the table reaches. Gives the reason if a step threw, and nothing if
    // all went through.
    std::optional<juce::String> migrate (juce::ValueTree& candidate, int fileVersion, const Migrations::Steps& steps)
    {
        const auto targetVersion = Migrations::versionReachedBy (steps);

        for (auto version = fileVersion; version < targetVersion; ++version)
        {
            const auto stepName = "The migration from schemaVersion " + juce::String (version) + " to " + juce::String (version + 1);

            try
            {
                // The first step takes version 1 to 2.
                steps[static_cast<std::size_t> (version - 1)](candidate);
            }
            catch (const std::exception& exception)
            {
                return stepName + " failed: " + juce::String::fromUTF8 (exception.what());
            }
            catch (...)
            {
                return stepName + " failed.";
            }

            candidate.setProperty (ids::schemaVersion, version + 1, nullptr);
        }

        return std::nullopt;
    }

    //==========================================================================
    // File names.

    juce::String whyTheNameIsNotAllowed (const juce::File& file)
    {
        return "The name " + file.getFileName() + " begins with \"~\", and such names are reserved for files Hums handles itself.";
    }

    //==========================================================================
    // Saving.

    using SaveResult = ProjectFile::SaveResult;

    SaveResult saveFailure (SaveResult::Outcome outcome, const juce::String& message)
    {
        SaveResult result;
        result.outcome = outcome;
        result.message = message;

        return result;
    }

    // Writes the text to the file, which must not exist yet, and forces it to
    // disk. Gives the reason if that failed.
    std::optional<juce::String> writeNewFile (const juce::File& file, const juce::String& text)
    {
        {
            juce::FileOutputStream stream (file);

            if (! stream.openedOk())
                return stream.getStatus().getErrorMessage();

            const auto written = stream.write (text.toRawUTF8(), text.getNumBytesAsUTF8());
            // The destructor alone would leave the data in the system's cache.
            stream.flush();

            if (! written || stream.getStatus().failed())
                return stream.getStatus().failed() ? stream.getStatus().getErrorMessage() : juce::String ("not all of it could be written");
        }

        // A write that came up short leaves no error in the stream's status,
        // so the file is asked what it holds.
        if (file.loadFileAsString() != text)
            return juce::String ("the file does not hold what was written to it");

        return std::nullopt;
    }

    // Makes a copy of the file the backup, in place of an older backup. The
    // copy is made under a temporary name and renamed, so that the older
    // backup is there until the new one is complete.
    bool keepAsBackup (const juce::File& file, const juce::File& backup)
    {
        // rename() puts a file over a file, not over a directory.
        if (backup.isDirectory())
            return false;

        const juce::TemporaryFile temporary (backup, juce::TemporaryFile::useHiddenFile);

        return file.copyFileTo (temporary.getFile()) && temporary.getFile().replaceFileIn (backup);
    }

} // namespace

bool ProjectFile::isNameAllowed (const juce::File& file)
{
    return ! file.getFileName().startsWithChar ('~');
}

ProjectFile::LoadResult ProjectFile::load (const juce::File& file,
                                           juce::ValueTree& document,
                                           juce::UndoManager& undoManager,
                                           const Migrations::Steps& steps)
{
    if (! isNameAllowed (file))
        return refusal (Outcome::nameNotAllowed, "The project file was not loaded. " + whyTheNameIsNotAllowed (file));

    if (! file.existsAsFile())
        return refusal (Outcome::fileNotReadable, "There is no file " + file.getFullPathName() + ".");

    juce::FileInputStream stream (file);

    if (! stream.openedOk())
        return refusal (Outcome::fileNotReadable,
                        "The file " + file.getFullPathName() + " cannot be read: " + stream.getStatus().getErrorMessage());

    return loadFromText (stream.readEntireStreamAsString(), document, undoManager, steps);
}

ProjectFile::LoadResult ProjectFile::loadFromText (const juce::String& xmlText,
                                                   juce::ValueTree& document,
                                                   juce::UndoManager& undoManager,
                                                   const Migrations::Steps& steps)
{
    // 1. Parse, into a tree nothing else refers to.
    juce::XmlDocument xmlDocument (xmlText);
    const auto root = xmlDocument.getDocumentElement();

    // JUCE's parser carries on after some errors (an unknown entity, an
    // illegal escape sequence) and still gives an element; the error text is
    // then the only sign. Such a file is refused like any other malformed one.
    if (root == nullptr || xmlDocument.getLastParseError().isNotEmpty())
        return refusal (Outcome::malformedXml, "The project file is not well-formed XML: " + xmlDocument.getLastParseError() + ".");

    if (const auto attributeGivenTwice = refusalOfAnAttributeGivenTwice (*root))
        return *attributeGivenTwice;

    if (const auto* holder = firstElementHoldingText (*root))
        return refusal (Outcome::notAProjectFile,
                        "The file is not a Hums project: there is text inside a " + holder->getTagName() + " node, where only nodes can be.");

    auto candidate = juce::ValueTree::fromXml (*root);

    if (! candidate.hasType (ids::Project))
        return refusal (Outcome::notAProjectFile,
                        "The file is not a Hums project: its root node is a " + candidate.getType().toString() + ", not a Project.");

    // 2. The version. It is read before anything else is looked at, because
    // it decides which steps run and whether this build may read the file.
    const auto fileVersion = model::PropertyReader::readInt64 (candidate, ids::schemaVersion);
    const auto supportedVersion = Migrations::versionReachedBy (steps);

    if (fileVersion.isAbsent())
        return versionRefusal (Outcome::noSchemaVersion, "the property is missing");

    if (! fileVersion.isValid() || fileVersion.value < 1)
        return versionRefusal (Outcome::noSchemaVersion, "\"" + storedText (candidate, ids::schemaVersion) + "\" is not a version number");

    if (fileVersion.value > supportedVersion)
        return versionRefusal (Outcome::versionTooNew,
                               "the file has version " + juce::String (fileVersion.value) + " and was written by a newer Hums; this one reads up to version "
                                   + juce::String (supportedVersion));

    // 3. Migrations, on the candidate.
    if (const auto failure = migrate (candidate, static_cast<int> (fileVersion.value), steps))
        return refusal (Outcome::migrationFailed, "The project file was not loaded. " + *failure);

    // 4. The Validator.
    auto violations = model::Validator::validate (candidate);

    if (! violations.empty())
    {
        auto message = "The project file is not valid. " + describe (violations.front());

        if (violations.size() > 1)
            message << " (and " << static_cast<int> (violations.size() - 1) << " more)";

        auto result = refusal (Outcome::invalidDocument, message);
        result.violations = std::move (violations);

        return result;
    }

    // 5. Install. Nothing above has touched the document or the history.
    document = candidate;
    undoManager.clearUndoHistory();

    return {};
}

ProjectFile::SaveResult ProjectFile::save (const juce::ValueTree& project, const juce::File& file)
{
    const auto path = file.getFullPathName();

    if (! isNameAllowed (file))
        return saveFailure (SaveResult::Outcome::nameNotAllowed, "The project was not saved. " + whyTheNameIsNotAllowed (file));

    if (file.isDirectory())
        return saveFailure (SaveResult::Outcome::targetNotWritable, "The project was not saved: " + path + " is a directory.");

    // rename() asks only the directory for permission, and would replace a
    // file its owner has marked read-only.
    if (file.existsAsFile() && ! file.hasWriteAccess())
        return saveFailure (SaveResult::Outcome::targetNotWritable, "The project was not saved: the file " + path + " is read-only.");

    // 1. The new text, beside the target. The temporary file is removed when
    // this function returns, unless it has become the target by then.
    const juce::TemporaryFile temporary (file, juce::TemporaryFile::useHiddenFile);

    if (const auto failure = writeNewFile (temporary.getFile(), toCanonicalXml (project)))
        return saveFailure (SaveResult::Outcome::writeFailed,
                            "The project was not saved: nothing could be written to the folder " + file.getParentDirectory().getFullPathName() + " ("
                                + *failure + ").");

    // 2. The previous file, kept. getSiblingFile would take a name that
    // begins with "~" for a path of its own; no such name comes this far.
    const auto backup = file.getSiblingFile (file.getFileName() + ".bak");

    if (file.existsAsFile() && ! keepAsBackup (file, backup))
        return saveFailure (SaveResult::Outcome::backupFailed,
                            "The project was not saved: the previous file could not be kept as " + backup.getFullPathName() + ".");

    // 3. The rename, which is the save.
    if (! temporary.getFile().replaceFileIn (file))
        return saveFailure (SaveResult::Outcome::replaceFailed, "The project was not saved: the file " + path + " could not be replaced.");

    return {};
}

juce::String ProjectFile::toCanonicalXml (const juce::ValueTree& project)
{
    jassert (project.hasType (ids::Project));

    juce::XmlElement::TextFormat format;
    format.newLineChars = "\n";
    // JUCE would otherwise break a node's attributes over several lines.
    format.lineWrapLength = std::numeric_limits<int>::max();

    return toXml (project)->toString (format);
}

} // namespace io
