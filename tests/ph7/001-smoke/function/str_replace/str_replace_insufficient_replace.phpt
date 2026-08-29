--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7: str_replace with insufficient replace array elements

--FILE--
<?php
$result = str_replace(array('a', 'b'), array('x'), 'ab');
echo $result . "\n";
?>
--EXPECT--
x
--CLEAN--
<?php
unset($result);
