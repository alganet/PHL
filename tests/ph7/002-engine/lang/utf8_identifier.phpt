--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
UTF-8 characters in identifiers
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--FILE--
<?php
$変数 = "Japanese var";
echo $変数 . "\n";

$αβγ = "Greek letters";
echo $αβγ . "\n";

function résumé($ñ) {
    return "Got: " . $ñ;
}
echo résumé("tilde-n") . "\n";
?>
--EXPECT--
Japanese var
Greek letters
Got: tilde-n