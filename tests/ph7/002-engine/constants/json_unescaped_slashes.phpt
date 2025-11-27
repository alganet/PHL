--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: JSON_UNESCAPED_SLASHES constant value
--FILE--
<?php
echo "JSON_UNESCAPED_SLASHES=" . JSON_UNESCAPED_SLASHES . "\n";
?>
--EXPECTF--
JSON_UNESCAPED_SLASHES=%d
