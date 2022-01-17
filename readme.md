# kgStruct

Import and export your C structures from external sources.

(Currently only JSON module is implemented.)

Disclaimer:
This is still in development. While you can already use this, i can change API at some point.
Also, before you use this, make sure it works as expected. I did some tests but it is not possible to test for every variation of source data.

## Basic info

All structures you want to work with using this library are defined using teplate JSON, `structs.json`.
This file is then processed by `ks_generator.py`, which will generate header file with C structures.

(It will also generate source code containing all the templates for parsers.)

Arrays, arrays of structures, structures inside structures and array structures inside structures ... inside structures ... are supported.

Numeric values can have `minimum` and `maximum` defined.

## JSON Editor and schema

Template parsing script is capable of producing a schema for JSON Editor.
Documentation / example pending.

## Template documentation

Hmm ... maybe later ...

See `structs.json` for hints.

## JSON parser / exporter

Important settings in `ks_json.h`.

JSON parser is capable of sequenced importing / exporting. That is, you don't have to provide the entire JSON buffer for handling.
JSON parser is capable of skipping unknown values.

- `KS_JSON_MAX_STRING_LENGTH` Maximum string length in bytes. NUL byte terminator also counts.
  - This affects *any* JSON string - so keys and values.
- `KS_JSON_MAX_DEPTH` Maximum JSON recursion.
  - Keep in mind that ignored values are also counted into recursion.
- `KS_JSON_ALLOW_STRING_NUMBERS` Allow parsing numeric values from JSON strings.
  - This exists to support broken generators that export everything as a strings.

## TODO

Hmm ...

- Documentation
- More parsers
  - CBOR
  - ...
