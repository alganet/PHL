--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
E_ALL is 30719 in php 8 — the removed E_STRICT bit (2048) is not part of it
--DESCRIPTION--
Only the CONSTANT is engine behavior and therefore cross-engine. The value
error_reporting() returns at startup is php.ini configuration, not an engine
property: a stock php often ships error_reporting = E_ALL & ~E_DEPRECATED
(22527), while PHL starts at E_ALL. Asserting the startup level here made this
test fail against the oracle for a config difference, which is not a fidelity
gap; error_reporting()'s round-trip behavior is asserted below instead.
--FILE--
<?php
echo E_ALL, "\n";
var_dump((E_ALL & 2048) === 0);   // E_STRICT was removed in php 8

// error_reporting() round-trips whatever it is set to, whatever the ini default.
$previous = error_reporting(E_ALL);
var_dump(error_reporting() === E_ALL);
error_reporting(E_ALL & ~E_WARNING);
var_dump((error_reporting() & E_WARNING) === 0);
error_reporting($previous);
var_dump(error_reporting() === $previous);
?>
--EXPECT--
30719
bool(true)
bool(true)
bool(true)
bool(true)
--CLEAN--
<?php
unset($previous);
