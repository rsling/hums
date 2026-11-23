// Fixture for tools/check_identifiers.py: this tree must fail the check.
// A schema name through the constructor of a class of ours, in a static
// member defined outside its class: the qualified name does not hide it.
const PropertyName NoteModel::pitchName ("pitch");
