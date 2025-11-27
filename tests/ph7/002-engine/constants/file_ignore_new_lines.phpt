--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: FILE_IGNORE_NEW_LINES constant
--FILE--
<?php
echo "FILE_IGNORE_NEW_LINES=" . FILE_IGNORE_NEW_LINES . "\n";
?>
--EXPECTF--
FILE_IGNORE_NEW_LINES=%d
