--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
filter_var FILTER_VALIDATE_FLOAT: min_range/max_range, the decimal + thousand options and their ValueErrors
--FILE--
<?php
/* The float filter's options were declared by php and read by nothing here:
 * min_range/max_range (which the INT filter has always honoured) were ignored,
 * so a bounded amount validated out of range; the "decimal" and "thousand"
 * options were ignored, so a program written for a comma decimal separator
 * accepted the dot form and refused its own; and both of php's option
 * ValueErrors were missing. */
function fo($label, $value, $options)
{
    try {
        $r = var_export(filter_var($value, FILTER_VALIDATE_FLOAT, $options), true);
    } catch (Throwable $e) {
        $r = get_class($e) . ': ' . $e->getMessage();
    }
    echo str_pad($label, 34), ' => ', $r, "\n";
}
$TH = ['flags' => FILTER_FLAG_ALLOW_THOUSAND];

// the range pair
fo('in range', '1.5', ['options' => ['min_range' => 1, 'max_range' => 2]]);
fo('below min', '0.5', ['options' => ['min_range' => 1]]);
fo('above max', '2.5', ['options' => ['max_range' => 2]]);
fo('min only, ok', '2.5', ['options' => ['min_range' => 1]]);
fo('reversed range', '5', ['options' => ['min_range' => 10, 'max_range' => 1]]);
fo('range from strings', '5', ['options' => ['min_range' => '3', 'max_range' => '8']]);

// "decimal": one byte, and only when the option IS a string
fo('decimal comma', '1,5', ['options' => ['decimal' => ',']]);
fo('decimal comma, dot input', '1.5', ['options' => ['decimal' => ',']]);
fo('decimal two bytes', '1.5', ['options' => ['decimal' => 'ab']]);
fo('decimal empty', '1.5', ['options' => ['decimal' => '']]);
fo('decimal not a string', '1.5', ['options' => ['decimal' => 120]]);

// "thousand": the separator SET, defaulting to apostrophe/comma/dot
fo('default set: apostrophe', "1'234.5", $TH);
fo('default set: comma', '1,234.5', $TH);
fo('default set: space', '1 234.5', $TH);
fo('own set: space', '1 234.5', ['flags' => FILTER_FLAG_ALLOW_THOUSAND, 'options' => ['thousand' => ' ']]);
fo('own set: space, comma input', '1,234.5', ['flags' => FILTER_FLAG_ALLOW_THOUSAND, 'options' => ['thousand' => ' ']]);
fo('thousand empty', '1234.5', ['flags' => FILTER_FLAG_ALLOW_THOUSAND, 'options' => ['thousand' => '']]);
fo('comma decimal + dot groups', '1.234,5', ['flags' => FILTER_FLAG_ALLOW_THOUSAND, 'options' => ['decimal' => ',']]);
fo('grouping is checked', '12,34.5', $TH);
fo('a group of one', "1'2'3", $TH);
fo('no flag, no separators', '1,234.5', ['options' => ['thousand' => ',']]);
fo('separator above 127', "1\xa0234.5", ['flags' => FILTER_FLAG_ALLOW_THOUSAND, 'options' => ['thousand' => "\xa0"]]);
fo('decimal above 127', "1\xa05", ['options' => ['decimal' => "\xa0"]]);

/* php answers a zero mantissa carrying a NON-ZERO exponent as an underflow, and
 * an integer-shaped literal through its long path, which has no signed zero. */
foreach (['0', '0.0', '0e0', '0e1', '0.e5', '-0', '-0.0', '-000', '-0e0'] as $z) {
    fo("zero [$z]", $z, 0);
}
?>
--EXPECT--
in range                           => 1.5
below min                          => false
above max                          => false
min only, ok                       => 2.5
reversed range                     => false
range from strings                 => 5.0
decimal comma                      => 1.5
decimal comma, dot input           => false
decimal two bytes                  => ValueError: filter_var(): "decimal" option must be one character long
decimal empty                      => ValueError: filter_var(): "decimal" option must be one character long
decimal not a string               => 1.5
default set: apostrophe            => 1234.5
default set: comma                 => 1234.5
default set: space                 => false
own set: space                     => 1234.5
own set: space, comma input        => false
thousand empty                     => ValueError: filter_var(): "thousand" option must not be empty
comma decimal + dot groups         => 1234.5
grouping is checked                => false
a group of one                     => false
no flag, no separators             => false
separator above 127                => 1234.5
decimal above 127                  => 1.5
zero [0]                           => 0.0
zero [0.0]                         => 0.0
zero [0e0]                         => 0.0
zero [0e1]                         => false
zero [0.e5]                        => false
zero [-0]                          => 0.0
zero [-0.0]                        => -0.0
zero [-000]                        => 0.0
zero [-0e0]                        => -0.0
--CLEAN--
<?php
unset($TH);
