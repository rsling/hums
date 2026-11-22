#include "DocumentGenerator.h"

#include "RandomSource.h"
#include "io/Migrations.h"
#include "model/Ids.h"

#include <algorithm>
#include <set>
#include <vector>

namespace guardrails
{

namespace
{

    namespace ids = model::ids;

    // What the generator remembers of a clip it has made, for the placements
    // and slots that reference it.
    struct ClipRecord
    {
        juce::String id;

        // One pass of the loop in ticks; a placement's offset stays below it
        // (invariant 4).
        std::int64_t loopLength = 0;

        // An event clip with nothing but cc events: the only kind an
        // AutomationLane takes (invariant 9).
        bool holdsOnlyCcEvents = false;
    };

    // One document in the making. The methods follow the tree of
    // docs/SCHEMA.md from top to bottom.
    class Builder
    {
    public:
        explicit Builder (std::uint64_t seed)
            : random (seed)
        {
        }

        juce::ValueTree project()
        {
            juce::ValueTree node (ids::Project);

            setInt (node, ids::schemaVersion, io::Migrations::currentVersion());
            setTextOrOmit (node, ids::name, text(), "");
            setIntOrOmit (node, ids::ppq, 960, 960);

            // The clips come before the tracks, and the routing before both:
            // a reference needs something to name.
            std::vector<juce::ValueTree> containers { routing(), timeline(), clipPool() };
            containers.push_back (tracks());
            containers.push_back (session());
            containers.push_back (uiState());

            appendInSomeOrder (node, containers, 20);

            return node;
        }

    private:
        //======================================================================
        // Values.

        // A value is held as a command of this session would set it, or as
        // its text, the way a loaded file holds it.
        void set (juce::ValueTree& node, const juce::Identifier& property, const juce::var& value)
        {
            node.setProperty (property, random.chance (25) ? juce::var (value.toString()) : value, nullptr);
        }

        void setInt (juce::ValueTree& node, const juce::Identifier& property, int value)
        {
            set (node, property, value);
        }

        void setTick (juce::ValueTree& node, const juce::Identifier& property, std::int64_t value)
        {
            set (node, property, static_cast<juce::int64> (value));
        }

        void setText (juce::ValueTree& node, const juce::Identifier& property, const juce::String& value)
        {
            node.setProperty (property, value, nullptr);
        }

        // For a property with a default: a value that is the default is as
        // often left out as written.
        bool leavesOut (bool isDefault)
        {
            return isDefault && random.chance (50);
        }

        void setIntOrOmit (juce::ValueTree& node, const juce::Identifier& property, int value, int valueWhenAbsent)
        {
            if (! leavesOut (value == valueWhenAbsent))
                setInt (node, property, value);
        }

        void setTickOrOmit (juce::ValueTree& node, const juce::Identifier& property, std::int64_t value, std::int64_t valueWhenAbsent)
        {
            if (! leavesOut (value == valueWhenAbsent))
                setTick (node, property, value);
        }

        void setBoolOrOmit (juce::ValueTree& node, const juce::Identifier& property, bool value, bool valueWhenAbsent)
        {
            if (! leavesOut (value == valueWhenAbsent))
                set (node, property, value);
        }

        void setTextOrOmit (juce::ValueTree& node, const juce::Identifier& property, const juce::String& value, const juce::String& valueWhenAbsent)
        {
            if (! leavesOut (value == valueWhenAbsent))
                setText (node, property, value);
        }

        // A double holds six significant digits [D14], and it is held as the
        // text that is saved, never as a number. A juce::var that holds a
        // double gives a text of fifteen digits, and model/PropertyReader
        // reads every value through its text: for a few values (0.000099,
        // 0.000765) that text is another number than the one stored, and the
        // Validator then counts more than six digits. How a command is to
        // store a double is open until the first one that does
        // (docs/MILESTONES.md -> Carried-over issues).
        void setDouble (juce::ValueTree& node, const juce::Identifier& property, double value)
        {
            setText (node, property, juce::String (value));
        }

        // The children in the order given, or, in `percentShuffled` of 100
        // cases, in another.
        void appendInSomeOrder (juce::ValueTree& parent, std::vector<juce::ValueTree> children, int percentShuffled)
        {
            if (random.chance (percentShuffled))
                random.shuffle (children);

            for (const auto& child : children)
                parent.appendChild (child, nullptr);
        }

