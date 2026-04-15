--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
throw expression: right-hand side of || short-circuit
--FILE--
<?php
$ok = false;
try {
    $ok || throw new Exception('bad');
} catch (Exception $e) {
    echo $e->getMessage(), "\n";
}
$ok = true;
$ok || throw new Exception('never');
echo "passed\n";
?>
--EXPECT--
bad
passed
--CLEAN--
<?php
