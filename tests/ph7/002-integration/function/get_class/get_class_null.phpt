--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
get_class(null) throws php's TypeError (now cross-engine: PHL used to return false behind a zend_version guard; the central non-nullable-parameter screen makes it php-exact)
--FILE--
<?php
try {
    get_class(null);
} catch (TypeError $e) {
    echo $e->getMessage(), "\n";
}
?>
--EXPECT--
get_class(): Argument #1 ($object) must be of type object, null given
--CLEAN--
<?php
unset($e);
