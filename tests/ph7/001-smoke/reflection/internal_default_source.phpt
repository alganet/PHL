--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
An internal default written as an expression prints as its source
--DESCRIPTION--
php keeps the SOURCE of a stub's default and prints it back verbatim, so
htmlspecialchars() shows `ENT_QUOTES | ENT_SUBSTITUTE | ENT_HTML401` and
mkdir() shows `0777` while getDefaultValue() still answers 11 and 511. Fourteen
rows spelled the reduced NUMBER instead, and four had no default at all because
their expression is an enum CASE the reducer could not materialize --- an enum
case is a singleton with its own materializer, and running a constant
initializer over one answers nothing.
--FILE--
<?php
function idsShow($label, $fn) {
    try { echo $label, ' => ', $fn(), "\n"; }
    catch (Throwable $e) { echo $label, ' => ', get_class($e), ': ', $e->getMessage(), "\n"; }
}
$idsRows = [['htmlspecialchars', 1], ['htmlspecialchars_decode', 1], ['htmlentities', 1],
            ['html_entity_decode', 1], ['get_html_translation_table', 1],
            ['stream_socket_server', 3], ['mkdir', 1], ['round', 2], ['bcround', 2]];
foreach ($idsRows as [$idsName, $idsPos]) {
    $idsParam = (new ReflectionFunction($idsName))->getParameters()[$idsPos];
    idsShow("$idsName #$idsPos line", fn() => (string)$idsParam);
    idsShow("$idsName #$idsPos val", fn() => var_export($idsParam->getDefaultValue(), true));
    idsShow("$idsName #$idsPos const", fn() => (int)$idsParam->isDefaultValueConstant()
        . ' ' . var_export($idsParam->getDefaultValueConstantName(), true));
}
$idsMethods = [['ArrayObject', '__construct', 2], ['SplTempFileObject', '__construct', 0],
               ['MultipleIterator', '__construct', 0], ['Random\Randomizer', 'getFloat', 2],
               ['BcMath\Number', 'round', 1]];
foreach ($idsMethods as [$idsClass, $idsMethod, $idsPos]) {
    $idsParam = (new ReflectionMethod($idsClass, $idsMethod))->getParameters()[$idsPos];
    idsShow("$idsClass::$idsMethod #$idsPos line", fn() => (string)$idsParam);
    idsShow("$idsClass::$idsMethod #$idsPos val", fn() => var_export($idsParam->getDefaultValue(), true));
    idsShow("$idsClass::$idsMethod #$idsPos const", fn() => (int)$idsParam->isDefaultValueConstant()
        . ' ' . var_export($idsParam->getDefaultValueConstantName(), true));
}
/* The defaults still BEHAVE as the numbers they reduce to. */
var_dump(htmlspecialchars("<'\">") === htmlspecialchars("<'\">", ENT_QUOTES | ENT_SUBSTITUTE | ENT_HTML401));
var_dump(round(2.5) === 3.0, round(-2.5) === -3.0);
--EXPECT--
htmlspecialchars #1 line => Parameter #1 [ <optional> int $flags = ENT_QUOTES | ENT_SUBSTITUTE | ENT_HTML401 ]
htmlspecialchars #1 val => 11
htmlspecialchars #1 const => 0 NULL
htmlspecialchars_decode #1 line => Parameter #1 [ <optional> int $flags = ENT_QUOTES | ENT_SUBSTITUTE | ENT_HTML401 ]
htmlspecialchars_decode #1 val => 11
htmlspecialchars_decode #1 const => 0 NULL
htmlentities #1 line => Parameter #1 [ <optional> int $flags = ENT_QUOTES | ENT_SUBSTITUTE | ENT_HTML401 ]
htmlentities #1 val => 11
htmlentities #1 const => 0 NULL
html_entity_decode #1 line => Parameter #1 [ <optional> int $flags = ENT_QUOTES | ENT_SUBSTITUTE | ENT_HTML401 ]
html_entity_decode #1 val => 11
html_entity_decode #1 const => 0 NULL
get_html_translation_table #1 line => Parameter #1 [ <optional> int $flags = ENT_QUOTES | ENT_SUBSTITUTE | ENT_HTML401 ]
get_html_translation_table #1 val => 11
get_html_translation_table #1 const => 0 NULL
stream_socket_server #3 line => Parameter #3 [ <optional> int $flags = STREAM_SERVER_BIND | STREAM_SERVER_LISTEN ]
stream_socket_server #3 val => 12
stream_socket_server #3 const => 0 NULL
mkdir #1 line => Parameter #1 [ <optional> int $permissions = 0777 ]
mkdir #1 val => 511
mkdir #1 const => 0 NULL
round #2 line => Parameter #2 [ <optional> RoundingMode|int $mode = RoundingMode::HalfAwayFromZero ]
round #2 val => \RoundingMode::HalfAwayFromZero
round #2 const => 1 'RoundingMode::HalfAwayFromZero'
bcround #2 line => Parameter #2 [ <optional> RoundingMode $mode = RoundingMode::HalfAwayFromZero ]
bcround #2 val => \RoundingMode::HalfAwayFromZero
bcround #2 const => 1 'RoundingMode::HalfAwayFromZero'
ArrayObject::__construct #2 line => Parameter #2 [ <optional> string $iteratorClass = ArrayIterator::class ]
ArrayObject::__construct #2 val => 'ArrayIterator'
ArrayObject::__construct #2 const => 0 NULL
SplTempFileObject::__construct #0 line => Parameter #0 [ <optional> int $maxMemory = 2 * 1024 * 1024 ]
SplTempFileObject::__construct #0 val => 2097152
SplTempFileObject::__construct #0 const => 0 NULL
MultipleIterator::__construct #0 line => Parameter #0 [ <optional> int $flags = MultipleIterator::MIT_NEED_ALL | MultipleIterator::MIT_KEYS_NUMERIC ]
MultipleIterator::__construct #0 val => 1
MultipleIterator::__construct #0 const => 0 NULL
Random\Randomizer::getFloat #2 line => Parameter #2 [ <optional> Random\IntervalBoundary $boundary = Random\IntervalBoundary::ClosedOpen ]
Random\Randomizer::getFloat #2 val => \Random\IntervalBoundary::ClosedOpen
Random\Randomizer::getFloat #2 const => 1 'Random\\IntervalBoundary::ClosedOpen'
BcMath\Number::round #1 line => Parameter #1 [ <optional> RoundingMode $mode = RoundingMode::HalfAwayFromZero ]
BcMath\Number::round #1 val => \RoundingMode::HalfAwayFromZero
BcMath\Number::round #1 const => 1 'RoundingMode::HalfAwayFromZero'
bool(true)
bool(true)
bool(true)
