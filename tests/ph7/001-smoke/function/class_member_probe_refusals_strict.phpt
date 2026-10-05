--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
method_exists/property_exists/get_parent_class refuse a non-class under strict_types
--DESCRIPTION--
The strict face of class_member_probe_refusals.phpt: the screen is the function
body's, so a strict file sees the same TypeErrors as a weak one, and an int
second argument is refused by the declared `string` parameter first.
--FILE--
<?php
declare(strict_types=1);
class CmpsLeaf {}
$cmpsShow = function ($label, $fn) {
    try { $out = var_export($fn(), true); }
    catch (TypeError $e) { $out = $e->getMessage(); }
    echo $label, ' => ', $out, "\n";
};
$cmpsShow("property_exists(1, 'a')", fn() => property_exists(1, 'a'));
$cmpsShow("method_exists(1.5, 'a')", fn() => method_exists(1.5, 'a'));
$cmpsShow("property_exists('CmpsLeaf', 1)", fn() => property_exists('CmpsLeaf', 1));
$cmpsShow("property_exists(1, 1)", fn() => property_exists(1, 1));
$cmpsShow("get_parent_class(1)", fn() => get_parent_class(1));
$cmpsShow("get_parent_class(null)", fn() => get_parent_class(null));
$cmpsShow("get_parent_class('CmpsNope')", fn() => get_parent_class('CmpsNope'));
$cmpsShow("get_parent_class('CmpsLeaf')", fn() => get_parent_class('CmpsLeaf'));
--EXPECT--
property_exists(1, 'a') => property_exists(): Argument #1 ($object_or_class) must be of type object|string, int given
method_exists(1.5, 'a') => method_exists(): Argument #1 ($object_or_class) must be of type object|string, float given
property_exists('CmpsLeaf', 1) => property_exists(): Argument #2 ($property) must be of type string, int given
property_exists(1, 1) => property_exists(): Argument #2 ($property) must be of type string, int given
get_parent_class(1) => get_parent_class(): Argument #1 ($object_or_class) must be an object or a valid class name, int given
get_parent_class(null) => get_parent_class(): Argument #1 ($object_or_class) must be an object or a valid class name, null given
get_parent_class('CmpsNope') => get_parent_class(): Argument #1 ($object_or_class) must be an object or a valid class name, string given
get_parent_class('CmpsLeaf') => false
