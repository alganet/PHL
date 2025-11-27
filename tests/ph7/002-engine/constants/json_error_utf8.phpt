--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: JSON_ERROR_UTF8 constant
--FILE--
<?php
echo "JSON_ERROR_UTF8=" . JSON_ERROR_UTF8 . "\n";
?>
--EXPECTF--
JSON_ERROR_UTF8=%d
