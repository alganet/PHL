--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: FILE_APPEND constant
--FILE--
<?php
echo "FILE_APPEND=" . FILE_APPEND . "\n";
?>
--EXPECTF--
FILE_APPEND=%d
