--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A failing output handler sends what it printed along with its input, and its exception surfaces after them
--FILE--
<?php
// php 8.4 deprecates a handler that prints; what it printed is the subject here.
error_reporting(E_ALL & ~E_DEPRECATED);
function fails($b) { echo "<printed>"; return false; }
function throws($b) { echo "<printed>"; throw new Exception("from handler"); }
function answers($b) { echo "<printed>"; return "[" . $b . "]"; }
function own_catch($b) { try { echo "<try>"; throw new Exception; } catch (Exception $e) { echo "<catch>"; } return false; }

echo "-- a FALSE answer sends the input and what the handler printed\n";
foreach (['ob_end_flush', 'ob_flush', 'ob_get_flush', 'ob_end_clean', 'ob_get_clean', 'ob_clean'] as $door) {
    ob_start('fails');
    echo "in";
    $r = $door();
    while (ob_get_level()) ob_end_flush();
    echo " <- $door answered ", var_export($r, true), "\n";
}

echo "-- a throw sends them too, and the catch runs after\n";
foreach (['ob_end_flush', 'ob_flush', 'ob_get_flush', 'ob_end_clean', 'ob_get_clean', 'ob_clean'] as $door) {
    try {
        ob_start('throws');
        echo "in";
        $door();
        echo "not reached";
    } catch (Exception $e) {
        echo " <- $door: caught '", $e->getMessage(), "' at level ", ob_get_level();
    }
    while (ob_get_level()) ob_end_flush();
    echo "\n";
}

echo "-- an answer still discards what the handler printed\n";
ob_start('answers');
echo "in";
ob_end_flush();
echo "\n";

echo "-- a handler that catches its own throw\n";
ob_start('own_catch');
echo "in";
ob_end_flush();
echo "\n";

echo "-- a chunked buffer filled by an echo\n";
ob_start('fails', 2);
echo "abc";
echo "d";
ob_end_flush();
echo "\n";
try {
    ob_start('throws', 2);
    echo "abc";
    echo "not reached";
} catch (Exception $e) {
    echo " <- caught at level ", ob_get_level();
}
while (ob_get_level()) ob_end_flush();
echo "\n";

echo "-- nested: the bytes land in the buffer below\n";
ob_start();
ob_start('fails');
echo "in";
ob_end_flush();
var_dump(ob_get_clean());
ob_start();
try {
    ob_start('throws');
    echo "in";
    ob_end_flush();
} catch (Exception $e) {
    echo "<caught>";
}
var_dump(ob_get_clean());

echo "-- the shutdown flush sends them as well\n";
ob_start('fails');
echo "at shutdown";
--EXPECT--
-- a FALSE answer sends the input and what the handler printed
in<printed> <- ob_end_flush answered true
in<printed> <- ob_flush answered true
in<printed> <- ob_get_flush answered 'in'
 <- ob_end_clean answered true
 <- ob_get_clean answered 'in'
 <- ob_clean answered true
-- a throw sends them too, and the catch runs after
in<printed> <- ob_end_flush: caught 'from handler' at level 0
in<printed> <- ob_flush: caught 'from handler' at level 1
in<printed> <- ob_get_flush: caught 'from handler' at level 0
 <- ob_end_clean: caught 'from handler' at level 0
 <- ob_get_clean: caught 'from handler' at level 0
 <- ob_clean: caught 'from handler' at level 1
-- an answer still discards what the handler printed
[in]
-- a handler that catches its own throw
in<try><catch>
-- a chunked buffer filled by an echo
abc<printed>d
abc<printed> <- caught at level 1
-- nested: the bytes land in the buffer below
string(11) "in<printed>"
string(19) "in<printed><caught>"
-- the shutdown flush sends them as well
at shutdown<printed>
