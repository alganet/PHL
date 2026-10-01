--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
POLICY DIVERGENCE the scope policy: libxml_disable_entity_loader() is removed (PHL half)
--DESCRIPTION--
php DEPRECATES libxml_disable_entity_loader() since 8.0 -- "as external
entity loading is disabled by default" -- and keeps it as a state flip that
answers the previous state. PHL targets php's non-deprecated surface (the scope policy),
so the name does not exist here; external entity loading is off by default on
both engines, which is the reason php deprecated the switch. php's half is
the _zend twin.
--SKIPIF--
<?php
if (function_exists('zend_version')) {
    echo "skip PHL half of the twin pair; php's behaviour is in the _zend twin";
}
?>
--FILE--
<?php
var_dump(function_exists('libxml_disable_entity_loader'));
try {
    libxml_disable_entity_loader(true);
} catch (Error $e) {
    echo get_class($e), ": ", $e->getMessage(), "\n";
}
?>
--EXPECT--
bool(false)
Error: Call to undefined function libxml_disable_entity_loader()
