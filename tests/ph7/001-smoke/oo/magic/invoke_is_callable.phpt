--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
is_callable returns true for objects with __invoke regardless of return value
--FILE--
<?php
class WithInvoke {
    public function __invoke() { return 1; }
}
class InvokeReturnsFalse {
    public function __invoke() { return false; }
}
class WithoutInvoke {}
echo is_callable(new WithInvoke()) ? "yes\n" : "no\n";
echo is_callable(new InvokeReturnsFalse()) ? "yes\n" : "no\n";
echo is_callable(new WithoutInvoke()) ? "yes\n" : "no\n";
?>
--EXPECT--
yes
yes
no
--CLEAN--
<?php
