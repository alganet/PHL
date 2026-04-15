--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
throw expression: ternary else branch
--FILE--
<?php
$n = -3;
try {
    $v = $n > 0 ? $n : throw new Exception('negative');
} catch (Exception $e) {
    echo $e->getMessage(), "\n";
}
$n = 7;
$v = $n > 0 ? $n : throw new Exception('never');
echo $v, "\n";
?>
--EXPECT--
negative
7
--CLEAN--
<?php
