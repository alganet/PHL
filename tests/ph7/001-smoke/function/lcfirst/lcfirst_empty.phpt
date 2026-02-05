--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
lcfirst with empty string
--FILE--
<?php
$result = lcfirst('');
echo "Result: '" . $result . "'\n";
echo "Length: " . strlen($result) . "\n";
?>
--EXPECT--
Result: ''
Length: 0
--CLEAN--
<?php
unset($result);
