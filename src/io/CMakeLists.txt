# io/: ProjectFile, XmlWellFormedness, SmfExport, SmfImport, Migrations.
#
# io/ may include render/ (CLAUDE.md -> Layers and dependencies), but only SMF
# export and import need it, and they arrive at milestone 2 together with
# render/'s first sources. Until then hums_render is not a target, and naming
# it here would stop configure. Milestone 1's io/ (ProjectFile, XmlWellFormedness, Migrations)
# includes model/ only. Add hums_render to LAYERS with the first Smf* source
# (docs/MILESTONES.md, milestone 2).
hums_add_layer(hums_io
    SOURCES
        Migrations.cpp
        Migrations.h
        ProjectFile.cpp
        ProjectFile.h
        XmlWellFormedness.cpp
        XmlWellFormedness.h
    LAYERS hums_model
    JUCE)
