--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A by-reference return with no variable to bind is reported at the return
--DESCRIPTION--
php has exactly one sentence here and raises it at the RETURN -- not at the call -- whether
the caller went on to take the answer by reference or by value. Falling off the end and a
bare `return;` count too. PHL had two sentences of its own naming PH7, plus the call site's
`Only variables should be assigned by reference`, which is about a callee that never
promised a reference and now stands down for one that did.
--FILE--
<?php
function &rbrnLit() {
    return 5;
}
$rbrnA = &rbrnLit();
var_dump($rbrnA);
function rbrnPlain() { return 5; }
function &rbrnCall() {
    return rbrnPlain();
}
var_dump(rbrnCall());
function &rbrnEmpty() {
}
var_dump(rbrnEmpty());
function rbrnNotRef() { return 5; }
$rbrnB = &rbrnNotRef();
var_dump($rbrnB);
?>
--EXPECTF--
PHP Notice:  Only variable references should be returned by reference in %s on line 3
int(5)
PHP Notice:  Only variable references should be returned by reference in %s on line 9
int(5)
PHP Notice:  Only variable references should be returned by reference in %s on line 13
NULL
PHP Notice:  Only variables should be assigned by reference in %s on line 16
int(5)
--CLEAN--
<?php
