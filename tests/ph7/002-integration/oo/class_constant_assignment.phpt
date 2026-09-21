--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Writing $obj->NAME when NAME is only a class constant targets a (dynamic) property
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip PHL rejects dynamic-property creation that php only deprecates (§10)'; ?>
--DESCRIPTION--
php keeps class CONSTANTS and PROPERTIES in separate namespaces, so `$t->MY_CONST = 123`
never touches the constant MY_CONST (which stays 42) — it writes a PROPERTY. Since Test
declares no such property and is not #[AllowDynamicProperties], php DEPRECATES the dynamic
creation while PHL, targeting php's non-deprecated surface (§10), REJECTS it with the same
"Cannot create dynamic property" Error it raises for every dynamic property. The constant
is untouched either way. (Before the constant/property namespace split, PHL resolved the
write to the constant and raised a misleading "assignment on a constant class attribute".)
--FILE--
<?php
class Test {
    const MY_CONST = 42;
}
$t = new Test();
try {
    $t->MY_CONST = 123;
    echo "NO-THROW\n";
} catch (\Throwable $e) {
    echo get_class($e), ": ", $e->getMessage(), "\n";
}
// The constant is a separate member, untouched by the property write.
echo "MY_CONST=", Test::MY_CONST, "\n";
?>
--EXPECT--
Error: Cannot create dynamic property Test::$MY_CONST
MY_CONST=42
--CLEAN--
<?php
unset($t);
