--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
get_defined_constants() lists user constants in the order they were defined, not newest first
--FILE--
<?php
// The constant table is a head-pushed hash, so walking it forward listed the
// most recent definition first; php lists them in definition order, through
// define(), a `const` statement, a conditional block and a function body alike.
define('GDCO_A', 1);
const GDCO_B = 2;
define('GDCO_C', 3);
if (true) {
    define('GDCO_D', 4);
}
function gdco_define() {
    define('Gdco\E', 5);
}
gdco_define();
const GDCO_F = 6;
define('GDCO_G', 7);

$gdco_pick = fn ($list) => array_values(array_filter(array_keys($list),
    fn ($n) => stripos($n, 'gdco') !== false));
echo implode(',', $gdco_pick(get_defined_constants(true)['user'])), "\n";
echo implode(',', $gdco_pick(get_defined_constants())), "\n";
$gdco_all = array_keys(get_defined_constants());
echo end($gdco_all), "\n";
?>
--EXPECT--
GDCO_A,GDCO_B,GDCO_C,GDCO_D,Gdco\E,GDCO_F,GDCO_G
GDCO_A,GDCO_B,GDCO_C,GDCO_D,Gdco\E,GDCO_F,GDCO_G
GDCO_G
