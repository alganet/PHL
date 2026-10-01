--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
POLICY DIVERGENCE the scope policy: xml_parser_free() deprecates and answers true (php half)
--DESCRIPTION--
The php half of xml_parser_free.phpt: php 8.5 emits "Function xml_parser_free()
is deprecated since 8.5, as it has no effect since PHP 8.0" and answers true.
PHL removes the name outright (the scope policy).
--SKIPIF--
<?php
if (!function_exists('zend_version')) {
    echo "skip php half of the twin pair; PHL's behaviour is in the non-_zend member";
}
?>
--FILE--
<?php
set_error_handler(function ($no, $msg) { echo "[$no] $msg\n"; return true; });
$p = xml_parser_create();
var_dump(xml_parser_free($p));
var_dump(xml_parse($p, "<a/>", true));
?>
--EXPECT--
[8192] Function xml_parser_free() is deprecated since 8.5, as it has no effect since PHP 8.0
bool(true)
int(1)
