--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause

--TEST--
array access with non-existent int key
--FILE--
<?php
$a = array();
echo $a[0];
?>
--EXPECTF--
Error [2]: Undefined array key 0 in %s on line %d
--CLEAN--
<?php
unset($a);
