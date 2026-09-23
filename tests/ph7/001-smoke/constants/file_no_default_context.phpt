--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: FILE_NO_DEFAULT_CONTEXT constant, accepted by file()
--FILE--
<?php
echo "FILE_NO_DEFAULT_CONTEXT=", FILE_NO_DEFAULT_CONTEXT, "\n";
$fn = tempnam(sys_get_temp_dir(), 'ph7_fndc');
file_put_contents($fn, "l1\nl2\n");
var_dump(file($fn, FILE_NO_DEFAULT_CONTEXT | FILE_IGNORE_NEW_LINES));
// It counts as a VALID bit of file()'s $flags mask; the next power of two does not.
try {
    file($fn, 1024);
} catch (\ValueError $e) {
    echo $e->getMessage(), "\n";
}
unlink($fn);
?>
--EXPECT--
FILE_NO_DEFAULT_CONTEXT=16
array(2) {
  [0]=>
  string(2) "l1"
  [1]=>
  string(2) "l2"
}
file(): Argument #2 ($flags) must be a valid flag value
--CLEAN--
<?php
unset($fn);
