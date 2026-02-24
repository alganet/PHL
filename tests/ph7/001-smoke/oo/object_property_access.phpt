--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Object property access
--FILE--
<?php
class ObjectPropertyAccessTestClass {
    public $publicProp = "public";
    private $privateProp = "private";
    protected $protectedProp = "protected";
}

$obj = new ObjectPropertyAccessTestClass();
echo $obj->publicProp . "\n";
echo "Object created successfully\n";
?>
--EXPECT--
public
Object created successfully
--CLEAN--
<?php
unset($obj);
