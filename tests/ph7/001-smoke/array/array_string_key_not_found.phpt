--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause

--TEST--
array access with non-existent string key
--FILE--
<?php
$a = array('key' => 'value');
echo $a['nonexistent'];
?>
--EXPECTF--
Error [2]: Undefined array key "nonexistent" in %s on line %d
--CLEAN--
<?php
unset($a);
