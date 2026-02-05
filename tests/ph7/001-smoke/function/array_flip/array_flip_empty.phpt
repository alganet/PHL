--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_flip with empty array
--FILE--
<?php
$empty = array();
$flipped = array_flip($empty);
echo "Empty array flip result: " . (empty($flipped) ? "PASS" : "FAIL") . "\n";
?>
--EXPECT--
Empty array flip result: PASS
--CLEAN--
<?php
unset($empty, $flipped);
