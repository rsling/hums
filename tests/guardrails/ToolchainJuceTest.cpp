// Proves the pinned JUCE builds, links and runs in this configuration —
// including under ASan + UBSan, where a JUCE finding would fail this test.

#include <catch2/catch_test_macros.hpp>

#include <juce_core/juce_core.h>
#include <juce_data_structures/juce_data_structures.h>

#include <atomic>

TEST_CASE ("ValueTree survives an XML round trip", "[toolchain]")
{
    // Literal identifiers are fine here: this is a toolchain test, not schema code.
    const juce::Identifier nodeType ("Node");
    const juce::Identifier value ("value");

    juce::ValueTree tree (nodeType);
    tree.setProperty (value, 42, nullptr);

    const auto xml = tree.toXmlString();
    const auto reloaded = juce::ValueTree::fromXml (xml);

    REQUIRE (reloaded.isEquivalentTo (tree));
    REQUIRE (reloaded.getProperty (value).toString() == "42");
}

TEST_CASE ("juce::Thread starts and joins", "[toolchain]")
{
    struct Worker final : juce::Thread
    {
        Worker() : juce::Thread ("toolchain worker") {}
        void run() override { ran = true; }
        std::atomic<bool> ran { false };
    };

    Worker worker;
    REQUIRE (worker.startThread());
    REQUIRE (worker.waitForThreadToExit (5000));
    REQUIRE (worker.ran.load());
}
