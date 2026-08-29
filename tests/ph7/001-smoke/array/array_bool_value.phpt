--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Array with boolean value

--FILE--
<?php
// Test array with boolean value
$a = array("key" => true);
echo "Array created\n";
?>
--EXPECT--
Array created
--CLEAN--
<?php
unset($a);
