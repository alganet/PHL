--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
filter_var: FILTER_CALLBACK, FILTER_THROW_ON_FAILURE with Filter\FilterFailedException, and the $options type
--FILE--
<?php
/* Two of php's filter surfaces were missing entirely. FILTER_CALLBACK is the
 * escape hatch — the caller's own function decides — and php 8.5's
 * FILTER_THROW_ON_FAILURE turns a false answer into an exception, which is what
 * lets a validation failure travel like every other error instead of being
 * tested for at each call. */
function cb($label, $fn)
{
    try { $r = str_replace("\n", '', var_export($fn(), true)); }
    catch (Throwable $e) { $r = get_class($e) . ': ' . $e->getMessage(); }
    echo str_pad($label, 26), ' => ', $r, "\n";
}
class CbHelper
{
    public static function up($v) { return strtoupper($v); }
    public function twice($v) { return $v . $v; }
}
cb('closure', fn() => filter_var('5', FILTER_CALLBACK, ['options' => fn($v) => $v * 2]));
cb('function name', fn() => filter_var('ab', FILTER_CALLBACK, ['options' => 'strtoupper']));
cb('static pair', fn() => filter_var('ab', FILTER_CALLBACK, ['options' => ['CbHelper', 'up']]));
cb('object pair', fn() => filter_var('ab', FILTER_CALLBACK, ['options' => [new CbHelper, 'twice']]));
cb('Class::method', fn() => filter_var('ab', FILTER_CALLBACK, ['options' => 'CbHelper::up']));
cb('answers null', fn() => filter_var('5', FILTER_CALLBACK, ['options' => fn($v) => null]));
cb('answers an array', fn() => filter_var('5', FILTER_CALLBACK, ['options' => fn($v) => [1, 2]]));
cb('array input', fn() => filter_var(['a', 'b'], FILTER_CALLBACK, ['options' => 'strtoupper']));
cb('no option', fn() => filter_var('5', FILTER_CALLBACK));
cb('not callable', fn() => filter_var('5', FILTER_CALLBACK, ['options' => 'nosuchfn']));
cb('option is an int', fn() => filter_var('5', FILTER_CALLBACK, ['options' => 5]));
cb('flat options', fn() => filter_var('5', FILTER_CALLBACK, 'strtoupper'));
cb('callback throws', fn() => filter_var('5', FILTER_CALLBACK,
    ['options' => function ($v) { throw new RuntimeException('boom'); }]));

// FILTER_THROW_ON_FAILURE names the filter and the value
foreach ([[FILTER_VALIDATE_INT, 'x'], [FILTER_VALIDATE_IP, 'zz'], [FILTER_VALIDATE_EMAIL, 'zz'],
          [FILTER_VALIDATE_BOOLEAN, 'zz'], [FILTER_VALIDATE_FLOAT, 'zz'], [FILTER_VALIDATE_MAC, 'zz'],
          [FILTER_VALIDATE_URL, 'zz']] as [$f, $v]) {
    cb('throw ' . $f, fn() => filter_var($v, $f, FILTER_THROW_ON_FAILURE));
}
cb('throw: passes', fn() => filter_var('5', FILTER_VALIDATE_INT, FILTER_THROW_ON_FAILURE));
cb('throw over default', fn() => filter_var('x', FILTER_VALIDATE_INT,
    ['flags' => FILTER_THROW_ON_FAILURE, 'options' => ['default' => 9]]));
cb('throw with null flag', fn() => filter_var('x', FILTER_VALIDATE_INT,
    FILTER_THROW_ON_FAILURE | FILTER_NULL_ON_FAILURE));
cb('throw: a sanitizer', fn() => filter_var('x', FILTER_SANITIZE_NUMBER_INT, FILTER_THROW_ON_FAILURE));
cb('throw inside an array', fn() => filter_var(['1', 'x'], FILTER_VALIDATE_INT,
    FILTER_REQUIRE_ARRAY | FILTER_THROW_ON_FAILURE));
cb('throw on boolean false', fn() => filter_var('no', FILTER_VALIDATE_BOOLEAN, FILTER_THROW_ON_FAILURE));

// the exception hierarchy a caller catches
try {
    filter_var('x', FILTER_VALIDATE_INT, FILTER_THROW_ON_FAILURE);
} catch (\Filter\FilterException $e) {
    echo get_class($e), ' | ', implode(',', class_parents($e)), "\n";
}

// $options is `array|int` and php's ZPP says so
cb('options: a string', fn() => filter_var('5', FILTER_VALIDATE_INT, 'str'));
cb('options: numeric string', fn() => filter_var('5', FILTER_VALIDATE_INT, '0'));
cb('options: a float', fn() => filter_var('5', FILTER_VALIDATE_INT, 1.0));
cb('options: fva string', fn() => filter_var_array(['a' => '1'], 'str'));
cb('options: fia string', fn() => filter_input_array(INPUT_GET, 'str'));
?>
--EXPECT--
closure                    => 10
function name              => 'AB'
static pair                => 'AB'
object pair                => 'abab'
Class::method              => 'AB'
answers null               => NULL
answers an array           => array (  0 => 1,  1 => 2,)
array input                => array (  0 => 'A',  1 => 'B',)
no option                  => TypeError: filter_var(): Option must be a valid callback
not callable               => TypeError: filter_var(): Option must be a valid callback
option is an int           => TypeError: filter_var(): Option must be a valid callback
flat options               => TypeError: filter_var(): Argument #3 ($options) must be of type array|int, string given
callback throws            => RuntimeException: boom
throw 257                  => Filter\FilterFailedException: filter validation failed: filter int not satisfied by 'x'
throw 275                  => Filter\FilterFailedException: filter validation failed: filter validate_ip not satisfied by 'zz'
throw 274                  => Filter\FilterFailedException: filter validation failed: filter validate_email not satisfied by 'zz'
throw 258                  => Filter\FilterFailedException: filter validation failed: filter boolean not satisfied by 'zz'
throw 259                  => Filter\FilterFailedException: filter validation failed: filter float not satisfied by 'zz'
throw 276                  => Filter\FilterFailedException: filter validation failed: filter validate_mac not satisfied by 'zz'
throw 273                  => Filter\FilterFailedException: filter validation failed: filter validate_url not satisfied by 'zz'
throw: passes              => 5
throw over default         => Filter\FilterFailedException: filter validation failed: filter int not satisfied by 'x'
throw with null flag       => ValueError: filter_var(): Argument #3 ($options) cannot use both FILTER_NULL_ON_FAILURE and FILTER_THROW_ON_FAILURE
throw: a sanitizer         => ''
throw inside an array      => Filter\FilterFailedException: filter validation failed: filter int not satisfied by 'x'
throw on boolean false     => false
Filter\FilterFailedException | Filter\FilterException,Exception
options: a string          => TypeError: filter_var(): Argument #3 ($options) must be of type array|int, string given
options: numeric string    => 5
options: a float           => 5
options: fva string        => TypeError: filter_var_array(): Argument #2 ($options) must be of type array|int, string given
options: fia string        => TypeError: filter_input_array(): Argument #2 ($options) must be of type array|int, string given
--CLEAN--
<?php
