// model::Settings: the settings path, the one way to read and write UiState.
// docs/SCHEMA.md -> UiState.

#include <catch2/catch_test_macros.hpp>

#include "model/Ids.h"
#include "model/Settings.h"

#include <utility>
#include <vector>

namespace
{

juce::ValueTree emptyUiState()
{
    return juce::ValueTree::fromXml ("<UiState/>");
}

juce::ValueTree storedUiState()
{
    return juce::ValueTree::fromXml (
        R"(<UiState activeTab="stepseq" sharedEditPolicy="makeUnique"
                    loopStart="3840" loopEnd="7680" loopEnabled="1" exportLength="15360"/>)");
}

// What a save and a reload make of a node: every property a string again.
juce::ValueTree savedAndReloaded (const juce::ValueTree& tree)
{
    return juce::ValueTree::fromXml (tree.toXmlString());
}

// Notes every property change it hears of, in order.
class ChangeListener : public juce::ValueTree::Listener
{
public:
    void valueTreePropertyChanged (juce::ValueTree& changedNode, const juce::Identifier& property) override
    {
        changedProperties.push_back (property);

        const model::Settings settings (changedNode);
        const auto loop = settings.getTransportLoop();

        if (loop.enabled && loop.start >= loop.end)
            sawInvalidLoop = true;
    }

    std::vector<juce::Identifier> changedProperties;
    bool sawInvalidLoop = false;
};

} // namespace

TEST_CASE ("Settings give back what is stored", "[model][Settings]")
{
    const model::Settings settings (storedUiState());

    CHECK (settings.getActiveTab() == model::Settings::Tab::stepseq);
    CHECK (settings.getSharedEditPolicy() == model::Settings::SharedEditPolicy::makeUnique);
    CHECK (settings.getTransportLoop().start == 3840);
    CHECK (settings.getTransportLoop().end == 7680);
    CHECK (settings.getTransportLoop().enabled);
    CHECK (settings.getExportLength() == 15360);
}

TEST_CASE ("Settings of an empty UiState are the schema's defaults", "[model][Settings]")
{
    const model::Settings settings (emptyUiState());

    CHECK (settings.getActiveTab() == model::Settings::Tab::arrangement);
    CHECK (settings.getSharedEditPolicy() == model::Settings::SharedEditPolicy::updateAll);
    CHECK (settings.getTransportLoop().start == 0);
    CHECK (settings.getTransportLoop().end == 0);
    CHECK_FALSE (settings.getTransportLoop().enabled);
    // 0 = the arrangement end.
    CHECK (settings.getExportLength() == 0);
}

TEST_CASE ("Settings store a stored updateAll policy as such", "[model][Settings]")
{
    const model::Settings settings (juce::ValueTree::fromXml (R"(<UiState sharedEditPolicy="updateAll"/>)"));

    CHECK (settings.getSharedEditPolicy() == model::Settings::SharedEditPolicy::updateAll);
}

TEST_CASE ("Settings write every setting, and it survives save and reload", "[model][Settings]")
{
    auto tree = emptyUiState();
    model::Settings settings (tree);

    settings.setActiveTab (model::Settings::Tab::pianoroll);
    settings.setSharedEditPolicy (model::Settings::SharedEditPolicy::makeUnique);
    REQUIRE (settings.setTransportLoop ({ 960, 4800, true }));
    REQUIRE (settings.setExportLength (30720));

    // Once as written in this session, once as a reloaded file holds it.
    for (const auto& node : { tree, savedAndReloaded (tree) })
    {
        const model::Settings read (node);

        CHECK (read.getActiveTab() == model::Settings::Tab::pianoroll);
        CHECK (read.getSharedEditPolicy() == model::Settings::SharedEditPolicy::makeUnique);
        CHECK (read.getTransportLoop().start == 960);
        CHECK (read.getTransportLoop().end == 4800);
        CHECK (read.getTransportLoop().enabled);
        CHECK (read.getExportLength() == 30720);
    }
}

TEST_CASE ("Settings store and read every tab by the name the schema lists", "[model][Settings]")
{
    using Tab = model::Settings::Tab;

    const std::vector<std::pair<Tab, juce::String>> tabs {
        { Tab::routing, "routing" },
        { Tab::pianoroll, "pianoroll" },
        { Tab::arrangement, "arrangement" },
        { Tab::drums, "drums" },
        { Tab::stepseq, "stepseq" },
        { Tab::automation, "automation" },
    };

    for (const auto& [tab, text] : tabs)
    {
        // Written from another tab, so that the write happens.
        auto tree = emptyUiState();
        model::Settings settings (tree);
        settings.setActiveTab (tab == Tab::routing ? Tab::drums : Tab::routing);
        settings.setActiveTab (tab);

        CHECK (tree[model::ids::activeTab].toString() == text);
        CHECK (model::Settings (savedAndReloaded (tree)).getActiveTab() == tab);
    }
}

