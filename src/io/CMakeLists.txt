# io/: ProjectFile, XmlWellFormedness, Migrations, and the Standard MIDI File:
# SmfFile, SmfTrack and SmfEvent (what a file holds), SmfReader and SmfWriter
# (its bytes), SmfExport, SmfImport.
#
# io/ may include render/ (CLAUDE.md -> Layers and dependencies), and
# hums_render is among its layers since the first Smf* source
# (docs/MILESTONES.md, milestone 2), although only SMF export and import will
# need it.
hums_add_layer(hums_io
    SOURCES
        Migrations.cpp
        Migrations.h
        ProjectFile.cpp
        ProjectFile.h
        SmfEvent.h
        SmfFile.h
        SmfReader.cpp
        SmfReader.h
        SmfTrack.h
        SmfWriter.cpp
        SmfWriter.h
        XmlWellFormedness.cpp
        XmlWellFormedness.h
    LAYERS hums_model hums_render
    JUCE)
