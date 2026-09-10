--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Non-capturing catch (PHP 8.0): catch (Type) / catch (A|B) with no variable
--FILE--
<?php
// Single type, no variable.
try {
    throw new RuntimeException('boom');
} catch (RuntimeException) {
    echo "caught single\n";
}

// Union type, no variable.
try {
    throw new LogicException('x');
} catch (RuntimeException|LogicException) {
    echo "caught union\n";
}

// Capturing catch still works and binds the variable.
try {
    throw new Exception('msg');
} catch (Exception $e) {
    echo "caught var: ", $e->getMessage(), "\n";
}

// Non-capturing inside a generator body (inline try/catch path).
function ncgen() {
    try {
        throw new Exception('g');
    } catch (Exception) {
        yield "gen caught\n";
    }
}
foreach (ncgen() as $line) {
    echo $line;
}
?>
--EXPECT--
caught single
caught union
caught var: msg
gen caught
--CLEAN--
<?php
