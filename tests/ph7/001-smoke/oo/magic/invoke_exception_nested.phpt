--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
An exception unwinds through a nested __invoke that has no handler
--DESCRIPTION--
The inner __invoke throws; the outer __invoke has no try/catch, so the exception
must unwind through it (its trailing code must not run) up to the caller's catch.
--FILE--
<?php
class InvokeNested_Inner {
    public function __invoke() { throw new Exception("deep"); }
}
class InvokeNested_Outer {
    public function __invoke($f) {
        $f();
        echo "outer-after\n"; // must NOT run
    }
}
try {
    (new InvokeNested_Outer())(new InvokeNested_Inner());
    echo "call-after\n"; // must NOT run
} catch (Exception $e) {
    echo "caught: ", $e->getMessage(), "\n";
}
?>
--EXPECT--
caught: deep
--CLEAN--
<?php
