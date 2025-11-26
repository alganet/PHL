--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: JSON error constants have expected numeric values
--FILE--
<?php
echo JSON_ERROR_NONE . "\n";
echo JSON_ERROR_DEPTH . "\n";
echo JSON_ERROR_STATE_MISMATCH . "\n";
echo JSON_ERROR_CTRL_CHAR . "\n";
echo JSON_ERROR_SYNTAX . "\n";
echo JSON_ERROR_UTF8 . "\n";
?>
--EXPECT--
0
1
2
3
4
5
