--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Magic method __toString
--FILE--
<?php
class FooToString {
    public function __toString(){
        return "FooToString";
    }
}
$o = new FooToString();
echo "as string: $o\n";
?>
--EXPECT--
as string: FooToString
--CLEAN--
<?php
unset($o);
