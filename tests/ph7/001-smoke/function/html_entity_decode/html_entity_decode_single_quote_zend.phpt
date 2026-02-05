--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
html_entity_decode behavior for single quotes with ENT_QUOTES
--SKIPIF--
<?php
if (!function_exists('zend_version')) {
    echo "skip";
}
?>
--FILE--
<?php
echo html_entity_decode("&#39;Hi&#39;") . "\n";
echo html_entity_decode("&#39;Hi&#39;", ENT_QUOTES) . "\n";
?>
--EXPECT--
'Hi'
'Hi'
--CLEAN--
<?php

