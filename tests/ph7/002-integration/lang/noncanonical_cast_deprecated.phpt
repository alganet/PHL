--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
The four non-canonical cast spellings announce themselves, once per occurrence
--DESCRIPTION--
php 8.5 deprecates the four alias SPELLINGS -- (integer), (boolean), (double),
(binary) -- while the casts themselves keep working. The sentence is scanner-level,
so it is one line per OCCURRENCE in the source rather than per execution, and it
lands ahead of the program's own first byte. Both gates are pinned because the
stock corpus runs neither: this diagnostic owes the display copy and the log copy
that every other compile-time one does. The level is pinned NUMERICALLY because
this box's php.ini masks E_DEPRECATED and the symbolic spelling is read by the two
engines differently.
--INI--
error_reporting=30719
display_errors=1
log_errors=1
--FILE--
<?php
/* php 8.5 deprecates the SPELLING, not the cast: the conversion is unchanged and
 * the program runs. The sentence is the scanner's, so it comes out ahead of the
 * program's own output, once for every occurrence in the source -- including one
 * in a function nobody calls and one in a branch nobody takes -- and never for
 * the canonical spelling. */
function never_called()
{
    $ignored = (integer)"1";
}
if (false) {
    $ignored = (boolean)"1";
}
for ($i = 0; $i < 3; $i++) {
    $looped = (double)"1";
}
echo "--- program starts here\n";
var_dump((integer)"1", (boolean)"1", (double)"1.5", (binary)"1");
var_dump((int)"1", (bool)"1", (float)"1.5", (string)1);
var_dump(( integer )"1", (Integer)"1");
eval('$e = (double)"2.5"; var_dump($e);');
echo "done\n";
?>
--EXPECTF--

Deprecated: Non-canonical cast (integer) is deprecated, use the (int) cast instead in %s on line 9

Deprecated: Non-canonical cast (boolean) is deprecated, use the (bool) cast instead in %s on line 12

Deprecated: Non-canonical cast (double) is deprecated, use the (float) cast instead in %s on line 15

Deprecated: Non-canonical cast (integer) is deprecated, use the (int) cast instead in %s on line 18

Deprecated: Non-canonical cast (boolean) is deprecated, use the (bool) cast instead in %s on line 18

Deprecated: Non-canonical cast (double) is deprecated, use the (float) cast instead in %s on line 18

Deprecated: Non-canonical cast (binary) is deprecated, use the (string) cast instead in %s on line 18

Deprecated: Non-canonical cast (integer) is deprecated, use the (int) cast instead in %s on line 20

Deprecated: Non-canonical cast (integer) is deprecated, use the (int) cast instead in %s on line 20
--- program starts here
int(1)
bool(true)
float(1.5)
string(1) "1"
int(1)
bool(true)
float(1.5)
string(1) "1"
int(1)
int(1)

Deprecated: Non-canonical cast (double) is deprecated, use the (float) cast instead in %s(21) : eval()'d code on line 1
float(2.5)
done
--EXPECT_STDERR--
PHP Deprecated:  Non-canonical cast (integer) is deprecated, use the (int) cast instead in %s on line 9
PHP Deprecated:  Non-canonical cast (boolean) is deprecated, use the (bool) cast instead in %s on line 12
PHP Deprecated:  Non-canonical cast (double) is deprecated, use the (float) cast instead in %s on line 15
PHP Deprecated:  Non-canonical cast (integer) is deprecated, use the (int) cast instead in %s on line 18
PHP Deprecated:  Non-canonical cast (boolean) is deprecated, use the (bool) cast instead in %s on line 18
PHP Deprecated:  Non-canonical cast (double) is deprecated, use the (float) cast instead in %s on line 18
PHP Deprecated:  Non-canonical cast (binary) is deprecated, use the (string) cast instead in %s on line 18
PHP Deprecated:  Non-canonical cast (integer) is deprecated, use the (int) cast instead in %s on line 20
PHP Deprecated:  Non-canonical cast (integer) is deprecated, use the (int) cast instead in %s on line 20
PHP Deprecated:  Non-canonical cast (double) is deprecated, use the (float) cast instead in %s(21) : eval()'d code on line 1
--CLEAN--
<?php
