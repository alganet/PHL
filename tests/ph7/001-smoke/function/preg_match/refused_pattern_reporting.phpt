--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Refuse a pattern under the name the caller wrote, and answer php's value
--DESCRIPTION--
Found sweeping every preg_* door after the pattern-STRING parser closed. All of
them share one compile, and what each SAYS and RETURNS when that compile is
refused was its own answer here.

Three of them were written as embedded PHP over another builtin, so the
diagnostic wore the delegate's name: `preg_filter` said `preg_replace():`,
`preg_replace_callback_array` said `preg_replace_callback():`, and `preg_grep`
said `preg_match():` -- once PER ELEMENT, and then answered an ARRAY where php
answers FALSE, so a caller testing `=== false` saw a successful filter that had
simply matched nothing. With an empty array preg_grep never compiled the pattern
at all and said nothing. The three are C now, sharing the very bodies php shares
(preg_filter IS preg_replace with a flag), which is what puts the caller's own
name on the refusal.

`preg_match_all` answered `int(0)` rather than `false`, against its own declared
`int|false` -- the same "no matches" a good pattern gives. With an ARRAY subject
php runs EVERY subject, so a refusal is reported once per subject and the answer
is the empty array; this bailed at the first one. And `preg_last_error()` was
touched by an argument php refuses before any pattern runs, while a callback that
THREW -- which php records as PREG_INTERNAL_ERROR -- left it saying "No error".

Beside them the callback ARGUMENT: php screens it and names the fault
(`function "f" not found or invalid function name`, `no array or string given`,
`class "x" not found`, `array callback must have exactly two members`) as a
catchable TypeError; PHL answered one warning for all four and carried on with
NULL. `preg_replace_callback_array` validates each entry only when it REACHES
it, so the first replacement has already run when the second entry's key or
callback is refused.
--FILE--
<?php
$GLOBALS['pcrHits'] = [];
function pcrHit($t) { $GLOBALS['pcrHits'][] = $t; return '<' . $t . '>'; }
function pcrRun($label, $fn) {
    $GLOBALS['pcrHits'] = [];
    echo '  ', str_pad($label, 20), ' -> ';
    try { echo json_encode($fn()); } catch (Throwable $e) { echo 'EX ', get_class($e), ': ', $e->getMessage(); }
    if ($GLOBALS['pcrHits']) { echo ' hits=', json_encode($GLOBALS['pcrHits']); }
    echo "\n  last=", preg_last_error(), " '", preg_last_error_msg(), "'\n";
}
set_error_handler(function ($no, $str) { echo '  W(', $no, '): ', $str, "\n"; return true; });

$BAD = '/a/Q';   /* refused by the modifier screen */
$OK  = '/a/';

echo "-- a refused pattern wears the name of the function the caller wrote\n";
pcrRun('match',      fn() => preg_match($BAD, 'aa'));
pcrRun('match_all',  fn() => preg_match_all($BAD, 'aa'));
pcrRun('replace',    fn() => preg_replace($BAD, 'X', 'aa'));
pcrRun('filter',     fn() => preg_filter($BAD, 'X', 'aa'));
pcrRun('rcb',        fn() => preg_replace_callback($BAD, fn($m) => 'X', 'aa'));
pcrRun('rcba',       fn() => preg_replace_callback_array([$BAD => fn($m) => 'X'], 'aa'));
pcrRun('split',      fn() => preg_split($BAD, 'XaY'));
pcrRun('grep',       fn() => preg_grep($BAD, ['aa', 'b']));
pcrRun('grep_empty', fn() => preg_grep($BAD, []));
pcrRun('grep_invert',fn() => preg_grep($BAD, ['aa', 'b'], PREG_GREP_INVERT));
pcrRun('filter_var', fn() => filter_var('aa', FILTER_VALIDATE_REGEXP, ['options' => ['regexp' => $BAD]]));

echo "-- with an ARRAY subject php runs every subject, so it reports the refusal per subject\n";
pcrRun('replace_arr', function () use ($BAD) { $c = 'pre'; $r = preg_replace($BAD, 'X', ['aa','bb'], -1, $c); return [$r, $c]; });
pcrRun('filter_arr',  function () use ($BAD) { $c = 'pre'; $r = preg_filter($BAD, 'X', ['aa','bb'], -1, $c); return [$r, $c]; });
pcrRun('rcb_arr',     function () use ($BAD) { $c = 'pre'; $r = preg_replace_callback($BAD, fn($m) => 'X', ['aa','bb'], -1, $c); return [$r, $c]; });
pcrRun('rcba_arr',    function () use ($BAD) { $c = 'pre'; $r = preg_replace_callback_array([$BAD => fn($m) => 'X'], ['aa'], -1, $c); return [$r, $c]; });
pcrRun('rcba_scalar', function () use ($BAD) { $c = 'pre'; $r = preg_replace_callback_array([$BAD => fn($m) => 'X'], 'aa', -1, $c); return [$r, $c]; });
pcrRun('replace_pats',fn() => preg_replace([$OK, $BAD], ['X','Y'], ['ab','ab']));

