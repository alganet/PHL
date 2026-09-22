--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: JSON_ERROR_UTF16 constant
--FILE--
<?php
echo "JSON_ERROR_UTF16=" . JSON_ERROR_UTF16 . "\n";
echo json_last_error_msg(), "\n";
json_decode('"\ud83d"');
echo json_last_error() === JSON_ERROR_UTF16 ? "yes\n" : "no\n";
echo json_last_error_msg(), "\n";
?>
--EXPECT--
JSON_ERROR_UTF16=10
No error
yes
Single unpaired UTF-16 surrogate in unicode escape
--CLEAN--
<?php