        // The text juce::Uuid::toString() writes, from the seed.
        juce::String uuid()
        {
            static const char* const digits = "0123456789abcdef";
            juce::String id;

            for (int digit = 0; digit < 32; ++digit)
                id << digits[random.indexBelow (16)];

            return id;
        }

        // A name or a label: often empty, and with the characters that XML
        // has to write in another form.
        juce::String text()
        {
            static const std::vector<juce::String> pieces {
                "Bass",
                "Lead",
                "Kick",
                "5-step",
                "MOTU midi express 1",
                " ",
                "  ",
                "&",
                "<",
                ">",
                "\"",
                "'",
                "/>",
                "&amp;",
                "&#10;",
                "\t",
                "\n",
                "\r\n",
                juce::String::fromUTF8 ("\xc3\xa4"),             // a with two dots
                juce::String::fromUTF8 ("\xe2\x99\xaa"),         // a note sign
                juce::String::fromUTF8 ("\xf0\x9f\x8e\xb9"),     // a keyboard, outside the 16 bits of UTF-16
            };

            if (random.chance (30))
                return {};

            juce::String result;

            for (auto count = random.intBetween (1, 4); count > 0; --count)
                result << random.oneOf (pieces);

            return result;
        }

        juce::String hexDigits (int count)
        {
            static const char* const digits = "0123456789abcdefABCDEF";
            juce::String result;

            for (int digit = 0; digit < count; ++digit)
                result << digits[random.indexBelow (22)];

            return result;
        }

        // "#AARRGGBB", or empty for no colour.
        juce::String colour()
        {
            return random.chance (50) ? juce::String() : "#" + hexDigits (8);
        }

        // `count` different numbers from lowest to highest, in no order.
        std::vector<int> distinctInts (int count, int lowest, int highest)
        {
            std::vector<int> numbers;

            for (int number = lowest; number <= highest; ++number)
                numbers.push_back (number);

            random.shuffle (numbers);
            numbers.resize (static_cast<std::size_t> (std::min (count, highest - lowest + 1)));

            return numbers;
        }

        // A position on a timeline: mostly on a sixteenth, sometimes
        // anywhere, now and then far beyond what an int holds.
        std::int64_t tick()
        {
            if (random.chance (60))
                return 240 * random.between (0, 128);

            if (random.chance (95))
                return random.between (0, 40000);

            return random.between (0, std::int64_t { 1 } << 40);
        }

        // A length on a timeline, at least one tick.
        std::int64_t length()
        {
            return tick() + 1;
        }

        // `count` different ticks, tick 0 among them.
        std::set<std::int64_t> distinctTicksFromZero (int count)
        {
            std::set<std::int64_t> ticks { 0 };

            for (int further = 1; further < count; ++further)
                ticks.insert (tick());

            return ticks;
        }

        //======================================================================
        // Routing.

        juce::ValueTree routing()
        {
            juce::ValueTree node (ids::Routing);
            std::vector<juce::ValueTree> children;

            for (auto count = random.intBetween (0, 3); count > 0; --count)
                children.push_back (performer());

            for (auto count = random.intBetween (0, 3); count > 0; --count)
                children.push_back (player());

            children.push_back (padMap());
            appendInSomeOrder (node, children, 30);

            return node;
        }

        juce::ValueTree performer()
        {
            juce::ValueTree node (ids::Performer);
            const auto id = uuid();

            setText (node, ids::id, id);
            setText (node, ids::name, text());
            setTextOrOmit (node, ids::deviceIdentifier, text(), "");
            setTextOrOmit (node, ids::deviceName, text(), "");
            setIntOrOmit (node, ids::channelFilter, random.intBetween (0, 16), 0);

            performerIds.push_back (id);

            return node;
        }

        juce::ValueTree player()
        {
            juce::ValueTree node (ids::Player);
            const auto id = uuid();

            setText (node, ids::id, id);
            setText (node, ids::name, text());
            setTextOrOmit (node, ids::deviceIdentifier, text(), "");
            setTextOrOmit (node, ids::deviceName, text(), "");
            setBoolOrOmit (node, ids::sendClock, random.chance (50), false);

            playerIds.push_back (id);

            return node;
        }

