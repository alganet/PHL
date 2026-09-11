--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
phl.stub_extensions (-d/php.ini) makes extension_loaded() report the listed extensions
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip PHL-specific phl.stub_extensions directive';
--INI--
phl.stub_extensions=phlfakeext, another_fake
--FILE--
<?php
var_dump(extension_loaded('phlfakeext'));
var_dump(extension_loaded('another_fake'));
var_dump(extension_loaded('ANOTHER_FAKE'));      // case-insensitive
var_dump(extension_loaded('definitely_not_here'));
var_dump(extension_loaded('json'));              // built-in stays true
var_dump(in_array('phlfakeext', get_loaded_extensions(), true));
?>
--EXPECT--
bool(true)
bool(true)
bool(true)
bool(false)
bool(true)
bool(true)
--CLEAN--
<?php
