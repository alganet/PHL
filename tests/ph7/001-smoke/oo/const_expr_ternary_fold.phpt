--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A constant expression's ternary is FOLDED before its rules apply, so the dropped branch is not screened
--DESCRIPTION--
php evaluates a constant condition at compile time and keeps only the branch it
selects, so an offender in the branch it drops never reaches the rules: `true ? 1
: new X` and `false ? new X : 1` are both legal, and so are the call, closure and
`static::` faces of the same shape. The screens ran over the raw token stream and
refused whatever they found anywhere in it, which turned valid php into a
compile-time fatal. A ternary NESTED in a bracket is folded the same way, and
where its condition begins is not something a token walk can say, so such a span
is left alone entirely rather than risk refusing the branch php keeps.
--FILE--
<?php
class CeTernX { public $v = 1; }
/* php CONSTANT-FOLDS a ternary before it applies the constant-expression rules,
 * and keeps only the branch the fold selects -- so an offender in the branch it
 * drops is not an offender at all. */
class CeTernFold {
    const T_NEW  = true  ? 1 : new CeTernX;
    const F_NEW  = false ? new CeTernX : 1;
    const T_CALL = true  ? 2 : strlen('a');
    const F_CALL = false ? strlen('a') : 2;
    const T_CLO  = true  ? 3 : function () {};
    const F_CLO  = false ? function () {} : 3;
    const T_ST   = true  ? 4 : static::K;
    const F_ST   = false ? static::K : 4;
    const ELVIS  = true ?: function () {};
    /* A ternary NESTED in a bracket is folded by php just the same. */
    const N_NEW  = [true ? 1 : new CeTernX];
    const N_CALL = [true ? 5 : strlen('a')];
    const N_CLO  = ['k' => false ? function () {} : 6];
    const N_PAREN = (true ? 7 : new CeTernX) + 1;
}
var_dump(CeTernFold::T_NEW, CeTernFold::F_NEW, CeTernFold::T_CALL, CeTernFold::F_CALL,
         CeTernFold::T_CLO, CeTernFold::F_CLO, CeTernFold::T_ST, CeTernFold::F_ST, CeTernFold::ELVIS);
var_dump(CeTernFold::N_NEW, CeTernFold::N_CALL, CeTernFold::N_CLO, CeTernFold::N_PAREN);
?>
--EXPECT--
int(1)
int(1)
int(2)
int(2)
int(3)
int(3)
int(4)
int(4)
bool(true)
array(1) {
  [0]=>
  int(1)
}
array(1) {
  [0]=>
  int(5)
}
array(1) {
  ["k"]=>
  int(6)
}
int(8)
--CLEAN--
<?php
