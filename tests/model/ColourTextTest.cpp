// model::ColourText: a colour as the document holds it.
// docs/SCHEMA.md -> Conventions.

#include <catch2/catch_test_macros.hpp>

#include "model/ColourText.h"

TEST_CASE ("A colour is '#' and eight hexadecimal digits in either case, or nothing", "[model][ColourText]")
{
    CHECK (model::ColourText::isValid (""));
    CHECK (model::ColourText::isValid ("#ff00aa10"));
    CHECK (model::ColourText::isValid ("#FF00AA10"));
    CHECK (model::ColourText::isValid ("#00000000"));
    CHECK (model::ColourText::isValid ("#aBcDeF09"));
}

TEST_CASE ("Any other text is no colour", "[model][ColourText]")
{
    CHECK_FALSE (model::ColourText::isValid ("red"));
    CHECK_FALSE (model::ColourText::isValid ("#"));
    CHECK_FALSE (model::ColourText::isValid ("#ff0000"));     // six digits
    CHECK_FALSE (model::ColourText::isValid ("#ffff000000")); // ten digits
    CHECK_FALSE (model::ColourText::isValid ("ffff00000"));   // no '#'
    CHECK_FALSE (model::ColourText::isValid ("#ff00000g"));
    CHECK_FALSE (model::ColourText::isValid (" #ff00aa10"));
    CHECK_FALSE (model::ColourText::isValid ("#ff00aa1 "));
    CHECK_FALSE (model::ColourText::isValid ("##f00aa10"));
}
