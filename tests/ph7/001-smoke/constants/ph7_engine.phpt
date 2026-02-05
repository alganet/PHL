--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7: PH7_ENGINE constant value
--SKIPIF--
<?php
if (function_exists('zend_version')) {
    echo "skip";
}
?>
--FILE--
<?php
echo "PH7_ENGINE=" . PH7_ENGINE . "\n";
?>
--EXPECT--
PH7_ENGINE=PH7/2.1.4
--CLEAN--
<?php