TEST_CASE ("Settings store the texts and numbers the schema names", "[model][Settings]")
{
    auto tree = emptyUiState();
    model::Settings settings (tree);

    settings.setSharedEditPolicy (model::Settings::SharedEditPolicy::makeUnique);
    REQUIRE (settings.setTransportLoop ({ 0, 3840, true }));

    CHECK (tree[model::ids::sharedEditPolicy].toString() == "makeUnique");
    CHECK (tree[model::ids::loopEnd].toString() == "3840");
    // A boolean is stored as 0 / 1.
    CHECK (tree[model::ids::loopEnabled].toString() == "1");
    // The start was 0 already, so nothing was written for it.
    CHECK_FALSE (tree.hasProperty (model::ids::loopStart));

    settings.setSharedEditPolicy (model::Settings::SharedEditPolicy::updateAll);
    REQUIRE (settings.setTransportLoop ({ 0, 3840, false }));

    CHECK (tree[model::ids::sharedEditPolicy].toString() == "updateAll");
    CHECK (tree[model::ids::loopEnabled].toString() == "0");
}

TEST_CASE ("Settings can set the policy back to updateAll", "[model][Settings]")
{
    model::Settings settings (storedUiState());

    settings.setSharedEditPolicy (model::Settings::SharedEditPolicy::updateAll);

    CHECK (settings.getSharedEditPolicy() == model::Settings::SharedEditPolicy::updateAll);
}

TEST_CASE ("Settings keep the loop points of a loop that is switched off", "[model][Settings]")
{
    model::Settings settings (storedUiState());

    REQUIRE (settings.setTransportLoop ({ 3840, 7680, false }));

    CHECK (settings.getTransportLoop().start == 3840);
    CHECK (settings.getTransportLoop().end == 7680);
    CHECK_FALSE (settings.getTransportLoop().enabled);
}

TEST_CASE ("Settings accept a disabled loop whose start is not before its end", "[model][Settings]")
{
    // Only an enabled loop needs start < end; the default, 0 to 0, is such a loop.
    model::Settings settings (emptyUiState());

    REQUIRE (settings.setTransportLoop ({ 960, 960, false }));

    CHECK (settings.getTransportLoop().start == 960);
    CHECK (settings.getTransportLoop().end == 960);
}

TEST_CASE ("Settings refuse an enabled loop whose start is not before its end", "[model][Settings]")
{
    auto tree = storedUiState();
    model::Settings settings (tree);
    const auto before = tree.toXmlString();

    CHECK_FALSE (settings.setTransportLoop ({ 960, 960, true }));
    CHECK_FALSE (settings.setTransportLoop ({ 1920, 960, true }));
    CHECK_FALSE (settings.setTransportLoop ({ 0, 0, true }));

    CHECK (tree.toXmlString() == before);
}

TEST_CASE ("Settings refuse a loop with a negative tick", "[model][Settings]")
{
    auto tree = storedUiState();
    model::Settings settings (tree);
    const auto before = tree.toXmlString();

    CHECK_FALSE (settings.setTransportLoop ({ -1, 960, true }));
    CHECK_FALSE (settings.setTransportLoop ({ -960, -1, true }));
    CHECK_FALSE (settings.setTransportLoop ({ -1, 0, false }));
    CHECK_FALSE (settings.setTransportLoop ({ 0, -1, false }));

    CHECK (tree.toXmlString() == before);
}

TEST_CASE ("Settings refuse a negative export length", "[model][Settings]")
{
    auto tree = storedUiState();
    model::Settings settings (tree);
    const auto before = tree.toXmlString();

    CHECK_FALSE (settings.setExportLength (-1));

    CHECK (tree.toXmlString() == before);
}

TEST_CASE ("Settings can set the export length back to the arrangement end", "[model][Settings]")
{
    model::Settings settings (storedUiState());

    REQUIRE (settings.setExportLength (0));

    CHECK (settings.getExportLength() == 0);
}

TEST_CASE ("Settings tell a listener of each setting that changed", "[model][Settings]")
{
    auto tree = emptyUiState();
    ChangeListener listener;
    tree.addListener (&listener);
    model::Settings settings (tree);

    settings.setActiveTab (model::Settings::Tab::drums);
    settings.setSharedEditPolicy (model::Settings::SharedEditPolicy::makeUnique);
    REQUIRE (settings.setExportLength (960));

    REQUIRE (listener.changedProperties.size() == 3);
    CHECK (listener.changedProperties[0] == model::ids::activeTab);
    CHECK (listener.changedProperties[1] == model::ids::sharedEditPolicy);
    CHECK (listener.changedProperties[2] == model::ids::exportLength);
}

