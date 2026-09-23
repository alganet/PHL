--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
version_compare canonicalizes the way php does, down to the corners
--FILE--
<?php
// php's canonicalization copies the FIRST byte verbatim -- even a separator --
// so "-1" becomes "-.1" and its leading "-" compares as an unrecognized form
// below dev, while ".1"/"..1"/"...1" all collapse to the same two components.
// A version already starting with '#' bypasses canonicalization entirely,
// because '#' is the marker php compares a numeric component as.
$pairs = [
    ["-1", "1"], ["-1", ".1"], [".1", "..1"], [".1", "...1"], ["+1", "-1"],
    ["1", "..1"], ["0.1", "-1"], ["1", "1."], ["1.", "1"],
    ["#1", "1"], ["#N#", "1"], ["#", "1"], ["1", "1#2"], ["#1", "1#2"],
    ["..", ".."], ["..", "1"], ["1", ".."], ["", ""], ["", "1"],
    ["1..2", "1.0.2"], ["1..2", "1.1.2"], ["1_0", "1.0"], ["1+0", "1-0"],
    ["1.0.0", "1.0.0#"], ["x", ""], ["1a2", "1.a.2"],
];
foreach ($pairs as [$a, $b]) {
    echo "$a|$b=", version_compare($a, $b), "\n";
}

// Every operator spelling, against the three verdicts.
foreach (["<", "lt", "<=", "le", ">", "gt", ">=", "ge", "==", "=", "eq", "!=", "<>", "ne"] as $op) {
    echo $op, ":";
    foreach ([["1", "2"], ["1", "1"], ["2", "1"]] as [$a, $b]) {
        echo version_compare($a, $b, $op) ? "T" : "F";
    }
    echo "\n";
}

// php 8 stopped answering NULL for an operator it does not know: it is a
// ValueError, and a non-string operator reaches it through weak coercion.
foreach (["zz", "", "===", "=<", "<=>", " <"] as $op) {
    try {
        var_dump(version_compare("1", "2", $op));
    } catch (ValueError $e) {
        echo get_class($e), ": ", $e->getMessage(), "\n";
    }
}
foreach ([false, 5] as $op) {
    try {
        var_dump(version_compare("1", "2", $op));
    } catch (ValueError $e) {
        echo get_class($e), ": ", $e->getMessage(), "\n";
    }
}
// An explicit null is "no operator" and answers the integer.
var_dump(version_compare("1", "2", null));

// The declared parameter types are screened before the body runs.
try {
    version_compare([1], "1");
} catch (TypeError $e) {
    echo get_class($e), ": ", $e->getMessage(), "\n";
}
try {
    version_compare("1", "1", [1]);
} catch (TypeError $e) {
    echo get_class($e), ": ", $e->getMessage(), "\n";
}
--EXPECT--
-1|1=-1
-1|.1=0
.1|..1=0
.1|...1=0
+1|-1=0
1|..1=1
0.1|-1=1
1|1.=0
1.|1=0
#1|1=0
#N#|1=0
#|1=0
1|1#2=-1
#1|1#2=-1
..|..=-1
..|1=-1
1|..=1
|=0
|1=-1
1..2|1.0.2=1
1..2|1.1.2=1
1_0|1.0=0
1+0|1-0=0
1.0.0|1.0.0#=0
x|=1
1a2|1.a.2=0
<:TFF
lt:TFF
<=:TTF
le:TTF
>:FFT
gt:FFT
>=:FTT
ge:FTT
==:FTF
=:FTF
eq:FTF
!=:TFT
<>:TFT
ne:TFT
ValueError: version_compare(): Argument #3 ($operator) must be a valid comparison operator
ValueError: version_compare(): Argument #3 ($operator) must be a valid comparison operator
ValueError: version_compare(): Argument #3 ($operator) must be a valid comparison operator
ValueError: version_compare(): Argument #3 ($operator) must be a valid comparison operator
ValueError: version_compare(): Argument #3 ($operator) must be a valid comparison operator
ValueError: version_compare(): Argument #3 ($operator) must be a valid comparison operator
ValueError: version_compare(): Argument #3 ($operator) must be a valid comparison operator
ValueError: version_compare(): Argument #3 ($operator) must be a valid comparison operator
int(-1)
TypeError: version_compare(): Argument #1 ($version1) must be of type string, array given
TypeError: version_compare(): Argument #3 ($operator) must be of type ?string, array given
--CLEAN--
<?php