echo "-- preg_filter keeps only what changed; preg_replace keeps everything\n";
pcrRun('filter_scalar',  function () { $c = 'pre'; $r = preg_filter('/a/', 'X', 'aa', -1, $c); return [$r, $c]; });
pcrRun('filter_nomatch', function () { $c = 'pre'; $r = preg_filter('/z/', 'X', 'aa', -1, $c); return [$r, $c]; });
pcrRun('filter_keys',    function () { $c = 'pre'; $r = preg_filter('/a/', 'X', ['k'=>'aa', 3=>'bb', 'z'=>'ca'], -1, $c); return [$r, $c]; });
pcrRun('replace_keys',   function () { $c = 'pre'; $r = preg_replace('/a/', 'X', ['k'=>'aa', 3=>'bb', 'z'=>'ca'], -1, $c); return [$r, $c]; });
pcrRun('filter_limit',   function () { $c = 'pre'; $r = preg_filter('/a/', 'X', 'aaa', 2, $c); return [$r, $c]; });

echo "-- preg_grep keeps the ELEMENT, matches on its string form\n";
pcrRun('grep_keys',   fn() => preg_grep('/a/', ['x'=>'aa', 7=>'b', 8=>'a']));
pcrRun('grep_scalars',fn() => preg_grep('/1/', [1, 2.5, true, null, '1']));
pcrRun('grep_invert2',fn() => preg_grep('/a/', ['aa','b'], PREG_GREP_INVERT));
pcrRun('grep_none',   fn() => preg_grep('/z/', ['aa','b']));

echo "-- preg_replace_callback_array validates each entry only when it reaches it\n";
pcrRun('rcba_chain',  function () { $c = 'pre'; $r = preg_replace_callback_array(['/a/' => fn($m) => pcrHit('a'), '/b/' => fn($m) => pcrHit('b')], 'ab', -1, $c); return [$r, $c]; });
pcrRun('rcba_lazy_cb',fn() => preg_replace_callback_array(['/a/' => fn($m) => pcrHit('a'), '/b/' => 'nosuchfn'], 'ab'));
pcrRun('rcba_int_key',fn() => preg_replace_callback_array(['/a/' => fn($m) => pcrHit('a'), 5 => fn($m) => pcrHit('5')], 'ab5'));
pcrRun('rcba_lazy_pat',function () use ($BAD) { $c = 'pre'; $r = preg_replace_callback_array(['/a/' => fn($m) => pcrHit('a'), $BAD => fn($m) => pcrHit('b')], 'ab', -1, $c); return [$r, $c]; });
pcrRun('rcba_empty',  function () { $c = 'pre'; $r = preg_replace_callback_array([], 'ab', -1, $c); return [$r, $c]; });
pcrRun('rcba_flags',  function () { $c = 'pre'; $r = preg_replace_callback_array(['/(a)/' => fn($m) => json_encode($m)], 'a', -1, $c, PREG_OFFSET_CAPTURE); return [$r, $c]; });

echo "-- php screens \$callback as an argument, and says what is wrong with it\n";
pcrRun('cb_nofn',  fn() => preg_replace_callback('/a/', 'nosuchfn', 'a'));
pcrRun('cb_null',  fn() => preg_replace_callback('/a/', null, 'a'));
pcrRun('cb_int',   fn() => preg_replace_callback('/a/', 5, 'a'));
pcrRun('cb_class', fn() => preg_replace_callback('/a/', ['nosuchclass','m'], 'a'));
pcrRun('cb_one',   fn() => preg_replace_callback('/a/', [1], 'a'));
pcrRun('cb_before',fn() => preg_replace_callback($BAD, 'nosuchfn', 'a'));

echo "-- a callback that THROWS leaves php's internal-error code behind\n";
pcrRun('cb_throw',  fn() => preg_replace_callback('/a/', function ($m) { throw new RuntimeException('boom'); }, 'a'));
pcrRun('rcba_throw',fn() => preg_replace_callback_array(['/a/' => function ($m) { throw new RuntimeException('boom'); }], 'a'));
pcrRun('after_ok',  fn() => preg_match('/a/', 'a'));

