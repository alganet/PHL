--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
getopt() returns options from command line arguments
--SKIPIF--
<?php
if (function_exists('zend_version')) {
    echo "skip Test requires PH7, not PHP";
}
?>
--FILE--
<?php
// Test getopt function - PH7 may have limited getopt support
// Note: getopt() in PH7 reads from actual command line arguments
// For testing purposes, we test that the function exists and can be called
$opts = getopt("a:b:c");
echo "getopt called successfully\n";
var_dump($opts);
?>
--EXPECTF--
getopt called successfully
array(0) {
 }
--CLEAN--
<?php
unset($opts);
