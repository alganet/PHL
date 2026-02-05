--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Instanceof operator
--FILE--
<?php
class InhInstanceofTestClass {
    public $value = 42;
}
$obj = new InhInstanceofTestClass();
echo $obj instanceof InhInstanceofTestClass ? "yes" : "no"; // yes
echo "\n";
$not_obj = "string";
echo $not_obj instanceof InhInstanceofTestClass ? "yes" : "no"; // no
?>
--EXPECT--
yes
no
--CLEAN--
<?php
unset($obj, $not_obj);
