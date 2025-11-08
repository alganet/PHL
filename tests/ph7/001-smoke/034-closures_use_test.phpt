--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Closures with use clause
--FILE--
<?php
$message = "Hello";
$func = function($name) use ($message) {
    return $message . " " . $name;
};
echo $func("World"); // Hello World
?>
--EXPECT--
Hello World
--CLEAN--
<?php
unset($message, $func);
?>