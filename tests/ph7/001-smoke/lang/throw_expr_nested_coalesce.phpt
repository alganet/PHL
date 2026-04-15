--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
throw expression: right-associative chain of ?? operators
--FILE--
<?php
$a = null;
$b = null;
try {
    $v = $a ?? $b ?? throw new Exception('all null');
} catch (Exception $e) {
    echo $e->getMessage(), "\n";
}
$b = 'fallback';
$v = $a ?? $b ?? throw new Exception('never');
echo $v, "\n";
?>
--EXPECT--
all null
fallback
--CLEAN--
<?php
