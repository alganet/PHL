--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
An error handler that throws on an argument's read stops the call there
--FILE--
<?php
/* An error handler that throws on an argument's read stops the call at that
 * read: no later argument is read or run, no later step of a subscript chain
 * warns, and the callee is never entered. */
set_error_handler(function ($no, $msg) { echo "  handler: $msg\n"; throw new ErrorException($msg); });
function ath_s() { echo "  ran s\n"; return 1; }
function ath_f($a, $b) { echo "  entered\n"; return 1; }
function ath_t($label, $c) {
    echo "$label\n";
    try { $c(); } catch (ErrorException $e) { echo "  caught: ", $e->getMessage(), "\n"; }
}
$a = [];
ath_t('two variables', function () { ath_f($u, $w); });
ath_t('variable before a runner', function () { ath_f($u, ath_s()); });
ath_t('undefined base of a subscript', function () { ath_f($arr['q'], 1); });
ath_t('nested missing keys', function () use ($a) { ath_f($a['x']['y'], 1); });
ath_t('nested missing keys before a runner', function () use ($a) { ath_f($a['x']['y'], ath_s()); });
ath_t('undefined base of a property', function () { ath_f($o->p, 1); });
ath_t('inside an array literal', function () { $r = [ath_f($u, ath_s()), 2]; });
ath_t('builtin', function () { new ArrayObject($u, ath_s()); });
function ath_gen() { yield ath_f($u, ath_s()); }
ath_t('generator', function () { foreach (ath_gen() as $v) {} });
function ath_inner() {
    try { return ath_f($u, ath_s()); }
    catch (ErrorException $e) { echo "  caught inside: ", $e->getMessage(), "\n"; return -1; }
}
echo "caught in the same frame\n";
var_dump(ath_inner());
--EXPECT--
two variables
  handler: Undefined variable $u
  caught: Undefined variable $u
variable before a runner
  handler: Undefined variable $u
  caught: Undefined variable $u
undefined base of a subscript
  handler: Undefined variable $arr
  caught: Undefined variable $arr
nested missing keys
  handler: Undefined array key "x"
  caught: Undefined array key "x"
nested missing keys before a runner
  handler: Undefined array key "x"
  caught: Undefined array key "x"
undefined base of a property
  handler: Undefined variable $o
  caught: Undefined variable $o
inside an array literal
  handler: Undefined variable $u
  caught: Undefined variable $u
builtin
  handler: Undefined variable $u
  caught: Undefined variable $u
generator
  handler: Undefined variable $u
  caught: Undefined variable $u
caught in the same frame
  handler: Undefined variable $u
  caught inside: Undefined variable $u
int(-1)
