--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: realpath returns string for the current file
--FILE--
<?php
$tmp = __FILE__;
// Show that realpath returns a string
echo gettype(realpath($tmp)) . "\n";
?>
--EXPECT--
string
--CLEAN--
<?php
unset($tmp);
?>
