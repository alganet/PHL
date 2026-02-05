--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
rtrim with empty string
--FILE--
<?php
// Test rtrim with empty string input
$result = rtrim('', 'abc');
echo "'" . $result . "'\n";
?>
--EXPECT--
''
--CLEAN--
<?php
unset($result);
