--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
bcscale() IS the bcmath.scale directive: one slot, and every $scale defaults from it
--DESCRIPTION--
bcscale() has no state of its own -- it reads and writes `bcmath.scale`, so
ini_set() and ini_get() see the same value and a bc* call with no $scale (or a
null one) takes the directive's. The directive is bounded 0..2147483647 and a
write outside that is refused in silence, keeping what it had.
--FILE--
<?php
function bcscale_directive_probe(): void {
    $saved = bcscale();
    try {
        var_dump(ini_get('bcmath.scale'), bcscale());
        echo "## bcscale() answers the OLD value when it sets\n";
        var_dump(bcscale(3), bcscale(), ini_get('bcmath.scale'));
        echo "## and the directive is what an absent or null \$scale means\n";
        var_dump(bcadd('1.23456', '2'), bcadd('1.23456', '2', null),
                 bcadd('1.23456', '2', 1));
        echo "## ini_set writes the same slot\n";
        var_dump(ini_set('bcmath.scale', '4'), bcscale(), bcadd('1.23456', '2'));
        echo "## out of range is refused in silence, in either direction\n";
        var_dump(ini_set('bcmath.scale', '-1'), ini_set('bcmath.scale', '99999999999'),
                 bcscale());
        var_dump(ini_set('bcmath.scale', '2147483647'), ini_get('bcmath.scale'));
        echo "## the same bound, worded from the function\n";
        foreach ([-1, PHP_INT_MAX] as $bad) {
            try { bcscale($bad); } catch (Throwable $e) {
                echo get_class($e), ': ', $e->getMessage(), "\n";
            }
        }
        echo "## the extension reports itself, and owns exactly one directive\n";
        var_dump(extension_loaded('bcmath'), in_array('bcmath', get_loaded_extensions()));
        var_dump(array_keys(ini_get_all('bcmath')));
        var_dump(ini_get_all('bcmath')['bcmath.scale']['access']);
    } finally {
        /* The smoke corpus runs in ONE interpreter: put the directive back. */
        bcscale($saved);
    }
    var_dump(bcscale());
}
bcscale_directive_probe();
?>
--EXPECT--
string(1) "0"
int(0)
## bcscale() answers the OLD value when it sets
int(0)
int(3)
string(1) "3"
## and the directive is what an absent or null $scale means
string(5) "3.234"
string(5) "3.234"
string(3) "3.2"
## ini_set writes the same slot
string(1) "3"
int(4)
string(6) "3.2345"
## out of range is refused in silence, in either direction
bool(false)
bool(false)
int(4)
string(1) "4"
string(10) "2147483647"
## the same bound, worded from the function
ValueError: bcscale(): Argument #1 ($scale) must be between 0 and 2147483647
ValueError: bcscale(): Argument #1 ($scale) must be between 0 and 2147483647
## the extension reports itself, and owns exactly one directive
bool(true)
bool(true)
array(1) {
  [0]=>
  string(12) "bcmath.scale"
}
int(7)
int(0)