echo "-- the ZPP faces, and the surface Reflection reports\n";
pcrRun('grep_badarr', fn() => preg_grep('/a/', 'notarray'));
pcrRun('grep_badpat', fn() => preg_grep([], ['a']));
pcrRun('grep_arity',  fn() => preg_grep('/a/'));
pcrRun('filter_arity',fn() => preg_filter('/a/', 'X'));
pcrRun('filter_pair', fn() => preg_filter('/a/', ['X'], 'aa'));
pcrRun('rcba_badpat', fn() => preg_replace_callback_array('nope', 'aa'));
pcrRun('rcba_arity',  fn() => preg_replace_callback_array(['/a/' => fn($m) => 'X']));
foreach (['preg_grep', 'preg_filter', 'preg_replace_callback_array'] as $pcrFn) {
    $r = new ReflectionFunction($pcrFn);
    echo '  ', $pcrFn, ' internal=', var_export($r->isInternal(), true), "\n";
    echo '  ', (string)$r, "\n";
}
restore_error_handler();
--EXPECT--
-- a refused pattern wears the name of the function the caller wrote
  match                ->   W(2): preg_match(): Unknown modifier 'Q'
false
  last=1 'Internal error'
  match_all            ->   W(2): preg_match_all(): Unknown modifier 'Q'
false
  last=1 'Internal error'
  replace              ->   W(2): preg_replace(): Unknown modifier 'Q'
null
  last=1 'Internal error'
  filter               ->   W(2): preg_filter(): Unknown modifier 'Q'
null
  last=1 'Internal error'
  rcb                  ->   W(2): preg_replace_callback(): Unknown modifier 'Q'
null
  last=1 'Internal error'
  rcba                 ->   W(2): preg_replace_callback_array(): Unknown modifier 'Q'
null
  last=1 'Internal error'
  split                ->   W(2): preg_split(): Unknown modifier 'Q'
false
  last=1 'Internal error'
  grep                 ->   W(2): preg_grep(): Unknown modifier 'Q'
false
  last=1 'Internal error'
  grep_empty           ->   W(2): preg_grep(): Unknown modifier 'Q'
false
  last=1 'Internal error'
  grep_invert          ->   W(2): preg_grep(): Unknown modifier 'Q'
false
  last=1 'Internal error'
  filter_var           ->   W(2): filter_var(): Unknown modifier 'Q'
false
  last=1 'Internal error'
-- with an ARRAY subject php runs every subject, so it reports the refusal per subject
  replace_arr          ->   W(2): preg_replace(): Unknown modifier 'Q'
  W(2): preg_replace(): Unknown modifier 'Q'
[[],0]
  last=1 'Internal error'
  filter_arr           ->   W(2): preg_filter(): Unknown modifier 'Q'
  W(2): preg_filter(): Unknown modifier 'Q'
[[],0]
  last=1 'Internal error'
  rcb_arr              ->   W(2): preg_replace_callback(): Unknown modifier 'Q'
  W(2): preg_replace_callback(): Unknown modifier 'Q'
[[],0]
  last=1 'Internal error'
  rcba_arr             ->   W(2): preg_replace_callback_array(): Unknown modifier 'Q'
[[],0]
  last=1 'Internal error'
  rcba_scalar          ->   W(2): preg_replace_callback_array(): Unknown modifier 'Q'
[null,"pre"]
  last=1 'Internal error'
  replace_pats         ->   W(2): preg_replace(): Unknown modifier 'Q'
  W(2): preg_replace(): Unknown modifier 'Q'
[]
  last=1 'Internal error'
-- preg_filter keeps only what changed; preg_replace keeps everything
  filter_scalar        -> ["XX",2]
  last=0 'No error'
  filter_nomatch       -> [null,0]
  last=0 'No error'
  filter_keys          -> [{"k":"XX","z":"cX"},3]
  last=0 'No error'
  replace_keys         -> [{"k":"XX","3":"bb","z":"cX"},3]
  last=0 'No error'
  filter_limit         -> ["XXa",2]
  last=0 'No error'
-- preg_grep keeps the ELEMENT, matches on its string form
  grep_keys            -> {"x":"aa","8":"a"}
  last=0 'No error'
  grep_scalars         -> {"0":1,"2":true,"4":"1"}
  last=0 'No error'
  grep_invert2         -> {"1":"b"}
  last=0 'No error'
  grep_none            -> []
  last=0 'No error'
