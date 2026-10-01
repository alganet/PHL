--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
POLICY DIVERGENCE the scope policy: libxml_disable_entity_loader() deprecates and flips state (php half)
--DESCRIPTION--
The php half of libxml_disable_entity_loader.phpt: php 8 emits the 8.0
deprecation and answers the PREVIOUS state each call. PHL removes the name
outright (the scope policy).
--SKIPIF--
<?php
if (!function_exists('zend_version')) {
    echo "skip php half of the twin pair; PHL's behaviour is in the non-_zend member";
}
?>
--FILE--
<?php
set_error_handler(function ($no, $msg) { echo "[$no] $msg\n"; return true; });
var_dump(libxml_disable_entity_loader(true));
var_dump(libxml_disable_entity_loader(false));
?>
--EXPECT--
[8192] Function libxml_disable_entity_loader() is deprecated since 8.0, as external entity loading is disabled by default
bool(false)
[8192] Function libxml_disable_entity_loader() is deprecated since 8.0, as external entity loading is disabled by default
bool(true)
