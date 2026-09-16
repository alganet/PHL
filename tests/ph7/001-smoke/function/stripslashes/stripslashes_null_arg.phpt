--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
POLICY DIVERGENCE (non-deprecated compatibility): passing null to a non-nullable string parameter is only DEPRECATED by php, so PHL rejects it with a TypeError instead of coercing to "". Enforced centrally by VmEnforceBuiltinArgTypes() for every builtin whose signature declares a non-nullable parameter. php's deprecating half lives in the _zend twin.
--SKIPIF--
<?php
if (function_exists('zend_version')) {
    echo "skip";
}
?>
--FILE--
<?php
try {
    var_dump(stripslashes(null));
} catch (TypeError $e) {
    echo get_class($e), ": ", $e->getMessage(), "\n";
}
?>
--EXPECT--
TypeError: stripslashes(): Argument #1 ($string) must be of type string, null given
--CLEAN--
<?php
unset($e);
