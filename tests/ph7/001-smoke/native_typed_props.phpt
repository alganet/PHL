--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A native class's presented properties carry php's declared types
--DESCRIPTION--
php types nearly every property it presents, and a native spec row could say so
since RegexIterator built `zType` — but no other class had used it, so every
reflector's $name, LibXMLError's six fields, DatePeriod's seven and DOMXPath's
$document were untyped. Declaring them found what an untyped slot had been
hiding: php's are declared WITHOUT a default (`public string $name;`), which a
spec row had no way to say, and the four typed shortcut setters
(PH7_NativeSetAttr{Int,Str,Bool,Obj}) wrote the slot without clearing the
not-yet-initialized mark that PH7_NativeSetProp clears — so the first
default-less typed slot made every reflector throw from a constructor that HAD
written it.
--FILE--
<?php
function tnpShow($label, $fn) {
    try { $out = var_export($fn(), true); }
    catch (Throwable $e) { $out = get_class($e) . ': ' . $e->getMessage(); }
    echo $label, ' => ', str_replace("\n", '', $out), "\n";
}
tnpShow('reflector name type', fn() => (string)(new ReflectionProperty('ReflectionClass', 'name'))->getType());
tnpShow('reflector name default', fn() => (new ReflectionProperty('ReflectionClass', 'name'))->hasDefaultValue());
tnpShow('libxml level type', fn() => (string)(new ReflectionProperty('LibXMLError', 'level'))->getType());
tnpShow('libxml message type', fn() => (string)(new ReflectionProperty('LibXMLError', 'message'))->getType());
tnpShow('DatePeriod start type', fn() => (string)(new ReflectionProperty('DatePeriod', 'start'))->getType());
tnpShow('DatePeriod recurrences type', fn() => (string)(new ReflectionProperty('DatePeriod', 'recurrences'))->getType());
tnpShow('DOMXPath document type', fn() => (string)(new ReflectionProperty('DOMXPath', 'document'))->getType());
tnpShow('uninitialized read', function () {
    $r = (new ReflectionClass('ReflectionClass'))->newInstanceWithoutConstructor();
    return $r->name;
});
tnpShow('initialized after ctor', fn() => (new ReflectionClass('ArrayObject'))->name);
tnpShow('isInitialized before', function () {
    $r = (new ReflectionClass('ReflectionClass'))->newInstanceWithoutConstructor();
    return (new ReflectionProperty('ReflectionClass', 'name'))->isInitialized($r);
});
tnpShow('store wrong type', function () {
    $e = new LibXMLError; $e->level = 'nope'; return $e->level;
});
tnpShow('store coercible', function () {
    $e = new LibXMLError; $e->level = '4'; return $e->level;
});
tnpShow('export line', function () {
    foreach (explode("\n", (string)new ReflectionClass('LibXMLError')) as $l) {
        if (strpos($l, '$message') !== false) { return trim($l); }
    }
    return '-';
});
--EXPECT--
reflector name type => 'string'
reflector name default => false
libxml level type => 'int'
libxml message type => 'string'
DatePeriod start type => '?DateTimeInterface'
DatePeriod recurrences type => 'int'
DOMXPath document type => 'DOMDocument'
uninitialized read => Error: Typed property ReflectionClass::$name must not be accessed before initialization
initialized after ctor => 'ArrayObject'
isInitialized before => false
store wrong type => TypeError: Cannot assign string to property LibXMLError::$level of type int
store coercible => 4
export line => 'Property [ public string $message ]'
