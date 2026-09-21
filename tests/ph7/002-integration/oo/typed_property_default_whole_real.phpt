--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Whole-real default into an int property: PHL accepts-and-materializes (dual-flag leniency)
--SKIPIF--
<?php
if (function_exists('zend_version')) {
    echo "skip PHL-pinned half of the twin pair";
}
?>
--FILE--
<?php
// php rejects a whole-float default for an int property, but PHL's
// dual-flagged whole-real also comes out of arithmetic/builtins that php
// returns as INT (pow(2,3)); PHL cannot tell them apart by flags, so the
// default rule accepts-and-materializes both — same leniency as
// strict_types params and typed constants. Leniency, recorded.
const TpwFloat = 2.0;
class TpwInt      { public int $p = TpwFloat; }
class TpwUnion    { public int|string $p = TpwFloat; }
var_dump((new TpwInt)->p);
var_dump((new TpwUnion)->p);
echo "done\n";
?>
--EXPECT--
int(2)
int(2)
done
--CLEAN--
<?php
