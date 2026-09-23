--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: JSON_ERROR_RECURSION constant
--FILE--
<?php
echo "JSON_ERROR_RECURSION=" . JSON_ERROR_RECURSION . "\n";
?>
--EXPECT--
JSON_ERROR_RECURSION=6
--CLEAN--
<?php
