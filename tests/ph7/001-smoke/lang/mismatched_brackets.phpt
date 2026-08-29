--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Test mismatched brackets error handling

--FILE--
<?php
// Test mismatched brackets
if (true) {
    echo "test";
}
?>
--EXPECT--
test
--CLEAN--
<?php