        juce::ValueTree padMap()
        {
            juce::ValueTree node (ids::PadMap);

            // The pad numbers and the notes the pads send are each unique in
            // the map (invariant 11); the notes they produce need not be.
            const auto count = random.intBetween (0, 8);
            const auto indexes = distinctInts (count, 0, 31);
            const auto inputPitches = distinctInts (count, 0, 127);

            for (std::size_t pad = 0; pad < indexes.size(); ++pad)
            {
                juce::ValueTree padNode (ids::Pad);

                setInt (padNode, ids::index, indexes[pad]);
                setInt (padNode, ids::inputPitch, inputPitches[pad]);
                setInt (padNode, ids::pitch, random.intBetween (0, 127));
                setTextOrOmit (padNode, ids::label, text(), "");

                node.appendChild (padNode, nullptr);
            }

            return node;
        }

        //======================================================================
        // Timeline.

        double bpm()
        {
            if (random.chance (40))
                return random.oneOf (std::vector<double> { 60.0, 118.0, 120.0, 174.5 });

            // Three decimals up to 999.999; a tempo of 1000 or more keeps
            // fewer.
            if (random.chance (90))
                return static_cast<double> (random.between (20000, 999999)) / 1000.0;

            return static_cast<double> (random.between (1000000, 9999999)) / 1000.0;
        }

        juce::ValueTree timeline()
        {
            juce::ValueTree node (ids::Timeline);
            std::vector<juce::ValueTree> children;

            // One of each at tick 0, and never two at one tick (invariant 6).
            for (const auto tempoTick : distinctTicksFromZero (random.intBetween (1, 4)))
            {
                juce::ValueTree tempoEvent (ids::TempoEvent);

                setTick (tempoEvent, ids::tick, tempoTick);
                setDouble (tempoEvent, ids::bpm, bpm());

                children.push_back (tempoEvent);
            }

            for (const auto timeSigTick : distinctTicksFromZero (random.intBetween (1, 3)))
            {
                juce::ValueTree timeSigEvent (ids::TimeSigEvent);

                setTick (timeSigEvent, ids::tick, timeSigTick);
                setInt (timeSigEvent, ids::numerator, random.intBetween (1, 32));
                setInt (timeSigEvent, ids::denominator, 1 << random.intBetween (0, 6));

                children.push_back (timeSigEvent);
            }

            // Markers may share a tick.
            for (auto count = random.intBetween (0, 3); count > 0; --count)
            {
                juce::ValueTree marker (ids::Marker);

                setTick (marker, ids::tick, tick());
                setTextOrOmit (marker, ids::name, text(), "");

                children.push_back (marker);
            }

            // Document order is not time order.
            appendInSomeOrder (node, children, 50);

            return node;
        }

        //======================================================================
        // ClipPool.

        juce::ValueTree clipPool()
        {
            juce::ValueTree node (ids::ClipPool);

            for (auto count = random.intBetween (0, 6); count > 0; --count)
            {
                const auto shape = random.intBetween (1, 100);

                if (shape <= 40)
                    node.appendChild (stepClip(), nullptr);
                else if (shape <= 75)
                    node.appendChild (eventClip (false), nullptr);
                else
                    node.appendChild (eventClip (true), nullptr);
            }

            return node;
        }

        // What every clip has, in the order of its table.
        juce::ValueTree clipOfKind (const juce::String& kind, const juce::String& id)
        {
            juce::ValueTree node (ids::Clip);

            setText (node, ids::id, id);
            setTextOrOmit (node, ids::name, text(), "");
            setText (node, ids::kind, kind);
            setTextOrOmit (node, ids::colour, colour(), "");

            return node;
        }

        juce::ValueTree eventClip (bool holdsOnlyCcEvents)
        {
            const auto id = uuid();
            auto node = clipOfKind ("event", id);

            std::int64_t loopLength = 0;

            if (random.chance (60))
                loopLength = random.oneOf (std::vector<std::int64_t> { 960, 1200, 1920, 3840, 15360 });
            else if (random.chance (95))
                loopLength = random.between (1, 20000);
            else
                loopLength = random.between (1, std::int64_t { 1 } << 36);

            setTick (node, ids::loopLength, loopLength);
            setTextOrOmit (node, ids::quantise, random.oneOf (std::vector<juce::String> { "", "", "1/2", "1/4", "1/8", "1/2T", "1/4T", "1/8T" }), "");

            // Content past the loop length is kept and never played, so a
            // tick may lie anywhere up to twice the loop.
            const auto latestTick = 2 * loopLength;
            std::vector<juce::ValueTree> content;

            if (! holdsOnlyCcEvents)
                for (auto count = random.intBetween (0, 10); count > 0; --count)
                    content.push_back (note (latestTick));

            for (auto count = random.intBetween (0, 8); count > 0; --count)
                content.push_back (event (latestTick, holdsOnlyCcEvents));

            // Notes and events stand mixed, in no order of time: the order of
            // the children is the tie-breaker at equal ticks and nothing else.
            random.shuffle (content);

            juce::ValueTree events (ids::Events);

            for (const auto& child : content)
                events.appendChild (child, nullptr);

            node.appendChild (events, nullptr);
            clips.push_back ({ id, loopLength, holdsOnlyCcEvents });

            return node;
        }

