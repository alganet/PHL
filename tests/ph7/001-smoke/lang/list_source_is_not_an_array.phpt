--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A destructure asks its source once per POSITION it means to fill
--DESCRIPTION--
`[$a, $b] = 5` warns `Cannot use int as array` TWICE in php -- once for each
position it means to fill -- where PHL raised one warning for the whole list. An
empty slot fills nothing and is not counted; a nested level counts as the one
position it occupies, and the NULL it is then handed destructures in silence.
`null` is the one source php says nothing about at all.

A list with no positions at all (`[, ,] = $src`) is php's `Cannot use empty list`,
which PHL used to run as a destructure with no targets.

Beside them the KEYED spelling's missing key, which php warns about exactly as the
positional one does and PHL passed over in silence.
--FILE--
<?php
function listSrcShow($label, $fn)
{
    echo "-- $label\n";
    set_error_handler(function ($n, $m) { echo "   W: $m\n"; return true; });
    try { $fn(); } catch (Throwable $e) { printf("   T %s: %s\n", get_class($e), $e->getMessage()); }
    restore_error_handler();
}
listSrcShow('one target', function () { [$a] = 5; var_dump($a); });
listSrcShow('two targets', function () { [$a, $b] = 5; var_dump($b); });
listSrcShow('three targets', function () { [$a, $b, $c] = 5; var_dump($c); });
listSrcShow('empty slot between', function () { [$a, , $b] = 5; var_dump($b); });
listSrcShow('nested level', function () { [$a, [$b]] = 5; var_dump($b); });
listSrcShow('string source', function () { list($a, $b) = "st"; var_dump($a); });
listSrcShow('null source', function () { [$a, $b] = null; var_dump($a); });
listSrcShow('object source', function () { [$a, $b] = new stdClass(); var_dump($a); });
listSrcShow('keyed, missing key', function () { $s = ['k' => 1]; ['missing' => $v] = $s; var_dump($v); });
listSrcShow('positional, missing key', function () { $s = [1]; [$a, $b] = $s; var_dump($b); });
?>
--EXPECT--
-- one target
   W: Cannot use int as array
NULL
-- two targets
   W: Cannot use int as array
   W: Cannot use int as array
NULL
-- three targets
   W: Cannot use int as array
   W: Cannot use int as array
   W: Cannot use int as array
NULL
-- empty slot between
   W: Cannot use int as array
   W: Cannot use int as array
NULL
-- nested level
   W: Cannot use int as array
   W: Cannot use int as array
NULL
-- string source
   W: Cannot use string as array
   W: Cannot use string as array
NULL
-- null source
NULL
-- object source
   T Error: Cannot use object of type stdClass as array
-- keyed, missing key
   W: Undefined array key "missing"
NULL
-- positional, missing key
   W: Undefined array key 1
NULL
