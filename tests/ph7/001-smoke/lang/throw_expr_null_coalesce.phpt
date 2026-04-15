--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
throw expression: right-hand side of null coalescing operator
--FILE--
<?php
$input = null;
try {
    $v = $input ?? throw new Exception('required');
} catch (Exception $e) {
    echo $e->getMessage(), "\n";
}
$input = 'ok';
$v = $input ?? throw new Exception('never');
echo $v, "\n";
?>
--EXPECT--
required
ok
--CLEAN--
<?php
