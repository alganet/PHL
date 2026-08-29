--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Array with numeric key

--FILE--
<?php
// Test array with numeric key
$a = array(1 => "value");
echo "Array created\n";
?>
--EXPECT--
Array created
--CLEAN--
<?php
unset($a);
