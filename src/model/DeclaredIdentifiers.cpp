#include "model/DeclaredIdentifiers.h"

#include "model/Ids.h"

namespace model
{

const std::vector<juce::Identifier>& DeclaredIdentifiers::inDeclarationOrder()
{
    // Built at the first call, not at program start, so that the constants
    // of Ids.h are sure to exist. The lines are grouped as in Ids.h.
    static const std::vector<juce::Identifier> identifiers {
        // Node types.
        ids::Project,

        ids::Routing,
        ids::Performer,
        ids::Player,
        ids::PadMap,
        ids::Pad,

        ids::Timeline,
        ids::TempoEvent,
        ids::TimeSigEvent,
        ids::Marker,

        ids::ClipPool,
        ids::Clip,
        ids::Events,
        ids::Note,
        ids::Event,
        ids::Grid,
        ids::Lane,
        ids::Cell,
        ids::CcValue,

        ids::Tracks,
        ids::Track,
        ids::Placements,
        ids::Placement,
        ids::AutomationLanes,
        ids::AutomationLane,
        ids::Knobs,
        ids::Knob,
        ids::Slots,
        ids::Slot,

        ids::Session,
        ids::Scene,

        ids::UiState,

        // Properties.
        ids::schemaVersion,
        ids::id,
        ids::index,
        ids::inputPitch,
        ids::tick,
        ids::name,
        ids::ppq,
        ids::deviceIdentifier,
        ids::deviceName,
        ids::channelFilter,
        ids::sendClock,
        ids::bpm,
        ids::numerator,
        ids::denominator,
        ids::kind,
        ids::loopLength,
        ids::quantise,
        ids::colour,
        ids::a,
        ids::b,
        ids::data,
        ids::stepLength,
        ids::stepCount,
        ids::resetAt,
        ids::skipMask,
        ids::fixedPitch,
        ids::cc,
        ids::value,
        ids::playerId,
        ids::channel,
        ids::performerId,
        ids::mute,
        ids::solo,
        ids::scene,
        ids::clipId,
        ids::start,
        ids::length,
        ids::pitch,
        ids::label,
        ids::on,
        ids::velocity,
        ids::offVelocity,
        ids::enabled,
        ids::gate,
        ids::offset,
        ids::launchQuantise,
        ids::activeTab,
        ids::sharedEditPolicy,
        ids::loopStart,
        ids::loopEnd,
        ids::loopEnabled,
        ids::exportLength,

        // View state: none yet.
    };

    return identifiers;
}

int DeclaredIdentifiers::positionOf (const juce::Identifier& identifier)
{
    const auto& identifiers = inDeclarationOrder();

    for (std::size_t position = 0; position < identifiers.size(); ++position)
        if (identifiers[position] == identifier)
            return static_cast<int> (position);

    return -1;
}

bool DeclaredIdentifiers::isDeclared (const juce::Identifier& identifier)
{
    return positionOf (identifier) >= 0;
}

} // namespace model
