--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
php evaluates a `const NAME = <expr>;` initializer AT THE STATEMENT (zend half of the twin pair — PHL evaluates at the first read, see const_initializer_lazy.phpt)
--SKIPIF--
<?php
if (!function_exists('zend_version')) {
    echo "skip";
}
?>
--FILE--
<?php
// The statement itself is where the work happens: the constructor has run
// before the next line, and the constant is a plain value from then on.
class CilzMaker { public static int $made = 0; public function __construct() { self::$made++; } }
var_dump(CilzMaker::$made);
const CILZ_OBJ = new CilzMaker();
var_dump(CilzMaker::$made);   // 1 — and nothing has READ the constant yet
var_dump(CILZ_OBJ === CILZ_OBJ, CilzMaker::$made);
// An initializer that cannot complete is a fatal AT THE STATEMENT, so no
// try/catch around a later read can see it and no retry exists.
const CILZ_LATE = CILZ_NEVER_DEFINED;
echo "NEVER\n";
?>
--EXPECTF--
int(0)
int(1)
bool(true)
int(1)
%AUncaught Error: Undefined constant "CILZ_NEVER_DEFINED"%A
--CLEAN--
<?php
