--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: FILE_USE_INCLUDE_PATH constant
--FILE--
<?php
echo "FILE_USE_INCLUDE_PATH=" . FILE_USE_INCLUDE_PATH . "\n";
?>
--EXPECTF--
FILE_USE_INCLUDE_PATH=%d
