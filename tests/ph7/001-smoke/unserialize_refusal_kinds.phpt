--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
php's unserialize() refusal has the same two kinds serialize() has, and its own escape
--DESCRIPTION--
The reading twin of serialize_refusal_kinds.phpt. It has that test's two-step and
its second sentence: the FLAG kind (Closure, SplFileInfo, XMLParser, PDO, DOMXPath,
CurlHandle) admits no escape -- a subclass declaring __unserialize() is still
refused -- while the deny HANDLER kind, whose only users are the DOM node classes,
says ", unless unserialization methods are implemented in a subclass" and means it.

The escape is the READING magic: __wakeup() or __unserialize() anywhere in the
chain. __sleep() is not one of them, so a DOM subclass declaring only that is still
refused, where on the writing side __sleep() is exactly what rescues it.

The refusal fires on the HEADER, before the body is read -- a truncated payload
behind a denied class name is still this exception and not a syntax error -- and it
names the RECEIVER, so a subclass reports its own name. It is skipped for the
incomplete carrier: `allowed_classes: false` never builds the named class, so php
hands back __PHP_Incomplete_Class without complaint.

PHL honoured none of this before: every one of these payloads BUILT the object,
handing a script a Closure, a PDO or an XMLParser whose engine state had never been
initialized.
--FILE--
<?php
class UnserRefusalOrdinary {}
class UnserRefusalPdo extends PDO { public function __construct() {} }
class UnserRefusalSoft extends DOMElement {}
class UnserRefusalSoftWake extends DOMElement { public function __wakeup(): void {} }
class UnserRefusalSoftUnser extends DOMElement { public function __unserialize(array $d): void {} }
class UnserRefusalSoftSleep extends DOMElement { public function __sleep(): array { return []; } }
class UnserRefusalSoftGrand extends UnserRefusalSoftWake {}
class UnserRefusalKid extends SplFileInfo { public function __construct() {} }
class UnserRefusalMagic extends SplFileInfo {
    public function __construct() {}
    public function __unserialize(array $d): void { echo "__unserialize ran\n"; }
}

foreach ([
    // the flag kind
    'O:7:"Closure":0:{}',
    'O:9:"XMLParser":0:{}',
    'O:11:"SplFileInfo":0:{}',
    // the deny-handler kind
    'O:3:"PDO":0:{}',
    // both, through a user subclass: the name reported is the RECEIVER's
    'O:15:"UnserRefusalPdo":0:{}',
    'O:15:"UnserRefusalKid":0:{}',
    // ... and a subclass __unserialize() does not rescue it
    'O:17:"UnserRefusalMagic":0:{}',
    // the header is enough: the body is never reached
    'O:7:"Closure":1:{',
    // nested in a container
    'a:1:{i:0;O:7:"Closure":0:{}}',
    // the deny-HANDLER kind: a different sentence, with an escape that works
    'O:10:"DOMElement":0:{}',
    'O:16:"UnserRefusalSoft":0:{}',
    'O:20:"UnserRefusalSoftWake":0:{}',
    'O:21:"UnserRefusalSoftUnser":0:{}',
    // __sleep() is the WRITING magic and does not rescue a read
    'O:21:"UnserRefusalSoftSleep":0:{}',
    // inherited from a subclass counts
    'O:21:"UnserRefusalSoftGrand":0:{}',
    // a DOM class of neither kind
    'O:11:"DOMNodeList":0:{}',
    // ordinary classes still unserialize
    'O:8:"stdClass":0:{}',
    'O:20:"UnserRefusalOrdinary":0:{}',
] as $payload) {
    try {
        $r = @unserialize($payload);
        echo $payload, ' => ', is_object($r) ? get_class($r) : var_export($r, true), "\n";
    } catch (Throwable $e) {
        echo $payload, ' => ', get_class($e), ': ', $e->getMessage(), "\n";
    }
}

// allowed_classes:false builds the carrier instead, and is not refused
$carrier = unserialize('O:7:"Closure":0:{}', ['allowed_classes' => false]);
echo 'allowed_classes:false => ', get_class($carrier), "\n";
--EXPECT--
O:7:"Closure":0:{} => Exception: Unserialization of 'Closure' is not allowed
O:9:"XMLParser":0:{} => Exception: Unserialization of 'XMLParser' is not allowed
O:11:"SplFileInfo":0:{} => Exception: Unserialization of 'SplFileInfo' is not allowed
O:3:"PDO":0:{} => Exception: Unserialization of 'PDO' is not allowed
O:15:"UnserRefusalPdo":0:{} => Exception: Unserialization of 'UnserRefusalPdo' is not allowed
O:15:"UnserRefusalKid":0:{} => Exception: Unserialization of 'UnserRefusalKid' is not allowed
O:17:"UnserRefusalMagic":0:{} => Exception: Unserialization of 'UnserRefusalMagic' is not allowed
O:7:"Closure":1:{ => Exception: Unserialization of 'Closure' is not allowed
a:1:{i:0;O:7:"Closure":0:{}} => Exception: Unserialization of 'Closure' is not allowed
O:10:"DOMElement":0:{} => Exception: Unserialization of 'DOMElement' is not allowed, unless unserialization methods are implemented in a subclass
O:16:"UnserRefusalSoft":0:{} => Exception: Unserialization of 'UnserRefusalSoft' is not allowed, unless unserialization methods are implemented in a subclass
O:20:"UnserRefusalSoftWake":0:{} => UnserRefusalSoftWake
O:21:"UnserRefusalSoftUnser":0:{} => UnserRefusalSoftUnser
O:21:"UnserRefusalSoftSleep":0:{} => Exception: Unserialization of 'UnserRefusalSoftSleep' is not allowed, unless unserialization methods are implemented in a subclass
O:21:"UnserRefusalSoftGrand":0:{} => UnserRefusalSoftGrand
O:11:"DOMNodeList":0:{} => DOMNodeList
O:8:"stdClass":0:{} => stdClass
O:20:"UnserRefusalOrdinary":0:{} => UnserRefusalOrdinary
allowed_classes:false => __PHP_Incomplete_Class
