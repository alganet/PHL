--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
php's serialize() refusal comes in two kinds, and both are inherited
--DESCRIPTION--
php refuses to serialize some classes in TWO different places and the sentence says which.
`ZEND_ACC_NOT_SERIALIZABLE` is tested before anything else, so even a subclass declaring
__serialize() is refused -- Closure, the Reflection classes, SplFileInfo, the directory
iterators and DOMXPath are that kind. A deny `ce->serialize` HANDLER is consulted only AFTER
the __serialize()/__sleep() lookup, so there a subclass declaring either one serializes
normally, and php's sentence names that escape: `, unless serialization methods are implemented
in a subclass`. The DOM node classes are the only users of the soft kind.
Both ride down to every USER subclass, which PHL's flag did not: `class Kid extends SplFileInfo
{}` serialized here and php refuses it. The DOM classes had neither -- before serialize()
stopped emitting their hidden slot they failed outright and answered false, and after it they
wrote a payload that unserialize() turned into a BLANK document.
--FILE--
<?php
function refShow($label, $fn) {
    try { $out = $fn(); if (!is_string($out)) { $out = var_export($out, true); } }
    catch (Throwable $e) { $out = get_class($e) . ': ' . $e->getMessage(); }
    echo $label, ' => ', $out, "\n";
}
$refDoc = new DOMDocument;
$refDoc->loadXML('<r a="1"><x>t</x></r>');

echo "-- the HARD kind: nothing rescues it\n";
refShow('Closure', fn() => serialize(fn() => 1));
refShow('WeakReference', fn() => serialize(WeakReference::create(new stdClass)));
refShow('SplFileInfo', fn() => serialize(new SplFileInfo(__DIR__)));
refShow('DirectoryIterator', fn() => serialize(new DirectoryIterator(__DIR__)));
refShow('ReflectionClass', fn() => serialize(new ReflectionClass('stdClass')));
refShow('DOMXPath', fn() => serialize(new DOMXPath($refDoc)));

echo "-- and it reaches a USER subclass, naming that subclass\n";
class RefKidInfo extends SplFileInfo {}
class RefKidDir extends DirectoryIterator {}
class RefKidXPath extends DOMXPath {
    public function __serialize(): array { return ['a' => 1]; }
    public function __unserialize(array $d): void {}
}
refShow('subclass of SplFileInfo', fn() => serialize(new RefKidInfo(__DIR__)));
refShow('subclass of DirectoryIterator', fn() => serialize(new RefKidDir(__DIR__)));
// The hard kind is tested BEFORE the magic lookup, so declaring __serialize does not help.
refShow('subclass of DOMXPath WITH __serialize', fn() => serialize(new RefKidXPath($refDoc)));

echo "-- the SOFT kind: the sentence names the escape\n";
refShow('DOMDocument', fn() => serialize(new DOMDocument));
refShow('DOMElement', fn() => serialize(new DOMElement('e')));
refShow('DOMText', fn() => serialize(new DOMText('t')));
refShow('DOMAttr', fn() => serialize(new DOMAttr('a')));
refShow('DOMComment', fn() => serialize(new DOMComment('c')));
refShow('DOMCdataSection', fn() => serialize(new DOMCdataSection('x')));

echo "-- and a subclass taking the escape is serialized\n";
class RefKidSer extends DOMText {
    public function __serialize(): array { return ['a' => 1]; }
    public function __unserialize(array $d): void {}
}
class RefKidSleep extends DOMText {
    public function __sleep(): array { return []; }
    public function __wakeup(): void {}
}
class RefKidWake extends DOMText { public function __wakeup(): void {} }
class RefKidPlain extends DOMDocument { public $p = 1; }
refShow('__serialize rescues', fn() => serialize(new RefKidSer('t')));
refShow('__sleep rescues too', fn() => serialize(new RefKidSleep('t')));
// __wakeup is not one of the two, and a plain subclass inherits the refusal.
refShow('__wakeup does NOT', fn() => serialize(new RefKidWake('t')));
refShow('plain subclass', fn() => serialize(new RefKidPlain));

echo "-- the DOM classes php does NOT refuse\n";
refShow('DOMNodeList', fn() => serialize($refDoc->getElementsByTagName('x')));
refShow('DOMNamedNodeMap', fn() => serialize($refDoc->documentElement->attributes));
--EXPECT--
-- the HARD kind: nothing rescues it
Closure => Exception: Serialization of 'Closure' is not allowed
WeakReference => Exception: Serialization of 'WeakReference' is not allowed
SplFileInfo => Exception: Serialization of 'SplFileInfo' is not allowed
DirectoryIterator => Exception: Serialization of 'DirectoryIterator' is not allowed
ReflectionClass => Exception: Serialization of 'ReflectionClass' is not allowed
DOMXPath => Exception: Serialization of 'DOMXPath' is not allowed
-- and it reaches a USER subclass, naming that subclass
subclass of SplFileInfo => Exception: Serialization of 'RefKidInfo' is not allowed
subclass of DirectoryIterator => Exception: Serialization of 'RefKidDir' is not allowed
subclass of DOMXPath WITH __serialize => Exception: Serialization of 'RefKidXPath' is not allowed
-- the SOFT kind: the sentence names the escape
DOMDocument => Exception: Serialization of 'DOMDocument' is not allowed, unless serialization methods are implemented in a subclass
DOMElement => Exception: Serialization of 'DOMElement' is not allowed, unless serialization methods are implemented in a subclass
DOMText => Exception: Serialization of 'DOMText' is not allowed, unless serialization methods are implemented in a subclass
DOMAttr => Exception: Serialization of 'DOMAttr' is not allowed, unless serialization methods are implemented in a subclass
DOMComment => Exception: Serialization of 'DOMComment' is not allowed, unless serialization methods are implemented in a subclass
DOMCdataSection => Exception: Serialization of 'DOMCdataSection' is not allowed, unless serialization methods are implemented in a subclass
-- and a subclass taking the escape is serialized
__serialize rescues => O:9:"RefKidSer":1:{s:1:"a";i:1;}
__sleep rescues too => O:11:"RefKidSleep":0:{}
__wakeup does NOT => Exception: Serialization of 'RefKidWake' is not allowed, unless serialization methods are implemented in a subclass
plain subclass => Exception: Serialization of 'RefKidPlain' is not allowed, unless serialization methods are implemented in a subclass
-- the DOM classes php does NOT refuse
DOMNodeList => O:11:"DOMNodeList":0:{}
DOMNamedNodeMap => O:15:"DOMNamedNodeMap":0:{}