        juce::ValueTree note (std::int64_t latestTick)
        {
            juce::ValueTree node (ids::Note);

            setTick (node, ids::tick, random.between (0, latestTick));
            setTick (node, ids::length, random.chance (80) ? random.between (1, 3840) : length());
            setInt (node, ids::pitch, random.intBetween (0, 127));
            setIntOrOmit (node, ids::velocity, random.chance (30) ? 100 : random.intBetween (1, 127), 100);
            setIntOrOmit (node, ids::offVelocity, random.chance (30) ? 64 : random.intBetween (0, 127), 64);
            setBoolOrOmit (node, ids::enabled, random.chance (80), true);

            return node;
        }

        // The table of docs/SCHEMA.md -> Event: what a kind does not use
        // stays at its default.
        juce::ValueTree event (std::int64_t latestTick, bool ccOnly)
        {
            static const std::vector<juce::String> kinds { "cc", "cc", "pitchBend", "programChange", "channelPressure", "polyPressure", "sysex" };

            juce::ValueTree node (ids::Event);
            const auto kind = ccOnly ? juce::String ("cc") : random.oneOf (kinds);

            setTick (node, ids::tick, random.between (0, latestTick));
            setText (node, ids::kind, kind);

            if (kind == "cc" || kind == "polyPressure")
            {
                setIntOrOmit (node, ids::a, random.intBetween (0, 127), 0);
                setIntOrOmit (node, ids::b, random.intBetween (0, 127), 0);
            }
            else if (kind == "pitchBend")
            {
                setIntOrOmit (node, ids::a, random.chance (20) ? 8192 : random.intBetween (0, 16383), 0);
            }
            else if (kind == "programChange" || kind == "channelPressure")
            {
                setIntOrOmit (node, ids::a, random.intBetween (0, 127), 0);
            }
            else
            {
                // A payload of whole bytes, without F0 and F7.
                setTextOrOmit (node, ids::data, hexDigits (2 * random.intBetween (0, 12)), "");
            }

            setBoolOrOmit (node, ids::enabled, random.chance (80), true);

            return node;
        }

        juce::ValueTree stepClip()
        {
            const auto id = uuid();
            auto node = clipOfKind ("step", id);

            const std::int64_t stepLength = random.chance (70) ? random.oneOf (std::vector<std::int64_t> { 120, 240, 240, 480, 320 })
                                                               : random.between (1, 3840);
            const auto stepCount = random.chance (40) ? random.oneOf (std::vector<int> { 8, 16, 16, 32 })
                                                      : random.intBetween (1, random.chance (80) ? 12 : 64);
            const auto resetAt = random.chance (50) ? stepCount : random.intBetween (1, stepCount);

            // '1' = the column takes no time. One column before resetAt is
            // kept from being skipped, so that the loop has a length
            // (invariant 2).
            const auto columnThatPlays = random.intBetween (0, resetAt - 1);
            const auto percentSkipped = random.chance (50) ? 0 : 25;
            juce::String skipMask;
            int audibleColumns = 0;

            for (int column = 0; column < stepCount; ++column)
            {
                const auto isSkipped = column != columnThatPlays && random.chance (percentSkipped);

                skipMask << (isSkipped ? '1' : '0');

                if (! isSkipped && column < resetAt)
                    ++audibleColumns;
            }

            juce::ValueTree grid (ids::Grid);

            setTickOrOmit (grid, ids::stepLength, stepLength, 240);
            setIntOrOmit (grid, ids::stepCount, stepCount, 16);
            setIntOrOmit (grid, ids::resetAt, resetAt, stepCount);
            setTextOrOmit (grid, ids::skipMask, skipMask, juce::String::repeatedString ("0", stepCount));

            // One melodic lane is a 960-style sequence, several with a fixed
            // pitch are a drum pattern; the schema allows any mix, and no
            // lane at all.
            for (auto count = random.intBetween (0, 3); count > 0; --count)
                grid.appendChild (lane (stepCount), nullptr);

            node.appendChild (grid, nullptr);
            clips.push_back ({ id, stepLength * audibleColumns, false });

            return node;
        }

