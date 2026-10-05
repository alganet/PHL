--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A conditional class that never runs is compiled but never linked
--DESCRIPTION--
php compiles a conditional class's body with the file but LINKS it -- to its
parent, its interfaces and its traits -- only when its statement runs. So a
polyfill branch whose signatures no longer fit what it implements, or that leaves
an inherited abstract method out, is no error while it is not reached. A branch
that IS reached is compiled once: its compile-time warning prints once, before
the first statement.
--INI--
error_reporting=E_ALL & ~E_DEPRECATED
display_errors=1
log_errors=0
--FILE--
<?php
echo "start\n";
if (false) {
    class CcbLinkA implements Countable { public function count(int $x): string { return ""; } }
    class CcbLinkB extends ArrayIterator { public function current(int $x): int { return 0; } }
    class CcbLinkC implements IteratorAggregate {}
    class CcbLinkD extends Exception { #[\Override] public function nothere() {} }
}
var_dump(class_exists('CcbLinkW', false));
if (true) {
    class CcbLinkW { public function f() { return "\400"; } }
}
var_dump(class_exists('CcbLinkW', false), strlen((new CcbLinkW)->f()));
echo "end\n";
?>
--EXPECTF--
Warning: Octal escape sequence overflow \400 is greater than \377 in %s on line 11
start
bool(false)
bool(true)
int(1)
end
