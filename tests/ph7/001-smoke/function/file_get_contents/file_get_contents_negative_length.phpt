--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
file_get_contents with a negative length throws ValueError before resolving the wrapper
--FILE--
<?php
$fn = tempnam(sys_get_temp_dir(), 'ph7_fgc');
file_put_contents($fn, 'Hello World');
try {
    file_get_contents($fn, false, null, 0, -5);
    echo "NO_ERROR\n";
} catch (\ValueError $e) {
    echo $e->getMessage(), "\n";
}
// The ValueError must win over an unknown-wrapper failure, i.e. fire up front.
try {
    file_get_contents('bogus://x', false, null, 0, -5);
    echo "NO_ERROR\n";
} catch (\ValueError $e) {
    echo $e->getMessage(), "\n";
}
echo file_get_contents($fn, false, null, 0, 5), "\n";
?>
--EXPECT--
file_get_contents(): Argument #5 ($length) must be greater than or equal to 0
file_get_contents(): Argument #5 ($length) must be greater than or equal to 0
Hello
--CLEAN--
<?php
if (isset($fn) && file_exists($fn)) unlink($fn);
unset($fn);
