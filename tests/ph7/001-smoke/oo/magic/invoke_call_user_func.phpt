--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
call_user_func dispatches to __invoke on objects
--FILE--
<?php
class Mul {
    public function __invoke($x, $y) {
        return $x * $y;
    }
}
$m = new Mul();
echo call_user_func($m, 6, 7), "\n";
?>
--EXPECT--
42
--CLEAN--
<?php
unset($m);
