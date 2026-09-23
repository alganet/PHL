--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
extension_loaded matches case-insensitively and declares its parameter
--FILE--
<?php
// The name match is case-INSENSITIVE, and every extension listed here is one
// both engines report (PHL's set is smaller than php's by design, §10, so the
// full list is not comparable).
$common = ['Core', 'standard', 'pcre', 'json', 'ctype', 'date', 'SPL',
           'Reflection', 'mbstring', 'hash', 'filter', 'session'];
foreach ($common as $e) {
    printf("%s=%d/%d/%d ", $e, (int)extension_loaded($e),
        (int)extension_loaded(strtolower($e)), (int)extension_loaded(strtoupper($e)));
}
echo "\n";
var_dump(extension_loaded('definitely_not_an_extension'), extension_loaded(''));

// Every one of them is in the list, and the list has no duplicates.
$loaded = get_loaded_extensions();
$missing = array_values(array_diff($common, $loaded));
var_dump($missing, count($loaded) === count(array_unique($loaded)));

// Declared parameters: the type screen and the arity bounds both come from the
// signature, so these are php's own errors rather than a silent false.
try {
    extension_loaded([1]);
} catch (TypeError $e) {
    echo $e->getMessage(), "\n";
}
try {
    get_loaded_extensions([1]);
} catch (TypeError $e) {
    echo $e->getMessage(), "\n";
}
try {
    extension_loaded();
} catch (ArgumentCountError $e) {
    echo $e->getMessage(), "\n";
}
try {
    extension_loaded('json', 'extra');
} catch (ArgumentCountError $e) {
    echo $e->getMessage(), "\n";
}
try {
    get_loaded_extensions(false, 1);
} catch (ArgumentCountError $e) {
    echo $e->getMessage(), "\n";
}
$r = new ReflectionFunction('extension_loaded');
printf("%s(%s): %s\n", $r->getName(),
    implode(', ', array_map(fn($p) => $p->getType() . ' $' . $p->getName(), $r->getParameters())),
    (string)$r->getReturnType());
$r = new ReflectionFunction('get_loaded_extensions');
printf("%s(%s): %s\n", $r->getName(),
    implode(', ', array_map(fn($p) => $p->getType() . ' $' . $p->getName(), $r->getParameters())),
    (string)$r->getReturnType());
--EXPECT--
Core=1/1/1 standard=1/1/1 pcre=1/1/1 json=1/1/1 ctype=1/1/1 date=1/1/1 SPL=1/1/1 Reflection=1/1/1 mbstring=1/1/1 hash=1/1/1 filter=1/1/1 session=1/1/1 
bool(false)
bool(false)
array(0) {
}
bool(true)
extension_loaded(): Argument #1 ($extension) must be of type string, array given
get_loaded_extensions(): Argument #1 ($zend_extensions) must be of type bool, array given
extension_loaded() expects exactly 1 argument, 0 given
extension_loaded() expects exactly 1 argument, 2 given
get_loaded_extensions() expects at most 1 argument, 2 given
extension_loaded(string $extension): bool
get_loaded_extensions(bool $zend_extensions): array
--CLEAN--
<?php
