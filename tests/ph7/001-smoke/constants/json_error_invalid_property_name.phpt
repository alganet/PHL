--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: JSON_ERROR_INVALID_PROPERTY_NAME constant
--FILE--
<?php
echo "JSON_ERROR_INVALID_PROPERTY_NAME=" . JSON_ERROR_INVALID_PROPERTY_NAME . "\n";
?>
--EXPECT--
JSON_ERROR_INVALID_PROPERTY_NAME=9
--CLEAN--
<?php
