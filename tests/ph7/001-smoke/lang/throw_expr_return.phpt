--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
throw expression: inside a return statement
--FILE--
<?php
function required($x) {
    return $x ?? throw new Exception('required');
}
try {
    required(null);
} catch (Exception $e) {
    echo $e->getMessage(), "\n";
}
echo required('ok'), "\n";
?>
--EXPECT--
required
ok
--CLEAN--
<?php
