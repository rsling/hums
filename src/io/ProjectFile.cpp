#include "io/ProjectFile.h"

#include "model/DeclaredIdentifiers.h"
#include "model/Ids.h"
#include "model/PropertyReader.h"

#include <algorithm>
#include <limits>
#include <memory>
#include <optional>
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

} // namespace

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
