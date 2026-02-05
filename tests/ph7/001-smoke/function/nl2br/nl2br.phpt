--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
nl2br replaces newlines with <br /> tags
--SKIPIF--
<?php
if (function_exists('zend_version')) {
    echo "skip";
}
--FILE--
<?php
echo nl2br("a\nb") . "\n";
?>
--EXPECT--
a<br/>
b
--CLEAN--
<?php

