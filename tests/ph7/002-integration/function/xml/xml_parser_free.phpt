--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
POLICY DIVERGENCE the scope policy: xml_parser_free() is removed (PHL half)
--DESCRIPTION--
php 8.5 DEPRECATES xml_parser_free() -- "as it has no effect since PHP 8.0";
the parser is freed by the garbage collector like any object. PHL targets
php's NON-deprecated surface (the scope policy), so the name does not exist here and the
call is a loud catchable Error instead of a silent no-op. php's half is the
_zend twin.
--SKIPIF--
<?php
if (function_exists('zend_version')) {
    echo "skip PHL half of the twin pair; php's behaviour is in the _zend twin";
}
?>
--FILE--
<?php
$p = xml_parser_create();
var_dump(function_exists('xml_parser_free'));
try {
    xml_parser_free($p);
} catch (Error $e) {
    echo get_class($e), ": ", $e->getMessage(), "\n";
}
// The parser is still whole -- freeing was never a thing the program did.
var_dump(xml_parse($p, "<a/>", true));
?>
--EXPECT--
bool(false)
Error: Call to undefined function xml_parser_free()
int(1)
