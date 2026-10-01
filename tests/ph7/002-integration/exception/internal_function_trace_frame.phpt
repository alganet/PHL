--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
An internal function or method leaves its own frame in a Throwable's trace
--DESCRIPTION--
php gives every internal call an execute_data of its own, so a throw raised
inside a C body names that body as frame #0 -- `#0 file(line): str_repeat()`,
and `#0 file(line): SplFileObject->__construct()` for a method, whose `class`
key is the DECLARING class and whose `type` is `::` for a static one. The
engine pushed no frame for a native call, so every such trace started at the
CALLER and a one-deep one printed `#0 {main}`.

A call made from inside ANOTHER internal function has no source position at
all, and php omits both keys rather than inventing one -- that is the
`#0 [internal function]: str_repeat()` under `array_map('str_repeat', ...)`.

php's compiler rewrites a handful of names into dedicated opcodes, and a
refusal raised by an opcode has no internal frame to name: `strlen($a)` on an
array reports its CALLER as frame #0 where `str_repeat($a, 2)` reports
str_repeat. The rewrite needs a literal name at the exact arity, so a `name:`
argument, a spread and a dynamic name all take the ordinary path and do carry
the frame. debug_backtrace() leaves its own internal frame out.
--FILE--
<?php
/* php gives every INTERNAL call an execute_data of its own, so a throw raised
 * inside a C body names that body as frame #0. */
function show(Throwable $e) {
    foreach ($e->getTrace() as $i => $fr) {
        echo "#$i ", implode(',', array_keys($fr)), " | ",
             isset($fr['file']) ? ($fr['file'] === __FILE__ ? 'FILE' : $fr['file']) : '[internal function]',
             isset($fr['line']) ? '(' . $fr['line'] . ')' : '', ': ',
             $fr['class'] ?? '', $fr['type'] ?? '', $fr['function'], "\n";
    }
    echo "~~\n";
}
/* A builtin's own body. */
function f1() { str_repeat('a', -1); }
try { f1(); } catch (Throwable $e) { show($e); }
/* Too few arguments, and too many: both refusals are php's, from inside. */
function f2() { str_repeat('a'); }
try { f2(); } catch (Throwable $e) { show($e); }
function f3() { str_repeat('a', 1, 1); }
try { f3(); } catch (Throwable $e) { show($e); }
/* An argument php refuses on type. */
function f4() { str_repeat([], 2); }
try { f4(); } catch (Throwable $e) { show($e); }
/* A native constructor, and a native method: the frame names the class too. */
function f5() { new SplFileObject(''); }
try { f5(); } catch (Throwable $e) { show($e); }
function f6() { (new DateTime())->modify(); }
try { f6(); } catch (Throwable $e) { show($e); }
/* An INHERITED native method reports its DECLARING class, not the receiver's. */
function f7() { (new SplTempFileObject())->setMaxLineLen(-1); }
try { f7(); } catch (Throwable $e) { show($e); }
/* A native STATIC method: php's separator is `::`. */
function f8() { DateTime::createFromFormat(); }
try { f8(); } catch (Throwable $e) { show($e); }
/* A builtin reached from INSIDE another builtin has no source position at all. */
function f9() { array_map('str_repeat', ['a'], [-1]); }
try { f9(); } catch (Throwable $e) { show($e); }
/* ...but one reached through a userland callback does. */
function f10() { array_map(fn($x) => str_repeat('a', -1), [1]); }
try { f10(); } catch (Throwable $e) { show($e); }
/* php's compiler rewrites a few names into opcodes of their own, and an opcode
 * has no internal frame to name. */
function f11() { $a = []; strlen($a); }
try { f11(); } catch (Throwable $e) { show($e); }
function f12() { $i = 1; count($i); }
try { f12(); } catch (Throwable $e) { show($e); }
function f13() { $i = 1; array_key_exists(1, $i); }
try { f13(); } catch (Throwable $e) { show($e); }
/* The rewrite needs the literal name at the exact arity: a `name:` argument, a
 * spread and a dynamic name all take the ordinary path and DO carry the frame. */
function f14() { $a = []; strlen(string: $a); }
try { f14(); } catch (Throwable $e) { show($e); }
function f15() { $a = [[]]; strlen(...$a); }
try { f15(); } catch (Throwable $e) { show($e); }
function f16() { $a = []; $n = 'strlen'; $n($a); }
try { f16(); } catch (Throwable $e) { show($e); }
function f17() { $i = 1; count($i, COUNT_RECURSIVE); }
try { f17(); } catch (Throwable $e) { show($e); }
/* debug_backtrace() leaves its OWN internal frame out. */
function f18() { foreach (debug_backtrace() as $i => $fr) {
    echo "#$i ", $fr['function'], "\n"; } echo "~~\n"; }
f18();
/* getTraceAsString() renders the fileless internal frame php's way. */
try { array_map('str_repeat', ['a'], [-1]); } catch (Throwable $e) {
    echo str_replace(__FILE__, 'FILE', $e->getTraceAsString()), "\n";
}
/* A php LANGUAGE CONSTRUCT dispatched as an internal function is not a call in
 * php at all: eval() and its include/require neighbours carry a frame of their
 * OWN shape, built from the include stack, and never a second one. */
$ev = eval('return new Exception();');
foreach ($ev->getTrace() as $i => $fr) {
    echo "#$i ", implode(',', array_keys($fr)), ": ", $fr['function'], "\n";
}
--EXPECT--
#0 file,line,function | FILE(14): str_repeat
#1 file,line,function | FILE(15): f1
~~
#0 file,line,function | FILE(17): str_repeat
#1 file,line,function | FILE(18): f2
~~
#0 file,line,function | FILE(19): str_repeat
#1 file,line,function | FILE(20): f3
~~
#0 file,line,function | FILE(22): str_repeat
#1 file,line,function | FILE(23): f4
~~
#0 file,line,function,class,type | FILE(25): SplFileObject->__construct
#1 file,line,function | FILE(26): f5
~~
#0 file,line,function,class,type | FILE(27): DateTime->modify
#1 file,line,function | FILE(28): f6
~~
#0 file,line,function,class,type | FILE(30): SplFileObject->setMaxLineLen
#1 file,line,function | FILE(31): f7
~~
#0 file,line,function,class,type | FILE(33): DateTime::createFromFormat
#1 file,line,function | FILE(34): f8
~~
#0 function | [internal function]: str_repeat
#1 file,line,function | FILE(36): array_map
#2 file,line,function | FILE(37): f9
~~
#0 file,line,function | FILE(39): str_repeat
#1 function | [internal function]: {closure:f10():39}
#2 file,line,function | FILE(39): array_map
#3 file,line,function | FILE(40): f10
~~
#0 file,line,function | FILE(44): f11
~~
#0 file,line,function | FILE(46): f12
~~
#0 file,line,function | FILE(48): f13
~~
#0 file,line,function | FILE(51): strlen
#1 file,line,function | FILE(52): f14
~~
#0 file,line,function | FILE(53): strlen
#1 file,line,function | FILE(54): f15
~~
#0 file,line,function | FILE(55): strlen
#1 file,line,function | FILE(56): f16
~~
#0 file,line,function | FILE(57): count
#1 file,line,function | FILE(58): f17
~~
#0 f18
~~
#0 [internal function]: str_repeat()
#1 FILE(64): array_map()
#2 {main}
#0 file,line,function: eval
