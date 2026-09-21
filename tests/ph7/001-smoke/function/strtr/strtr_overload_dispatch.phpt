--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
strtr() dispatches on arity: $from is `array` with 2 args, `string` with 3
--FILE--
<?php
// With two arguments there is no $to, so $from must be the translation table.
// PH7 accepted anything and returned the SUBJECT UNCHANGED -- the caller got
// its input back as if it had been translated.
foreach ([["ab"], [1], [true], [1.5], [null], [new stdClass]] as $case) {
    try {
        var_dump(strtr("abc", $case[0]));
    } catch (TypeError $e) {
        echo $e->getMessage(), "\n";
    }
}
// The check precedes the empty-subject shortcut.
try {
    strtr("", "ab");
} catch (TypeError $e) {
    echo $e->getMessage(), "\n";
}

// With three arguments the table form is the error, and it was silent too.
try {
    strtr("abc", [], "x");
} catch (TypeError $e) {
    echo $e->getMessage(), "\n";
}
try {
    strtr("abc", ["a" => "x"], null);
} catch (TypeError $e) {
    echo $e->getMessage(), "\n";
}
// $from is reported before $to.
try {
    strtr("abc", [], []);
} catch (TypeError $e) {
    echo $e->getMessage(), "\n";
}
try {
    strtr("abc", "a", []);
} catch (TypeError $e) {
    echo $e->getMessage(), "\n";
}
try {
    strtr(["a"], "a", "b");
} catch (TypeError $e) {
    echo $e->getMessage(), "\n";
}

// Both real overloads keep working, and a __toString() object still coerces.
class StrtrStringable { public function __toString(): string { return "a"; } }
var_dump(strtr("abc", ["a" => "x"]));
var_dump(strtr("abc", []));
var_dump(strtr("", []));
var_dump(strtr("hello world", "lo", "01"));
// (a null $to is NOT asserted here: php answers the subject unchanged like PHL
// but reaches it through an E_DEPRECATED, which the runner would print.)
var_dump(strtr("abc", new StrtrStringable, "z"));
var_dump(strtr("abc", "a", new StrtrStringable));
?>
--EXPECT--
strtr(): Argument #2 ($from) must be of type array, string given
strtr(): Argument #2 ($from) must be of type array, int given
strtr(): Argument #2 ($from) must be of type array, true given
strtr(): Argument #2 ($from) must be of type array, float given
strtr(): Argument #2 ($from) must be of type array, null given
strtr(): Argument #2 ($from) must be of type array, stdClass given
strtr(): Argument #2 ($from) must be of type array, string given
strtr(): Argument #2 ($from) must be of type string, array given
strtr(): Argument #2 ($from) must be of type string, array given
strtr(): Argument #2 ($from) must be of type string, array given
strtr(): Argument #3 ($to) must be of type string, array given
strtr(): Argument #1 ($string) must be of type string, array given
string(3) "xbc"
string(3) "abc"
string(0) ""
string(11) "he001 w1r0d"
string(3) "zbc"
string(3) "abc"
--CLEAN--
<?php