        juce::ValueTree lane (int stepCount)
        {
            juce::ValueTree node (ids::Lane);

            setText (node, ids::id, uuid());
            setTextOrOmit (node, ids::name, text(), "");

            // No default: a lane without it is a melodic row.
            if (random.chance (50))
                setInt (node, ids::fixedPitch, random.intBetween (0, 127));

            for (int column = 0; column < stepCount; ++column)
                node.appendChild (cell(), nullptr);

            return node;
        }

        double gate()
        {
            if (random.chance (60))
                return random.oneOf (std::vector<double> { 0.5, 0.5, 0.25, 1.0 / 3.0, 1.0, 1.2, 4.0, 0.00001 });

            return static_cast<double> (random.between (1, 4000000)) / 1000000.0;
        }

        juce::ValueTree cell()
        {
            juce::ValueTree node (ids::Cell);

            // Many cells are as empty as a rest in a drum row.
            if (random.chance (40))
                return node;

            setIntOrOmit (node, ids::pitch, random.chance (20) ? 60 : random.intBetween (0, 127), 60);
            setBoolOrOmit (node, ids::on, random.chance (60), false);
            setIntOrOmit (node, ids::velocity, random.chance (30) ? 100 : random.intBetween (1, 127), 100);

            const auto cellGate = gate();

            if (! leavesOut (juce::exactlyEqual (cellGate, 0.5)))
                setDouble (node, ids::gate, cellGate);

            // One value per controller number (invariant 11).
            for (const auto controller : distinctInts (random.chance (70) ? 0 : random.intBetween (1, 3), 0, 127))
            {
                juce::ValueTree ccValue (ids::CcValue);

                setInt (ccValue, ids::cc, controller);
                setInt (ccValue, ids::value, random.intBetween (0, 127));

                node.appendChild (ccValue, nullptr);
            }

            return node;
        }

        //======================================================================
        // Tracks.

        juce::ValueTree tracks()
        {
            juce::ValueTree node (ids::Tracks);

            for (auto count = random.intBetween (0, 4); count > 0; --count)
                node.appendChild (track(), nullptr);

            return node;
        }

        // The id of one of the nodes made, or empty for unassigned.
        juce::String referenceTo (const std::vector<juce::String>& idsOfTargets)
        {
            if (idsOfTargets.empty() || random.chance (30))
                return {};

            return random.oneOf (idsOfTargets);
        }

        juce::ValueTree track()
        {
            juce::ValueTree node (ids::Track);

            setText (node, ids::id, uuid());
            setTextOrOmit (node, ids::name, text(), "");
            setTextOrOmit (node, ids::colour, colour(), "");
            setTextOrOmit (node, ids::playerId, referenceTo (playerIds), "");
            setIntOrOmit (node, ids::channel, random.intBetween (1, 16), 1);
            setTextOrOmit (node, ids::performerId, referenceTo (performerIds), "");
            setBoolOrOmit (node, ids::mute, random.chance (20), false);
            setBoolOrOmit (node, ids::solo, random.chance (20), false);

            std::vector<ClipRecord> automationClips;

            for (const auto& clip : clips)
                if (clip.holdsOnlyCcEvents)
                    automationClips.push_back (clip);

            juce::ValueTree placements (ids::Placements);
            addPlacements (placements, clips);

            juce::ValueTree automationLanes (ids::AutomationLanes);

            for (auto count = random.intBetween (0, 2); count > 0; --count)
            {
                juce::ValueTree automationLane (ids::AutomationLane);

                setText (automationLane, ids::id, uuid());
                setTextOrOmit (automationLane, ids::name, text(), "");
                addPlacements (automationLane, automationClips);

                automationLanes.appendChild (automationLane, nullptr);
            }

            juce::ValueTree knobs (ids::Knobs);

            for (const auto controller : distinctInts (random.intBetween (0, 4), 0, 127))
            {
                juce::ValueTree knob (ids::Knob);

                setInt (knob, ids::cc, controller);
                setTextOrOmit (knob, ids::label, text(), "");

                knobs.appendChild (knob, nullptr);
            }

            // Sparse: a few scenes of the first sixteen, each at most once.
            juce::ValueTree slots (ids::Slots);

            if (! clips.empty())
            {
                for (const auto scene : distinctInts (random.intBetween (0, 3), 0, 15))
                {
                    juce::ValueTree slot (ids::Slot);

                    setInt (slot, ids::scene, scene);
                    setText (slot, ids::clipId, random.oneOf (clips).id);

                    slots.appendChild (slot, nullptr);
                }
            }

            appendInSomeOrder (node, { placements, automationLanes, knobs, slots }, 20);

            return node;
        }

