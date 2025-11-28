--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Magic method __clone
--FILE--
<?php
class FooClone {
    public function __construct(){ echo "constructed\n"; }
    public function __clone(){ echo "cloned\n"; }
}
$orig = new FooClone();
$c = clone $orig;
?>
--EXPECT--
constructed
cloned

--CLEAN--
<?php unset($orig,$c); ?>
