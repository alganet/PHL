--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
php_uname refuses every mode outside its six
--FILE--
<?php
foreach (['x', 'A', 'N', '', 'nn', ' n', 'a '] as $unameMode) {
    try {
        php_uname($unameMode);
        echo var_export($unameMode, true), " => ok\n";
    } catch (Throwable $unameErr) {
        echo var_export($unameMode, true), " => ", get_class($unameErr), ": ", $unameErr->getMessage(), "\n";
    }
}
try { php_uname('a', 'b'); } catch (Throwable $unameErr) { echo get_class($unameErr), ": ", $unameErr->getMessage(), "\n"; }
try { php_uname([]); } catch (Throwable $unameErr) { echo get_class($unameErr), ": ", $unameErr->getMessage(), "\n"; }
?>
--EXPECT--
'x' => ValueError: php_uname(): Argument #1 ($mode) must be one of "a", "m", "n", "r", "s", or "v"
'A' => ValueError: php_uname(): Argument #1 ($mode) must be one of "a", "m", "n", "r", "s", or "v"
'N' => ValueError: php_uname(): Argument #1 ($mode) must be one of "a", "m", "n", "r", "s", or "v"
'' => ValueError: php_uname(): Argument #1 ($mode) must be a single character
'nn' => ValueError: php_uname(): Argument #1 ($mode) must be a single character
' n' => ValueError: php_uname(): Argument #1 ($mode) must be a single character
'a ' => ValueError: php_uname(): Argument #1 ($mode) must be a single character
ArgumentCountError: php_uname() expects at most 1 argument, 2 given
TypeError: php_uname(): Argument #1 ($mode) must be of type string, array given
--CLEAN--
<?php
unset($unameMode, $unameErr);
