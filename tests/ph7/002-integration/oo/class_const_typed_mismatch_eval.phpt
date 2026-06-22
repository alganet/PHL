--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A typed-constant violation in an eval()-declared class still reports a fatal (not silent)
--FILE--
<?php
eval('class TypedConstEvalMismatch { const int X = "no"; }');
echo "unreached\n";
?>
--EXPECTF--
%ACannot use string as value for class constant TypedConstEvalMismatch::X of type int%A
--CLEAN--
<?php