TEST_CASE ("Settings tell no listener of a value that stays the same", "[model][Settings]")
{
    // Both as loaded from a file (strings) and as written in this session
    // (numbers): the same value is the same value, whatever holds it.
    auto loaded = storedUiState();
    auto written = emptyUiState();
    {
        model::Settings settings (written);
        settings.setActiveTab (model::Settings::Tab::stepseq);
        settings.setSharedEditPolicy (model::Settings::SharedEditPolicy::makeUnique);
        REQUIRE (settings.setTransportLoop ({ 3840, 7680, true }));
        REQUIRE (settings.setExportLength (15360));
    }

    for (auto tree : { loaded, written })
    {
        ChangeListener listener;
        tree.addListener (&listener);
        model::Settings settings (tree);

        settings.setActiveTab (model::Settings::Tab::stepseq);
        settings.setSharedEditPolicy (model::Settings::SharedEditPolicy::makeUnique);
        CHECK (settings.setTransportLoop ({ 3840, 7680, true }));
        CHECK (settings.setExportLength (15360));

        CHECK (listener.changedProperties.empty());
    }
}

TEST_CASE ("Settings tell no listener of a default set on an empty UiState", "[model][Settings]")
{
    auto tree = emptyUiState();
    ChangeListener listener;
    tree.addListener (&listener);
    model::Settings settings (tree);

    settings.setActiveTab (model::Settings::Tab::arrangement);
    settings.setSharedEditPolicy (model::Settings::SharedEditPolicy::updateAll);
    CHECK (settings.setTransportLoop ({ 0, 0, false }));
    CHECK (settings.setExportLength (0));

    CHECK (listener.changedProperties.empty());
    CHECK (tree.getNumProperties() == 0);
}

TEST_CASE ("Settings never show a listener an enabled loop with start not before end", "[model][Settings]")
{
    auto tree = storedUiState(); // 3840 to 7680, enabled
    ChangeListener listener;
    tree.addListener (&listener);
    model::Settings settings (tree);

    // To the right, past the old end; back to the left, before the old start;
    // wider; narrower; off with points that would be invalid when on; on again.
    REQUIRE (settings.setTransportLoop ({ 9600, 11520, true }));
    REQUIRE (settings.setTransportLoop ({ 0, 960, true }));
    REQUIRE (settings.setTransportLoop ({ 0, 19200, true }));
    REQUIRE (settings.setTransportLoop ({ 7680, 8640, true }));
    REQUIRE (settings.setTransportLoop ({ 9600, 960, false }));
    REQUIRE (settings.setTransportLoop ({ 960, 1920, true }));

    CHECK_FALSE (listener.sawInvalidLoop);
    CHECK (settings.getTransportLoop().start == 960);
    CHECK (settings.getTransportLoop().end == 1920);
    CHECK (settings.getTransportLoop().enabled);
}

TEST_CASE ("Settings never touch the undo history", "[model][Settings]")
{
    // The undoable part of the document changes through the UndoManager; a
    // setting changed in between is neither undone with it nor in its way.
    auto project = juce::ValueTree::fromXml (R"(<Project schemaVersion="1" name="Before"><UiState/></Project>)");
    juce::UndoManager undoManager;
    model::Settings settings (project.getChildWithName (model::ids::UiState));

    undoManager.beginNewTransaction();
    project.setProperty (model::ids::name, "After", &undoManager);
    settings.setActiveTab (model::Settings::Tab::drums);
    REQUIRE (settings.setTransportLoop ({ 0, 3840, true }));

    REQUIRE (undoManager.undo());

    CHECK (project[model::ids::name].toString() == "Before");
    CHECK (settings.getActiveTab() == model::Settings::Tab::drums);
    CHECK (settings.getTransportLoop().enabled);
    CHECK_FALSE (undoManager.canUndo());

    // Changing a setting after an undo leaves the redo in place.
    settings.setActiveTab (model::Settings::Tab::stepseq);

    REQUIRE (undoManager.redo());
    CHECK (project[model::ids::name].toString() == "After");
}

TEST_CASE ("Two Settings on one UiState see each other's changes", "[model][Settings]")
{
    // A lens, not a copy: it holds the node, not its values.
    auto tree = emptyUiState();
    model::Settings writer (tree);
    const model::Settings reader (tree);

    writer.setActiveTab (model::Settings::Tab::routing);

    CHECK (reader.getActiveTab() == model::Settings::Tab::routing);
}
