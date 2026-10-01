--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
mb_encoding_aliases(): php's alias list for each encoding this engine models -- and the same table at every encoding ARGUMENT, so every alias php accepts is one PHL accepts. Whitespace is trimmed by a comma-separated LIST and by nothing else.
--FILE--
<?php
// The five encodings this engine models, and php's own alias list for each.
foreach (['UTF-8', '8bit', 'ISO-8859-1', 'ASCII', 'Windows-1252'] as $enc) {
    printf("%-13s %s\n", $enc, json_encode(mb_encoding_aliases($enc)));
}

// An ALIAS resolves to its encoding's list, and the lookup is case-insensitive.
foreach (['utf8', 'binary', 'latin1', 'ISO8859-1', 'csASCII', 'CP1252', 'Us'] as $enc) {
    printf("%-13s %s\n", $enc, json_encode(mb_encoding_aliases($enc)));
}

// A name outside the set is the family's ValueError, and so is the empty string.
foreach (['nosuchenc', ''] as $enc) {
    try { mb_encoding_aliases($enc); }
    catch (ValueError $e) { echo "refused: ", $e->getMessage(), "\n"; }
}

// Every alias php lists is a name every encoding ARGUMENT takes, not just this one.
$aliases = array_merge(...array_map('mb_encoding_aliases', ['UTF-8', '8bit', 'ISO-8859-1', 'ASCII', 'Windows-1252']));
$took = [];
foreach ($aliases as $a) {
    $took[] = mb_strlen('abc', $a) === 3
        && mb_str_split('abc', 1, $a) === ['a', 'b', 'c']
        && mb_convert_encoding('abc', 'UTF-8', $a) === 'abc';
}
var_dump(count($aliases), count(array_filter($took)));

// WHITESPACE is trimmed by a comma-separated LIST and by nothing else.
try { mb_strlen('abc', ' utf8'); echo "single trimmed\n"; }
catch (ValueError $e) { echo "single: ", $e->getMessage(), "\n"; }
try { mb_convert_encoding('abc', ' UTF-8 '); echo "to-encoding trimmed\n"; }
catch (ValueError $e) { echo "to-encoding: ", $e->getMessage(), "\n"; }
var_dump(mb_convert_encoding('abc', 'UTF-8', ' utf8 ,  ascii '));
try { mb_detect_encoding('abc', [' UTF-8']); echo "array element trimmed\n"; }
catch (ValueError $e) { echo "array element: ", $e->getMessage(), "\n"; }
// ...and the list door quotes the TRIMMED token back.
try { mb_detect_encoding('abc', 'UTF-8,  nope  '); }
catch (ValueError $e) { echo "list: ", $e->getMessage(), "\n"; }
?>
--EXPECT--
UTF-8         ["utf8"]
8bit          ["binary"]
ISO-8859-1    ["ISO8859-1","latin1"]
ASCII         ["ANSI_X3.4-1968","iso-ir-6","ANSI_X3.4-1986","ISO_646.irv:1991","US-ASCII","ISO646-US","us","IBM367","IBM-367","cp367","csASCII"]
Windows-1252  ["cp1252"]
utf8          ["utf8"]
binary        ["binary"]
latin1        ["ISO8859-1","latin1"]
ISO8859-1     ["ISO8859-1","latin1"]
csASCII       ["ANSI_X3.4-1968","iso-ir-6","ANSI_X3.4-1986","ISO_646.irv:1991","US-ASCII","ISO646-US","us","IBM367","IBM-367","cp367","csASCII"]
CP1252        ["cp1252"]
Us            ["ANSI_X3.4-1968","iso-ir-6","ANSI_X3.4-1986","ISO_646.irv:1991","US-ASCII","ISO646-US","us","IBM367","IBM-367","cp367","csASCII"]
refused: mb_encoding_aliases(): Argument #1 ($encoding) must be a valid encoding, "nosuchenc" given
refused: mb_encoding_aliases(): Argument #1 ($encoding) must be a valid encoding, "" given
int(16)
int(16)
single: mb_strlen(): Argument #2 ($encoding) must be a valid encoding, " utf8" given
to-encoding: mb_convert_encoding(): Argument #2 ($to_encoding) must be a valid encoding, " UTF-8 " given
string(3) "abc"
array element: mb_detect_encoding(): Argument #2 ($encodings) contains invalid encoding " UTF-8"
list: mb_detect_encoding(): Argument #2 ($encodings) contains invalid encoding "nope"
--CLEAN--
<?php
