--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
mb_strpos/mb_stripos/mb_strrpos apply php's $offset window and empty-needle rules
--FILE--
<?php
// The 8-bit family already refuses an out-of-range $offset and matches an empty
// needle; the mb_ twins answered false for both, which is what a genuine miss
// answers too.
function mbSearchCase(string $h, string $n, $off = null): void {
    foreach (['mb_strpos', 'mb_stripos', 'mb_strrpos'] as $f) {
        try {
            $r = $off === null ? $f($h, $n) : $f($h, $n, $off);
            echo $f, '(', $h, ',', $n, ',', var_export($off, true), ') => ',
                var_export($r, true), "\n";
        } catch (\Throwable $e) {
            echo $f, '(', $h, ',', $n, ',', var_export($off, true), ') => ',
                get_class($e), ': ', $e->getMessage(), "\n";
        }
    }
}
// "áéíóú" is 5 code points in 10 bytes: the window is counted in CODE POINTS.
mbSearchCase("áéíóú", "í", 6);
mbSearchCase("áéíóú", "í", -6);
mbSearchCase("áéíóú", "í", 5);
mbSearchCase("áéíóú", "í", -5);
// A negative $offset is an UPPER bound on where a backwards match may start,
// not a place to start counting forward from.
mbSearchCase("áéíóú", "í", -1);
mbSearchCase("áéíóú", "í", 0);
// An empty needle matches at the offset itself.
mbSearchCase("abc", "");
mbSearchCase("abc", "", 1);
mbSearchCase("abc", "", 3);
mbSearchCase("abc", "", -1);
--EXPECT--
mb_strpos(áéíóú,í,6) => ValueError: mb_strpos(): Argument #3 ($offset) must be contained in argument #1 ($haystack)
mb_stripos(áéíóú,í,6) => ValueError: mb_stripos(): Argument #3 ($offset) must be contained in argument #1 ($haystack)
mb_strrpos(áéíóú,í,6) => ValueError: mb_strrpos(): Argument #3 ($offset) must be contained in argument #1 ($haystack)
mb_strpos(áéíóú,í,-6) => ValueError: mb_strpos(): Argument #3 ($offset) must be contained in argument #1 ($haystack)
mb_stripos(áéíóú,í,-6) => ValueError: mb_stripos(): Argument #3 ($offset) must be contained in argument #1 ($haystack)
mb_strrpos(áéíóú,í,-6) => ValueError: mb_strrpos(): Argument #3 ($offset) must be contained in argument #1 ($haystack)
mb_strpos(áéíóú,í,5) => false
mb_stripos(áéíóú,í,5) => false
mb_strrpos(áéíóú,í,5) => false
mb_strpos(áéíóú,í,-5) => 2
mb_stripos(áéíóú,í,-5) => 2
mb_strrpos(áéíóú,í,-5) => false
mb_strpos(áéíóú,í,-1) => false
mb_stripos(áéíóú,í,-1) => false
mb_strrpos(áéíóú,í,-1) => 2
mb_strpos(áéíóú,í,0) => 2
mb_stripos(áéíóú,í,0) => 2
mb_strrpos(áéíóú,í,0) => 2
mb_strpos(abc,,NULL) => 0
mb_stripos(abc,,NULL) => 0
mb_strrpos(abc,,NULL) => 3
mb_strpos(abc,,1) => 1
mb_stripos(abc,,1) => 1
mb_strrpos(abc,,1) => 3
mb_strpos(abc,,3) => 3
mb_stripos(abc,,3) => 3
mb_strrpos(abc,,3) => 3
mb_strpos(abc,,-1) => 2
mb_stripos(abc,,-1) => 2
mb_strrpos(abc,,-1) => 2
--CLEAN--
<?php
