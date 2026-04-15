--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
throw expression: inside an assignment within a function's try/catch
--FILE--
<?php
function load($src) {
    try {
        $value = $src ?? throw new Exception('no source');
        echo "loaded: $value\n";
    } catch (Exception $e) {
        echo 'caught: ', $e->getMessage(), "\n";
    }
}
load(null);
load('data');
?>
--EXPECT--
caught: no source
loaded: data
--CLEAN--
<?php
