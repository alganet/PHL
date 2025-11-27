--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: FILE_SKIP_EMPTY_LINES constant
--FILE--
<?php
echo "FILE_SKIP_EMPTY_LINES=" . FILE_SKIP_EMPTY_LINES . "\n";
?>
--EXPECTF--
FILE_SKIP_EMPTY_LINES=%d
