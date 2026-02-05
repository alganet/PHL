--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7: strtoupper missing argument returns NULL
--SKIPIF--
<?php if(function_exists('zend_version')) { echo 'skip'; } ?>
--FILE--
<?php
if (strtoupper() === null) {
    echo "true";
} else {
    echo "false";
}
?>
--EXPECT--
true
--CLEAN--
<?php

