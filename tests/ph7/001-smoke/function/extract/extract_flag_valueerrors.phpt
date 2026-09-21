--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
extract: out-of-range $flags, missing $prefix and a non-identifier $prefix all throw ValueError
--FILE--
<?php
$exve_try = function ($fn) {
    try {
        var_dump($fn());
    } catch (\ValueError $e) {
        echo $e->getMessage(), "\n";
    } catch (\TypeError $e) {
        echo $e->getMessage(), "\n";
    }
};
$exve_src = ['a' => 1];
// (flags & 0xff) outside 0..6
$exve_try(fn() => extract($exve_src, 7));
$exve_try(fn() => extract($exve_src, -1));
$exve_try(fn() => extract($exve_src, 64, 'p'));
// the four prefixing modes require $prefix
$exve_try(fn() => extract($exve_src, EXTR_PREFIX_SAME));
$exve_try(fn() => extract($exve_src, EXTR_PREFIX_ALL));
$exve_try(fn() => extract($exve_src, EXTR_PREFIX_INVALID));
$exve_try(fn() => extract($exve_src, EXTR_PREFIX_IF_EXISTS));
// ... the other three do not
$exve_try(fn() => extract($exve_src, EXTR_OVERWRITE));
$exve_try(fn() => extract($exve_src, EXTR_SKIP));
$exve_try(fn() => extract($exve_src, EXTR_IF_EXISTS));
// a non-empty prefix must be a legal identifier; "" is legal
$exve_try(fn() => extract($exve_src, EXTR_PREFIX_ALL, '1x'));
$exve_try(fn() => extract($exve_src, EXTR_PREFIX_ALL, 'a b'));
$exve_try(fn() => extract($exve_src, EXTR_PREFIX_ALL, ''));
// argument types
$exve_try(fn() => extract($exve_src, 'nope'));
?>
--EXPECT--
extract(): Argument #2 ($flags) must be a valid extract type
extract(): Argument #2 ($flags) must be a valid extract type
extract(): Argument #2 ($flags) must be a valid extract type
extract(): Argument #3 ($prefix) is required when using this extract type
extract(): Argument #3 ($prefix) is required when using this extract type
extract(): Argument #3 ($prefix) is required when using this extract type
extract(): Argument #3 ($prefix) is required when using this extract type
int(1)
int(1)
int(0)
extract(): Argument #3 ($prefix) must be a valid identifier
extract(): Argument #3 ($prefix) must be a valid identifier
int(1)
extract(): Argument #2 ($flags) must be of type int, string given
--CLEAN--
<?php
