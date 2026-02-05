--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Nested try/catch with thrown exception from inner function
--FILE--
<?php
function inner() {
    throw new Exception("inner_exc");
}

try {
    try {
        inner();
    } catch (Exception $e) {
        echo "caught inner: " . $e->getMessage() . "\n";
        // rethrow
        throw $e;
    }
} catch (Exception $e) {
    echo "caught outer: " . $e->getMessage() . "\n";
}
--EXPECT--
caught inner: inner_exc
caught outer: inner_exc
--CLEAN--
<?php

