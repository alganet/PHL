--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
file() on a missing path warns and returns false; a non-string path is a TypeError
--FILE--
<?php
// A missing file is a runtime failure: warning + false
$result = @file("/nonexistent/path/file.txt");
echo "invalid_path: " . (is_array($result) ? count($result) : "false") . "\n";

// A non-string path cannot be coerced: php rejects it at the call boundary
try {
    file(array("test"));
} catch (TypeError $e) {
    echo "array_arg: ", $e->getMessage(), "\n";
}
?>
--EXPECT--
invalid_path: false
array_arg: file(): Argument #1 ($filename) must be of type string, array given
--CLEAN--
<?php
unset($result);
