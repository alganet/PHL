--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
zend.exception_ignore_args and zend.exception_string_param_max_len shape a trace's arguments
--DESCRIPTION--
With zend.exception_ignore_args Off an exception's trace keeps each frame's
arguments, and getTraceAsString() prints them php's way: a string escaped and cut
after zend.exception_string_param_max_len bytes, a float with its fraction, an enum
case by name. An unhandled match case names its value the same way. Both directives
are PHP_INI_ALL, and the first is read when the exception is made.
--INI--
zend.exception_ignore_args=0
zend.exception_string_param_max_len=15
--FILE--
<?php
// zend.exception_ignore_args=0 keeps each frame's arguments, and
// zend.exception_string_param_max_len decides how much of a string one shows.
enum Suit { case Hearts; }
class Box {}
function thrower(...$args) { throw new Exception('x'); }
function show(Throwable $e) {
    echo str_replace(__FILE__, 'FILE', $e->getTraceAsString()), "\n";
}
function call() {
    thrower(1, 2.0, -0.5, 0.1 + 0.2, 1e100, INF, true, false, null, '',
        "a\n\t\\\x1b\x01\xff'z", str_repeat('y', 20), [1], new Box, Suit::Hearts);
}
foreach (['15', '0', '3'] as $len) {
    ini_set('zend.exception_string_param_max_len', $len);
    try { call(); } catch (Exception $e) {
        var_dump(count($e->getTrace()[0]['args']));
        show($e);
    }
}
// php's range is 0..1000000; outside it the write is refused, and a word is
// taken and reads as 0.
var_dump(ini_set('zend.exception_string_param_max_len', '-1'));
var_dump(ini_set('zend.exception_string_param_max_len', '1000001'));
var_dump(ini_set('zend.exception_string_param_max_len', '1000000'));
var_dump(ini_set('zend.exception_string_param_max_len', 'many'));
var_dump(ini_get('zend.exception_string_param_max_len'));
try { call(); } catch (Exception $e) { show($e); }
ini_set('zend.exception_string_param_max_len', '4');

// An unhandled match case names the value the way a trace argument prints it.
foreach ([5, 'abcdefgh', '', 1.5, true, null, [1], new Box, Suit::Hearts] as $v) {
    try { echo match ($v) { 999 => 1 }; }
    catch (UnhandledMatchError $e) { echo $e->getMessage(), "\n"; }
}

// The directive is read when the exception is MADE: switching it back on
// leaves an exception already made with its arguments, and a new one without.
try { thrower(7); } catch (Exception $kept) {}
var_dump(ini_set('zend.exception_ignore_args', '1'));
try { thrower(8); } catch (Exception $e) { show($e); var_dump(isset($e->getTrace()[0]['args'])); }
show($kept);
try { echo match (5) { 999 => 1 }; } catch (UnhandledMatchError $e) { echo $e->getMessage(), "\n"; }
try { echo match (new Box) { 999 => 1 }; } catch (UnhandledMatchError $e) { echo $e->getMessage(), "\n"; }
?>
--EXPECT--
int(15)
#0 FILE(11): thrower(1, 2.0, -0.5, 0.3, 1.0E+100, INF, true, false, NULL, '', 'a\n\t\\\e\x01\xFF'z', 'yyyyyyyyyyyyyyy...', Array, Object(Box), Suit::Hearts)
#1 FILE(16): call()
#2 {main}
int(15)
#0 FILE(11): thrower(1, 2.0, -0.5, 0.3, 1.0E+100, INF, true, false, NULL, '', '...', '...', Array, Object(Box), Suit::Hearts)
#1 FILE(16): call()
#2 {main}
int(15)
#0 FILE(11): thrower(1, 2.0, -0.5, 0.3, 1.0E+100, INF, true, false, NULL, '', 'a\n\t...', 'yyy...', Array, Object(Box), Suit::Hearts)
#1 FILE(16): call()
#2 {main}
bool(false)
bool(false)
string(1) "3"
string(7) "1000000"
string(4) "many"
#0 FILE(11): thrower(1, 2.0, -0.5, 0.3, 1.0E+100, INF, true, false, NULL, '', '...', '...', Array, Object(Box), Suit::Hearts)
#1 FILE(28): call()
#2 {main}
Unhandled match case 5
Unhandled match case 'abcd...'
Unhandled match case ''
Unhandled match case 1.5
Unhandled match case true
Unhandled match case NULL
Unhandled match case of type array
Unhandled match case of type Box
Unhandled match case Suit::Hearts
string(1) "0"
#0 FILE(41): thrower()
#1 {main}
bool(false)
#0 FILE(39): thrower(7)
#1 {main}
Unhandled match case of type int
Unhandled match case of type Box
