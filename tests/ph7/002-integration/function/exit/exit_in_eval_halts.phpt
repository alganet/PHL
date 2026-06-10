--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
die() inside eval() halts the whole script
--DESCRIPTION--
Regression: VmEvalChunk swallowed an abort from the evaluated chunk, so execution
continued past eval() after a die(). The halt now cascades out of eval().
--FILE--
<?php
echo "before\n";
eval('die("died\n");');
echo "after\n";
?>
--EXPECT--
before
died
--CLEAN--
<?php
?>
