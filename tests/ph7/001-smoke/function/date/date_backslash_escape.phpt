--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7: date backslash escape

--FILE--
<?php
// Test backslash escape - should output the character after backslash
$result = date("\\y");
echo $result . "\n";
?>
--EXPECT--
y
--CLEAN--
<?php
unset($result);
