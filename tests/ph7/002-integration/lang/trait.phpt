--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
trait definition compiles without error
--FILE--
<?php
trait Hello {
    public function sayHello() {
        echo 'Hello ';
    }
}
echo "ok\n";
?>
--EXPECT--
ok
--CLEAN--
<?php

