--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
throw expression: inside an arrow function body
--FILE--
<?php
$required = fn($x) => $x ?? throw new Exception('missing');
try {
    $required(null);
} catch (Exception $e) {
    echo $e->getMessage(), "\n";
}
echo $required('value'), "\n";
?>
--EXPECT--
missing
value
--CLEAN--
<?php
