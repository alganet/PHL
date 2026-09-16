--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
php merely deprecates null for a non-nullable string parameter and still coerces it to "" (zend half of the twin pair — PHL rejects it, see stripslashes_null_arg.phpt)
--SKIPIF--
<?php
if (!function_exists('zend_version')) {
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
--EXPECTF--
%APassing null to parameter #1 ($string) of type string is deprecated%A
string(0) ""
--CLEAN--
<?php
unset($e);