-- preg_replace_callback_array validates each entry only when it reaches it
  rcba_chain           -> ["<a><b>",2] hits=["a","b"]
  last=0 'No error'
  rcba_lazy_cb         -> EX TypeError: preg_replace_callback_array(): Argument #1 ($pattern) must contain only valid callbacks hits=["a"]
  last=0 'No error'
  rcba_int_key         -> EX TypeError: preg_replace_callback_array(): Argument #1 ($pattern) must contain only string patterns as keys hits=["a"]
  last=0 'No error'
  rcba_lazy_pat        ->   W(2): preg_replace_callback_array(): Unknown modifier 'Q'
[null,"pre"] hits=["a"]
  last=1 'Internal error'
  rcba_empty           -> ["ab",0]
  last=1 'Internal error'
  rcba_flags           -> ["[[\"a\",0],[\"a\",0]]",1]
  last=0 'No error'
-- php screens $callback as an argument, and says what is wrong with it
  cb_nofn              -> EX TypeError: preg_replace_callback(): Argument #2 ($callback) must be a valid callback, function "nosuchfn" not found or invalid function name
  last=0 'No error'
  cb_null              -> EX TypeError: preg_replace_callback(): Argument #2 ($callback) must be a valid callback, no array or string given
  last=0 'No error'
  cb_int               -> EX TypeError: preg_replace_callback(): Argument #2 ($callback) must be a valid callback, no array or string given
  last=0 'No error'
  cb_class             -> EX TypeError: preg_replace_callback(): Argument #2 ($callback) must be a valid callback, class "nosuchclass" not found
  last=0 'No error'
  cb_one               -> EX TypeError: preg_replace_callback(): Argument #2 ($callback) must be a valid callback, array callback must have exactly two members
  last=0 'No error'
  cb_before            -> EX TypeError: preg_replace_callback(): Argument #2 ($callback) must be a valid callback, function "nosuchfn" not found or invalid function name
  last=0 'No error'
-- a callback that THROWS leaves php's internal-error code behind
  cb_throw             -> EX RuntimeException: boom
  last=1 'Internal error'
  rcba_throw           -> EX RuntimeException: boom
  last=1 'Internal error'
  after_ok             -> 1
  last=0 'No error'
-- the ZPP faces, and the surface Reflection reports
  grep_badarr          -> EX TypeError: preg_grep(): Argument #2 ($array) must be of type array, string given
  last=0 'No error'
  grep_badpat          -> EX TypeError: preg_grep(): Argument #1 ($pattern) must be of type string, array given
  last=0 'No error'
  grep_arity           -> EX ArgumentCountError: preg_grep() expects at least 2 arguments, 1 given
  last=0 'No error'
  filter_arity         -> EX ArgumentCountError: preg_filter() expects at least 3 arguments, 2 given
  last=0 'No error'
  filter_pair          -> EX TypeError: preg_filter(): Argument #1 ($pattern) must be of type array when argument #2 ($replacement) is an array, string given
  last=0 'No error'
  rcba_badpat          -> EX TypeError: preg_replace_callback_array(): Argument #1 ($pattern) must be of type array, string given
  last=0 'No error'
  rcba_arity           -> EX ArgumentCountError: preg_replace_callback_array() expects at least 2 arguments, 1 given
  last=0 'No error'
  preg_grep internal=true
  Function [ <internal:pcre> function preg_grep ] {

  - Parameters [3] {
    Parameter #0 [ <required> string $pattern ]
    Parameter #1 [ <required> array $array ]
    Parameter #2 [ <optional> int $flags = 0 ]
  }
  - Return [ array|false ]
}

  preg_filter internal=true
  Function [ <internal:pcre> function preg_filter ] {

  - Parameters [5] {
    Parameter #0 [ <required> array|string $pattern ]
    Parameter #1 [ <required> array|string $replacement ]
    Parameter #2 [ <required> array|string $subject ]
    Parameter #3 [ <optional> int $limit = -1 ]
    Parameter #4 [ <optional> &$count = null ]
  }
  - Return [ array|string|null ]
}

  preg_replace_callback_array internal=true
  Function [ <internal:pcre> function preg_replace_callback_array ] {

  - Parameters [5] {
    Parameter #0 [ <required> array $pattern ]
    Parameter #1 [ <required> array|string $subject ]
    Parameter #2 [ <optional> int $limit = -1 ]
    Parameter #3 [ <optional> &$count = null ]
    Parameter #4 [ <optional> int $flags = 0 ]
  }
  - Return [ array|string|null ]
}

