--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Typed property: Foo|Bar union rejects unrelated class instance
--FILE--
<?php
class TpucrFoo {}
class TpucrBar {}
class TpucrBaz {}
class TpucrHolder { public TpucrFoo|TpucrBar $x; }
$h = new TpucrHolder();
try {
    $h->x = new TpucrBaz();
} catch (TypeError $e) {
    echo $e->getMessage(), "\n";
}
?>
--EXPECT--
Cannot assign TpucrBaz to property TpucrHolder::$x of type TpucrFoo|TpucrBar
--CLEAN--
<?php
unset($h);
