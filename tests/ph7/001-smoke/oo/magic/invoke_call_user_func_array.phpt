--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
call_user_func_array dispatches to __invoke on objects
--FILE--
<?php
class Concat {
    public function __invoke($a, $b, $c) {
        return $a . $b . $c;
    }
}
$obj = new Concat();
echo call_user_func_array($obj, ["foo", "-", "bar"]), "\n";
?>
--EXPECT--
foo-bar
--CLEAN--
<?php
unset($obj);
