--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7: PH7_VERSION string value
--SKIPIF--
<?php
if (function_exists('zend_version')) {
    echo "skip";
}
?>
--FILE--
<?php
echo "PH7_VERSION=" . PH7_VERSION . "\n";
?>
--EXPECTF--
PH7_VERSION=PH7/%s
--CLEAN--
<?php

