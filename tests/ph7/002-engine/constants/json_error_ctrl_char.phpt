--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: JSON_ERROR_CTRL_CHAR constant
--FILE--
<?php
echo "JSON_ERROR_CTRL_CHAR=" . JSON_ERROR_CTRL_CHAR . "\n";
?>
--EXPECTF--
JSON_ERROR_CTRL_CHAR=%d
