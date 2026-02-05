--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
floatval returns 0.0 when called with no arguments
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--FILE--
<?php
$result = floatval();
if ($result === 0.0) {
    echo "floatval with no args returns 0.0\n";
} else {
    echo "unexpected result\n";
}
?>
--EXPECT--
floatval with no args returns 0.0
--CLEAN--
<?php
unset($result);
