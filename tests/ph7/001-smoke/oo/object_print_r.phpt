--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Object print_r functionality
--FILE--
<?php
class OprTClass {
    public $public_attr = "test";
    private $private_attr = 42;
}

$oprtObj = new OprTClass();
$oprtOut = print_r($oprtObj, true);
echo $oprtOut;
?>
--EXPECTF--
OprTClass Object
(
    [public_attr] => test
    [private_attr:OprTClass:private] => 42
)
--CLEAN--
<?php
unset($oprtObj, $oprtOut);
