--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
compact() says which name it dropped, and why
--DESCRIPTION--
compact() takes names, not values, so its parameters are untyped and no signature
screen can speak for them — php checks each argument in the builtin's own body
and warns twice over: `compact(): Argument #N must be string or array of strings,
TYPE given` for a name that is neither, and `compact(): Undefined variable $name`
for a string naming a variable the frame does not have. Both are E_WARNING and
both skip just that entry. PHL emitted neither, so compact('typo') and compact(1)
answered a shorter array in silence — the caller's only clue that a name had been
dropped was the array's own size. The argument NUMBER is php's even for an
element found inside a nested array: php names the argument the element came
from, never the element's own position.
--FILE--
<?php
set_error_handler(function ($n, $s) { echo '  [', $n, '] ', $s, "\n"; return true; });
$cmpA = 1;
$cmpB = 2;
function cmpShow($x) { echo str_replace("\n", '', var_export($x, true)), "\n"; }

cmpShow(compact('cmpA', 'cmpB'));
cmpShow(compact('cmpA', 'cmpZZ'));
cmpShow(compact(1));
cmpShow(compact(['cmpA', 2, ['cmpB']]));
cmpShow(compact(null));
cmpShow(compact(true));
cmpShow(compact(1.5));
cmpShow(compact('cmpA', new stdClass));
cmpShow(compact(''));
cmpShow(compact([['cmpA'], ['cmpZZ']]));
cmpShow(compact('cmpA', 3, 'cmpB'));
cmpShow(compact([]));
--EXPECT--
array (  'cmpA' => 1,  'cmpB' => 2,)
  [2] compact(): Undefined variable $cmpZZ
array (  'cmpA' => 1,)
  [2] compact(): Argument #1 must be string or array of strings, int given
array ()
  [2] compact(): Argument #1 must be string or array of strings, int given
array (  'cmpA' => 1,  'cmpB' => 2,)
  [2] compact(): Argument #1 must be string or array of strings, null given
array ()
  [2] compact(): Argument #1 must be string or array of strings, true given
array ()
  [2] compact(): Argument #1 must be string or array of strings, float given
array ()
  [2] compact(): Argument #2 must be string or array of strings, stdClass given
array (  'cmpA' => 1,)
  [2] compact(): Undefined variable $
array ()
  [2] compact(): Undefined variable $cmpZZ
array (  'cmpA' => 1,)
  [2] compact(): Argument #2 must be string or array of strings, int given
array (  'cmpA' => 1,  'cmpB' => 2,)
array ()
