--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: dirname returns '.' for simple filename
--FILE--
<?php
// dirname of a simple filename should be '.'
echo dirname('file.txt') . "\n";
?>
--EXPECT--
.
