--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
preg_match with a by-ref subscript of a non-lvalue parent is a safe no-op (no crash)
--FILE--
<?php
function preg_match_nonlvalue_src() { return ['k' => 0]; }
// the call result is a temporary; the write-back has nowhere to land and is
// dropped. The important property is that this does not crash the interpreter.
preg_match('/(\w+)/', 'Hi', preg_match_nonlvalue_src()['k']);
echo "ok\n";
?>
--EXPECT--
ok
--CLEAN--
<?php
