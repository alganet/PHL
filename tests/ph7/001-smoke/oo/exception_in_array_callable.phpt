--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
An exception thrown by an array-callable [$obj,'m']() unwinds the call
--FILE--
<?php
class ExcArrCallable_C {
    public function m() { throw new Exception("arr"); }
}
$o = new ExcArrCallable_C();
$c = [$o, 'm'];
try {
    $c();
    echo "after\n"; // must NOT run
} catch (Exception $e) {
    echo "caught: ", $e->getMessage(), "\n";
}
?>
--EXPECT--
caught: arr
--CLEAN--
<?php
unset($o, $c);
