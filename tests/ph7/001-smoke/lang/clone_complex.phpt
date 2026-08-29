--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Clone operator with complex expressions

--FILE--
<?php
class CloneCplxTest {
    public $value = 1;
}
$objs = array(new CloneCplxTest(), new CloneCplxTest());
$cloned = clone $objs[0];
echo $cloned->value . "\n";
?>
--EXPECT--
1
--CLEAN--
<?php
unset($objs, $cloned);
