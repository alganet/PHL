--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Namespace separator handling in literals - edge case
--FILE--
<?php
// Test namespace separator in different contexts
echo "Test\\namespace\\constant";
echo PHP_EOL;
?>
--EXPECTF--
Test\namespace\constant