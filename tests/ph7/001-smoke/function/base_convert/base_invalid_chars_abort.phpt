--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
POLICY DIVERGENCE: invalid base-conversion characters are a ValueError that ABORTS the call — no value is produced. php's deprecating half lives in the _zend twin.
--SKIPIF--
<?php
if (function_exists('zend_version')) {
    echo "skip";
}
?>
--FILE--
<?php
// php skips the invalid digits, deprecates the skip, and still returns a value.
// §10 rejects that surface loudly; the throw must stop the builtin, so $bica_r is
// never assigned and nothing after the throwing call inside the try runs.
$bica_r = "untouched";
foreach ([
    'bindec'  => static fn() => bindec("10z1"),
    'octdec'  => static fn() => octdec("178"),
    'hexdec'  => static fn() => hexdec("1g2"),
    'baseconv' => static fn() => base_convert("12z", 16, 10),
] as $bica_name => $bica_fn) {
    try {
        $bica_r = $bica_fn();
        echo "NO_THROW\n";
    } catch (\ValueError $e) {
        echo $bica_name, ": ", $e->getMessage(), "\n";
    }
}
var_dump($bica_r);
?>
--EXPECT--
bindec: Invalid characters passed for attempted conversion
octdec: Invalid characters passed for attempted conversion
hexdec: Invalid characters passed for attempted conversion
baseconv: Invalid characters passed for attempted conversion
string(9) "untouched"
--CLEAN--
<?php
unset($e);
