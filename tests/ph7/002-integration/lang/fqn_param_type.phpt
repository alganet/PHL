--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Fully-qualified class name type hints on parameters
--FILE--
<?php
class MyException extends Exception {}

function handleError(\Exception $e): string {
    return $e->getMessage();
}
echo handleError(new MyException("oops")) . "\n";

function process(?\Exception $e): string {
    if ($e === null) {
        return "no error";
    }
    return $e->getMessage();
}
echo process(null) . "\n";
echo process(new Exception("fail")) . "\n";
?>
--EXPECT--
oops
no error
fail
--CLEAN--
<?php