        // Placements of the given clips under one parent, a track's
        // Placements or an AutomationLane: one after the other along the
        // timeline, so that no two overlap (invariant 3), then stored in any
        // order.
        void addPlacements (juce::ValueTree& parent, const std::vector<ClipRecord>& candidates)
        {
            if (candidates.empty())
                return;

            std::vector<juce::ValueTree> placements;

            auto clip = random.oneOf (candidates);
            std::int64_t end = random.chance (50) ? 0 : tick();
            std::int64_t phaseAtEnd = 0;

            for (auto count = random.intBetween (0, 4); count > 0; --count)
            {
                // As after a split: the same clip, right behind the last
                // placement, going on where that one stopped in the loop.
                const auto continuesTheLast = ! placements.empty() && random.chance (25);

                if (! continuesTheLast)
                {
                    clip = random.oneOf (candidates);

                    if (random.chance (60))
                        end += length();
                }

                std::int64_t placementLength = 0;

                if (random.chance (40))
                    placementLength = clip.loopLength * random.between (1, 4);
                else if (random.chance (50))
                    placementLength = random.between (1, clip.loopLength);
                else
                    placementLength = length();

                std::int64_t offset = 0;

                if (continuesTheLast)
                    offset = phaseAtEnd;
                else if (random.chance (40))
                    offset = random.between (0, clip.loopLength - 1);

                juce::ValueTree placement (ids::Placement);

                setText (placement, ids::id, uuid());
                setText (placement, ids::clipId, clip.id);
                setTick (placement, ids::start, end);
                setTick (placement, ids::length, placementLength);
                setTickOrOmit (placement, ids::offset, offset, 0);

                placements.push_back (placement);

                end += placementLength;
                phaseAtEnd = (offset + placementLength % clip.loopLength) % clip.loopLength;
            }

            random.shuffle (placements);

            for (const auto& placement : placements)
                parent.appendChild (placement, nullptr);
        }

        //======================================================================
        // Session and UiState.

        juce::ValueTree session()
        {
            juce::ValueTree node (ids::Session);

            const std::int64_t launchQuantise = random.chance (70) ? random.oneOf (std::vector<std::int64_t> { 3840, 3840, 0, 960, 1920 })
                                                                   : tick();

            setTickOrOmit (node, ids::launchQuantise, launchQuantise, 3840);

            // Sparse: only the scenes that have a node have a name.
            for (const auto index : distinctInts (random.intBetween (0, 4), 0, 15))
            {
                juce::ValueTree scene (ids::Scene);

                setInt (scene, ids::index, index);
                setTextOrOmit (scene, ids::name, text(), "");

                node.appendChild (scene, nullptr);
            }

            return node;
        }

        juce::ValueTree uiState()
        {
            juce::ValueTree node (ids::UiState);

            setTextOrOmit (node, ids::activeTab, random.oneOf (std::vector<juce::String> { "arrangement", "routing", "pianoroll", "drums", "stepseq", "automation" }), "arrangement");
            setTextOrOmit (node, ids::sharedEditPolicy, random.oneOf (std::vector<juce::String> { "updateAll", "makeUnique" }), "updateAll");

            // An enabled loop starts before it ends; one that is switched off
            // may hold anything that is no negative tick.
            const auto loopEnabled = random.chance (40);
            const auto loopStart = random.chance (50) ? 0 : tick();
            const auto loopEnd = loopEnabled ? loopStart + length() : (random.chance (50) ? 0 : tick());

            setTickOrOmit (node, ids::loopStart, loopStart, 0);
            setTickOrOmit (node, ids::loopEnd, loopEnd, 0);
            setBoolOrOmit (node, ids::loopEnabled, loopEnabled, false);
            setTickOrOmit (node, ids::exportLength, random.chance (60) ? 0 : tick(), 0);

            return node;
        }

        RandomSource random;

        std::vector<juce::String> performerIds;
        std::vector<juce::String> playerIds;
        std::vector<ClipRecord> clips;
    };

} // namespace

juce::ValueTree DocumentGenerator::generate (std::uint64_t seed)
{
    return Builder (seed).project();
}

} // namespace guardrails
