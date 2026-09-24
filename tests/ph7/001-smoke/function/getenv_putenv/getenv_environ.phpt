--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
getenv() with no name answers the WHOLE environment as a map, putenv() without '=' removes a variable, and an empty assignment is a ValueError
--FILE--
<?php
putenv('PHL_GE_A=1');
putenv('PHL_GE_C=a=b');

// No name at all -- the documented way to read the whole environment. It
// answered FALSE, so `foreach (getenv() as $k => $v)` iterated over a bool.
$gev_all = getenv();
var_dump(is_array($gev_all), count($gev_all) > 0);
var_dump($gev_all['PHL_GE_A'], $gev_all['PHL_GE_C']);
var_dump(isset($gev_all['PATH']) || isset($gev_all['Path']));

// NULL is the same request spelled out, and $local_only selects the same
// environment on the CLI -- but it has to be accepted rather than turn the map
// into false.
var_dump(getenv(null) == $gev_all, getenv(null, true) == $gev_all);
var_dump(getenv('PHL_GE_A', true), getenv('PHL_GE_A', false));
var_dump(getenv('PHL_GE_NOPE'), getenv('PHL_GE_NOPE', true), getenv(''));

// The value keeps everything after the FIRST '=', and the name is taken
// verbatim -- spaces included.
putenv('PHL_GE_D=  spaced  ');
var_dump(getenv('PHL_GE_D'));
putenv('  PHL_GE_E  =v');
var_dump(getenv('  PHL_GE_E  '), getenv('PHL_GE_E'));

// An assignment with no '=' REMOVES the variable -- php's documented unset, and
// the answer is TRUE for doing it.
var_dump(getenv('PHL_GE_A'), putenv('PHL_GE_A'), getenv('PHL_GE_A'));
$gev_all = getenv();
var_dump(array_key_exists('PHL_GE_A', $gev_all));

// An empty assignment, or one with no name in front of the '=', is a ValueError.
foreach (['', '=x', '='] as $gev_bad) {
    try { putenv($gev_bad); } catch (Throwable $gev_e) { echo get_class($gev_e), ': ', $gev_e->getMessage(), "\n"; }
}

// An empty VALUE is a value, not a missing variable -- on Windows too, where
// GetEnvironmentVariable answers 0 for "absent" and for "empty" alike and only
// the error code it leaves tells them apart.
var_dump(putenv('PHL_GE_F='));
var_dump(getenv('PHL_GE_F'));

// The assignment is coerced like any declared `string` parameter: an int, a
// float, a bool or a __toString() object all reach the parser.
class GevS { public function __toString(): string { return 'PHL_GE_S=objval'; } }
var_dump(putenv(123), putenv(1.5), putenv(true), putenv(new GevS), getenv('PHL_GE_S'));
try { putenv(false); } catch (Throwable $gev_e) { echo get_class($gev_e), ': ', $gev_e->getMessage(), "\n"; }

// php looks for the '=' with strchr(), so an embedded NUL ENDS the search: the
// assignment below finds no '=' at all and REMOVES the variable named "FO"
// rather than setting one.
putenv('FO=keepme');
var_dump(putenv("FO\0O=BAR"), getenv('FO'), getenv("FO\0O"));

// putenv() does NOT touch $_ENV: that array is the startup snapshot, and a
// variable set afterwards is visible through getenv() alone.
var_dump(isset($_ENV['PHL_GE_C']));

putenv('PHL_GE_C');
putenv('PHL_GE_D');
putenv('  PHL_GE_E  ');
putenv('PHL_GE_F');
putenv('PHL_GE_S');
putenv('123');
putenv('1');
?>
--EXPECT--
bool(true)
bool(true)
string(1) "1"
string(3) "a=b"
bool(true)
bool(true)
bool(true)
string(1) "1"
string(1) "1"
bool(false)
bool(false)
bool(false)
string(10) "  spaced  "
string(1) "v"
bool(false)
string(1) "1"
bool(true)
bool(false)
bool(false)
ValueError: putenv(): Argument #1 ($assignment) must have a valid syntax
ValueError: putenv(): Argument #1 ($assignment) must have a valid syntax
ValueError: putenv(): Argument #1 ($assignment) must have a valid syntax
bool(true)
string(0) ""
bool(true)
bool(true)
bool(true)
bool(true)
string(6) "objval"
ValueError: putenv(): Argument #1 ($assignment) must have a valid syntax
bool(true)
bool(false)
bool(false)
bool(false)
