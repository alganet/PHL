--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
filter_var_array / filter_input_array / filter_has_var / filter_list / filter_id
--FILE--
<?php
/* php has seven filter functions and this engine had two. The five below are
 * the ones a request handler actually calls: one definition array describing
 * every field, applied in one call, with the per-field filter, flags and
 * options each field wants. */
function sh($label, $v)
{
    echo str_pad($label, 22), ' ', str_replace(["\n", '  '], '', var_export($v, true)), "\n";
}
$data = ['age' => '25', 'name' => 'bob', 'email' => 'a@b.com', 'list' => ['1', '2'],
         'empty' => '', 'n' => null];

sh('no definition', filter_var_array($data));
sh('one filter', filter_var_array($data, FILTER_VALIDATE_INT));
sh('per key', filter_var_array($data, ['age' => FILTER_VALIDATE_INT,
                                       'email' => FILTER_VALIDATE_EMAIL,
                                       'missing' => FILTER_VALIDATE_INT]));
sh('add_empty off', filter_var_array($data, ['age' => FILTER_VALIDATE_INT, 'missing' => FILTER_VALIDATE_INT], false));
sh('key options', filter_var_array($data, ['age' => ['filter' => FILTER_VALIDATE_INT,
                                                     'options' => ['min_range' => 30]]]));
sh('key default', filter_var_array($data, ['age' => ['filter' => FILTER_VALIDATE_INT,
                                                     'options' => ['default' => 7, 'min_range' => 30]]]));
sh('key require_array', filter_var_array($data, ['list' => ['filter' => FILTER_VALIDATE_INT,
                                                            'flags' => FILTER_REQUIRE_ARRAY]]));
sh('key force_array', filter_var_array($data, ['age' => ['filter' => FILTER_VALIDATE_INT,
                                                         'flags' => FILTER_FORCE_ARRAY]]));
sh('key null_on_failure', filter_var_array(['a' => 'x'], ['a' => ['filter' => FILTER_VALIDATE_INT,
                                                                  'flags' => FILTER_NULL_ON_FAILURE]]));
sh('empty input', filter_var_array([], FILTER_VALIDATE_INT));
sh('nested input', filter_var_array(['a' => ['b' => '1']], FILTER_VALIDATE_INT));
sh('null member', filter_var_array($data, ['n' => FILTER_VALIDATE_INT]));

// the argument contracts
foreach ([fn() => filter_var_array(['a' => '1'], [0 => FILTER_VALIDATE_INT]),
          fn() => filter_var_array(['a' => '1'], ['' => FILTER_VALIDATE_INT]),
          fn() => filter_var_array('notarray')] as $i => $f) {
    try { $f(); } catch (Throwable $e) { echo 'err', $i, ': ', get_class($e), ': ', $e->getMessage(), "\n"; }
}
// an unknown id warns and falls back to the default filter INSIDE a definition,
// and answers false from filter_var() itself
sh('unknown in def', @filter_var_array(['a' => '1'], ['a' => 99999]));
sh('unknown alone', @filter_var('x', 99999));

// the INPUT_* half
sh('has_var missing', filter_has_var(INPUT_GET, 'nope'));
sh('input_array empty', filter_input_array(INPUT_GET));
foreach ([fn() => filter_has_var(9, 'x'), fn() => filter_input_array(9, [])] as $i => $f) {
    try { $f(); } catch (Throwable $e) { echo 'src', $i, ': ', get_class($e), ': ', $e->getMessage(), "\n"; }
}

// the name/id table
sh('filter_list', filter_list());
foreach (filter_list() as $n) { echo $n, '=', filter_id($n), ' '; }
echo "\n";
sh('unknown names', [filter_id('nosuch'), filter_id(''), filter_id('INT')]);
?>
--EXPECT--
no definition          array ('age' => '25','name' => 'bob','email' => 'a@b.com','list' => array (0 => '1',1 => '2',),'empty' => '','n' => '',)
one filter             array ('age' => 25,'name' => false,'email' => false,'list' => array (0 => 1,1 => 2,),'empty' => false,'n' => false,)
per key                array ('age' => 25,'email' => 'a@b.com','missing' => NULL,)
add_empty off          array ('age' => 25,)
key options            array ('age' => false,)
key default            array ('age' => 7,)
key require_array      array ('list' => array (0 => 1,1 => 2,),)
key force_array        array ('age' => array (0 => 25,),)
key null_on_failure    array ('a' => NULL,)
empty input            array ()
nested input           array ('a' => array ('b' => 1,),)
null member            array ('n' => false,)
err0: TypeError: filter_var_array(): Argument #2 ($options) must contain only string keys
err1: ValueError: filter_var_array(): Argument #2 ($options) cannot contain empty keys
err2: TypeError: filter_var_array(): Argument #1 ($array) must be of type array, string given
unknown in def         array ('a' => '1',)
unknown alone          false
has_var missing        false
input_array empty      NULL
src0: ValueError: filter_has_var(): Argument #1 ($input_type) must be an INPUT_* constant
src1: ValueError: filter_input_array(): Argument #1 ($type) must be an INPUT_* constant
filter_list            array (0 => 'int',1 => 'boolean',2 => 'float',3 => 'validate_regexp',4 => 'validate_domain',5 => 'validate_url',6 => 'validate_email',7 => 'validate_ip',8 => 'validate_mac',9 => 'string',10 => 'stripped',11 => 'encoded',12 => 'special_chars',13 => 'full_special_chars',14 => 'unsafe_raw',15 => 'email',16 => 'url',17 => 'number_int',18 => 'number_float',19 => 'add_slashes',20 => 'callback',)
int=257 boolean=258 float=259 validate_regexp=272 validate_domain=277 validate_url=273 validate_email=274 validate_ip=275 validate_mac=276 string=513 stripped=513 encoded=514 special_chars=515 full_special_chars=522 unsafe_raw=516 email=517 url=518 number_int=519 number_float=520 add_slashes=523 callback=1024 
unknown names          array (0 => false,1 => false,2 => false,)
--CLEAN--
<?php
unset($data);
