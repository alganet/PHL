--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A language construct in a ternary arm ends at the colon that is not its own
--DESCRIPTION--
`include`, `require`, their `_once` forms, `print`, `throw` and `yield` all sit BELOW the
ternary in php's precedence table, so a `:` closing a `?` opened outside the construct ends
its operand: `c ? include $f : null` includes $f. The operand collector swallowed the colon
regardless, leaving a `?` with no `:`, and every `cond ? require $file : null` bootstrap was
a syntax error. A `?` opened INSIDE the operand still takes its own colon with it.
--FILE--
<?php
$talcFile = __DIR__ . '/ternary_arm_language_construct.inc';
file_put_contents($talcFile, "<?php return 41;\n");
var_dump(true ? include $talcFile : null);
var_dump(false ? include $talcFile : 7);
var_dump(true ? require $talcFile : null);
var_dump(true ? include_once $talcFile : null);
var_dump(false ?: include $talcFile);
var_dump(null ?? include $talcFile);
var_dump(true ? (include $talcFile) : null);
$talcR = true ? print("p\n") : null;
var_dump($talcR);
try { $talcT = true ? throw new RuntimeException('t') : null; }
catch (RuntimeException $e) { echo 'caught ', $e->getMessage(), "\n"; }
function talcGen() { $g = true ? yield 5 : null; }
var_dump(talcGen()->current());
function talcGen2() { yield 5 ? 1 : 2; }
var_dump(talcGen2()->current());
unlink($talcFile);
?>
--EXPECT--
int(41)
int(7)
int(41)
bool(true)
int(41)
int(41)
int(41)
p
int(1)
caught t
int(5)
int(1)
--CLEAN--
<?php
