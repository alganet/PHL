--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Object var_dump functionality
--FILE--
<?php
class OvdTClass {
    public $public_attr = "test";
    private $private_attr = 42;
}

$ovdtObj = new OvdTClass();
ob_start();
var_dump($ovdtObj);
$ovdtOut = ob_get_clean();
echo $ovdtOut;
?>
--EXPECTF--
object(OvdTClass)#%d (2) {
  ["public_attr"]=>
  string(4) "test"
  ["private_attr":"OvdTClass":private]=>
  int(42)
}
--CLEAN--
<?php
unset($ovdtObj, $ovdtOut);
