--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: JSON_ERROR_UNSUPPORTED_TYPE constant
--FILE--
<?php
echo "JSON_ERROR_UNSUPPORTED_TYPE=" . JSON_ERROR_UNSUPPORTED_TYPE . "\n";
?>
--EXPECT--
JSON_ERROR_UNSUPPORTED_TYPE=8
--CLEAN--
<?php
