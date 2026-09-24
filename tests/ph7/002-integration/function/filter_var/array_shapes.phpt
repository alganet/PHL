--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
filter_var: FILTER_REQUIRE_ARRAY / FILTER_REQUIRE_SCALAR / FILTER_FORCE_ARRAY and the recursive walk
--FILE--
<?php
/* An array input failed every filter here, and the three flags that say what
 * SHAPE is expected were undefined constants — so the documented way to
 * validate `$_POST['ids']` (`filter_var($ids, FILTER_VALIDATE_INT,
 * FILTER_REQUIRE_ARRAY)`) was a fatal, and without the flags an array was
 * silently false. php walks the array, keeps the KEYS, recurses into nested
 * arrays, and answers per-element failures in place. */
function fa($label, $value, $filter, $opts = 0)
{
    printf("%-30s => %s\n", $label, json_encode(filter_var($value, $filter, $opts)));
}
$list = ['1', '2'];
$mixed = ['a' => '1', 'b' => 'x'];
$nested = [['1', 'z'], ['3']];

fa('array, no flags', $list, FILTER_VALIDATE_INT);
fa('array, require_array', $list, FILTER_VALIDATE_INT, FILTER_REQUIRE_ARRAY);
fa('array, keys kept', $mixed, FILTER_VALIDATE_INT, FILTER_REQUIRE_ARRAY);
fa('array, nested', $nested, FILTER_VALIDATE_INT, FILTER_REQUIRE_ARRAY);
fa('array, force_array', $list, FILTER_VALIDATE_INT, FILTER_FORCE_ARRAY);
fa('array, require_scalar', $list, FILTER_VALIDATE_INT, FILTER_REQUIRE_SCALAR);
fa('array, null_on_failure', ['x'], FILTER_VALIDATE_INT, FILTER_REQUIRE_ARRAY | FILTER_NULL_ON_FAILURE);
fa('array, scalar+null', $list, FILTER_VALIDATE_INT, FILTER_REQUIRE_SCALAR | FILTER_NULL_ON_FAILURE);
fa('empty array', [], FILTER_VALIDATE_INT, FILTER_REQUIRE_ARRAY);
fa('array, default filter', ['a', 'b'], FILTER_DEFAULT, FILTER_REQUIRE_ARRAY);

fa('scalar, no flags', '5', FILTER_VALIDATE_INT);
fa('scalar, require_array', '5', FILTER_VALIDATE_INT, FILTER_REQUIRE_ARRAY);
fa('scalar, require_array+null', '5', FILTER_VALIDATE_INT, FILTER_REQUIRE_ARRAY | FILTER_NULL_ON_FAILURE);
fa('scalar, force_array', '5', FILTER_VALIDATE_INT, FILTER_FORCE_ARRAY);
fa('scalar, force_array bad', 'x', FILTER_VALIDATE_INT, FILTER_FORCE_ARRAY);
fa('scalar, require_scalar', '5', FILTER_VALIDATE_INT, FILTER_REQUIRE_SCALAR);

// the same flags through the options-array spelling, where they live under 'flags'
fa('opts: require_array', $list, FILTER_VALIDATE_INT, ['flags' => FILTER_REQUIRE_ARRAY]);
fa('opts: array + min_range', $list, FILTER_VALIDATE_INT,
   ['flags' => FILTER_REQUIRE_ARRAY, 'options' => ['min_range' => 2]]);
fa('opts: force_array', '7', FILTER_VALIDATE_INT, ['flags' => FILTER_FORCE_ARRAY]);
fa('opts: filter key wins', '7', FILTER_DEFAULT, ['filter' => FILTER_VALIDATE_INT]);
fa('opts: array, no flags key', $list, FILTER_VALIDATE_INT, ['options' => ['min_range' => 0]]);

/* php replaces whatever the filter ANSWERED false with the default — including
 * the boolean filter's legitimate false. */
fa('default replaces failure', 'x', FILTER_VALIDATE_INT, ['options' => ['default' => 'D']]);
fa('default replaces bool false', 'no', FILTER_VALIDATE_BOOLEAN, ['options' => ['default' => 'D']]);
fa('default per element', ['1', 'x'], FILTER_VALIDATE_INT,
   ['flags' => FILTER_REQUIRE_ARRAY, 'options' => ['default' => 'D']]);
?>
--EXPECT--
array, no flags                => false
array, require_array           => [1,2]
array, keys kept               => {"a":1,"b":false}
array, nested                  => [[1,false],[3]]
array, force_array             => [1,2]
array, require_scalar          => false
array, null_on_failure         => [null]
array, scalar+null             => null
empty array                    => []
array, default filter          => ["a","b"]
scalar, no flags               => 5
scalar, require_array          => false
scalar, require_array+null     => null
scalar, force_array            => [5]
scalar, force_array bad        => [false]
scalar, require_scalar         => 5
opts: require_array            => [1,2]
opts: array + min_range        => [false,2]
opts: force_array              => [7]
opts: filter key wins          => 7
opts: array, no flags key      => false
default replaces failure       => "D"
default replaces bool false    => "D"
default per element            => [1,"D"]
--CLEAN--
<?php
unset($list, $mixed, $nested);
