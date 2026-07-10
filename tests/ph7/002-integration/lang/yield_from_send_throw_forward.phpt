--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Generator: yield from forwards send() and throw() into the delegated generator (PHP transparency)
--SKIPIF--
<?php if (function_exists('zend_version') && version_compare(PHP_VERSION, '7.0.0', '<')) echo 'skip Requires PHP 7.0+'; ?>
--FILE--
<?php
// send() reaches the delegate's own yield expression, not the outer generator.
function innerSend(){ $x = yield 1; echo "inner received: ", var_export($x, true), "\n"; yield 2; }
function outerSend(){ yield from innerSend(); }
$g = outerSend();
echo "start: ", $g->current(), "\n";
echo "send ret: ", $g->send("HELLO"), "\n";
echo "cur: ", $g->current(), "\n";

// throw() is injected at the delegate's suspended yield; the delegate's own catch handles it.
function innerThrow(){ try { yield 1; } catch (Exception $e) { echo "inner caught: ", $e->getMessage(), "\n"; yield 99; } }
function outerThrow(){ yield from innerThrow(); }
$h = outerThrow();
$h->current();
echo "throw ret: ", $h->throw(new Exception("boom")), "\n";
echo "cur: ", $h->current(), "\n";

// Nested delegation top -> mid -> leaf: send/throw thread to the innermost, return values bubble up.
function leaf(){ $a = yield 1; echo "leaf got: $a\n"; $b = yield 2; echo "leaf got: $b\n"; return "leaf-ret"; }
function mid(){ $r = yield from leaf(); echo "mid saw return: $r\n"; yield 3; }
function top(){ yield from mid(); }
$t = top();
echo "cur=", $t->current(), "\n";
echo "send1=", $t->send("A"), "\n";
echo "send2=", $t->send("B"), "\n";
echo "cur=", $t->current(), "\n";

// An uncaught throw in the delegate propagates through the yield from to the throw() caller.
function innerBare(){ yield 1; yield 2; }
function outerBare(){ yield from innerBare(); echo "NOT REACHED\n"; }
$u = outerBare();
$u->current();
try { $u->throw(new RuntimeException("kaboom")); }
catch (RuntimeException $e) { echo "caught at top: ", $e->getMessage(), "\n"; }

// throw() at a non-Generator (array) delegate is raised at the outer yield-from,
// not forwarded; if the outer catches it, a LATER `yield from` must classify its
// operand fresh instead of resuming the abandoned array delegate cursor.
function staleArr(){
    try { yield from [1,2,3]; }
    catch (Exception $e) { echo "arr caught\n"; yield 'x'; }
    yield from ['a','b'];
    echo "arr end\n";
}
$s = staleArr();
echo "cur=", $s->current(), "\n";
echo "throw=", $s->throw(new Exception()), "\n";
$s->next(); echo "after=", $s->current(), "\n";
$s->next(); echo "after=", $s->current(), "\n";
?>
--EXPECT--
start: 1
send ret: inner received: 'HELLO'
2
cur: 2
throw ret: inner caught: boom
99
cur: 99
cur=1
send1=leaf got: A
2
send2=leaf got: B
mid saw return: leaf-ret
3
cur=3
caught at top: kaboom
cur=1
throw=arr caught
x
after=a
after=b
--CLEAN--
<?php
