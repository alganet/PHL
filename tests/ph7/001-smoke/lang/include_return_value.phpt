--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: what include/require ANSWER
--DESCRIPTION--
A file that returns nothing of its own makes include/require answer INT 1, not
`true` — the value a script stores, compares and passes on, and
`include $f === 1` was false here where php makes it true. `include_once` on a
file already included is the one that answers a bool. And an explicit bare
`return;` in an included file is php's NULL, which is what tells "the file said
nothing" from "the file stopped early".
--FILE--
<?php
$incrDir = sys_get_temp_dir();
$incrMake = function ($name, $body) use ($incrDir) {
    $p = $incrDir . DIRECTORY_SEPARATOR . $name;
    file_put_contents($p, $body);
    return $p;
};
$incrValue = $incrMake('incr_value.php', '<?php return 5;');
$incrBare  = $incrMake('incr_bare.php', '<?php return;');
$incrNone  = $incrMake('incr_none.php', '<?php $incrSideEffect = 1;');
$incrFalse = $incrMake('incr_false.php', '<?php return false;');

var_dump(include $incrValue);
var_dump(include $incrBare);
var_dump(include $incrNone);
var_dump(include $incrFalse);
var_dump(require $incrNone);
var_dump(include_once $incrNone);
var_dump(include_once $incrNone);
var_dump(require_once $incrValue);
var_dump(require_once $incrValue);

/* eval() answers the chunk's value and NULL when there is none — the same
 * distinction from the other side. */
var_dump(eval('return 3;'), eval('$incrSideEffect = 2;'), eval('return;'));

/* A function's bare return is unchanged. */
function incrBareFn() { return; }
function incrNullFn(): ?int { return null; }
function incrCaught() { try { throw new Exception('x'); } catch (Exception $e) { return; } }
var_dump(incrBareFn(), incrNullFn(), incrCaught());

foreach ([$incrValue, $incrBare, $incrNone, $incrFalse] as $incrPath) { @unlink($incrPath); }
?>
--EXPECT--
int(5)
NULL
int(1)
bool(false)
int(1)
bool(true)
bool(true)
bool(true)
bool(true)
int(3)
NULL
NULL
NULL
NULL
NULL
