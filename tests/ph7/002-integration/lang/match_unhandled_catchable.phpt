--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Match expression: UnhandledMatchError is catchable and extends Error
--SKIPIF--
skip: macos
--FILE--
<?php
try {
    $r = match (99) { 1 => 'one' };
} catch (\UnhandledMatchError $e) {
    echo get_class($e), ": ", $e->getMessage(), "\n";
    var_dump($e instanceof \Error, $e instanceof \Throwable);
}
try {
    $r = match ('zz') { 'a' => 1 };
} catch (\Error $e) {
    echo "as Error: ", $e->getMessage(), "\n";
}
echo "alive\n";
?>
--EXPECT--
UnhandledMatchError: Unhandled match case of type int
bool(true)
bool(true)
as Error: Unhandled match case of type string
alive
