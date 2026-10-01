--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A name php has no function for stays declarable, whichever table the engine keeps it in
--FILE--
<?php
/* php's redeclaration fatal means "this name is already taken", and it is only
 * taken in a program php would refuse. This engine carries internal names php
 * has none of, and every one of them is a name real code declares -- size_format()
 * is WordPress's, each() the classic php-5 polyfill. Refusing those would break
 * programs php runs. Both declaration doors are covered -- the top-level one the
 * compiler binds, and the conditional one bound where the statement sits. */
function size_format($n) { return "user size_format"; }
function each($a) { return "user each"; }
echo size_format(1), "\n";
echo each([1]), "\n";
if (true) {
    function strglob($p) { return "user strglob"; }
}
echo strglob("*"), "\n";
--EXPECT--
user size_format
user each
user strglob
--CLEAN--
<?php
