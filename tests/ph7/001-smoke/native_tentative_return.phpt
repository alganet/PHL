--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A native method's return type can be php's @tentative-return-type
--DESCRIPTION--
php's stubs carry two kinds of internal return type and Reflection answers
differently for each: a REAL one is reported by getReturnType()/hasReturnType()
and printed `Return [ T ]`; an `@tentative-return-type` answers null and false
from those two, is reported by getTentativeReturnType()/hasTentativeReturnType(),
and prints `Tentative return [ T ]`. Nearly every internal SPL, date, DOM and
Reflection method is the tentative kind. PHL had one field and no marker, so the
advice was to leave it EMPTY — which agreed with php on hasReturnType() and
disagreed on the other three. A leading `@` on a native spec's return text is the
mark now. The date family shows why the text cannot always be `static`: php's
stub writes the CONCRETE class for the legacy mutators, so DateTime::add reports
DateTime and DateTimeImmutable::add reports DateTimeImmutable, while
setMicrosecond really is a `static` — and a real one, not a tentative.
--FILE--
<?php
function ntrShow($class, $method) {
    $m = new ReflectionMethod($class, $method);
    $real = $m->hasReturnType() ? (string)$m->getReturnType() : '-';
    $tent = $m->hasTentativeReturnType() ? (string)$m->getTentativeReturnType() : '-';
    $line = '-';
    foreach (explode("\n", (string)$m) as $l) {
        $l = trim($l);
        if (strncmp($l, '- Return [', 10) === 0 || strncmp($l, '- Tentative return [', 20) === 0) {
            $line = $l;
        }
    }
    printf("%-34s real=%-14s tentative=%-22s %s\n", "$class::$method", $real, $tent, $line);
}

/* Tentative, one per family. */
ntrShow('ArrayIterator', 'count');
ntrShow('ArrayIterator', 'append');
ntrShow('ArrayObject', 'getIterator');
ntrShow('RegexIterator', 'accept');
ntrShow('DateTime', 'format');
ntrShow('DateTime', 'getTimezone');
ntrShow('ReflectionClass', 'getName');
ntrShow('ReflectionProperty', 'getValue');
ntrShow('DOMDocument', 'createTextNode');
ntrShow('XMLWriter', 'text');

/* The same method on the two date classes reports its OWN class, which is why
 * the shared declaration is parameterized rather than saying `static`. */
ntrShow('DateTime', 'add');
ntrShow('DateTimeImmutable', 'add');

/* REAL return types, which must keep answering from getReturnType(): a `static`
 * php really declares, a plain one, and a method php never made tentative. */
ntrShow('DateTime', 'setMicrosecond');
ntrShow('DateTime', 'getMicrosecond');
ntrShow('Closure', 'call');
ntrShow('ReflectionClass', 'getStaticPropertyValue');

/* A USERLAND method has no tentative type at all — the concept is stubs-only. */
class NtrUser { public function m(): int { return 1; } }
ntrShow('NtrUser', 'm');
--EXPECT--
ArrayIterator::count               real=-              tentative=int                    - Tentative return [ int ]
ArrayIterator::append              real=-              tentative=void                   - Tentative return [ void ]
ArrayObject::getIterator           real=-              tentative=Iterator               - Tentative return [ Iterator ]
RegexIterator::accept              real=-              tentative=bool                   - Tentative return [ bool ]
DateTime::format                   real=-              tentative=string                 - Tentative return [ string ]
DateTime::getTimezone              real=-              tentative=DateTimeZone|false     - Tentative return [ DateTimeZone|false ]
ReflectionClass::getName           real=-              tentative=string                 - Tentative return [ string ]
ReflectionProperty::getValue       real=-              tentative=mixed                  - Tentative return [ mixed ]
DOMDocument::createTextNode        real=-              tentative=DOMText                - Tentative return [ DOMText ]
XMLWriter::text                    real=-              tentative=bool                   - Tentative return [ bool ]
DateTime::add                      real=-              tentative=DateTime               - Tentative return [ DateTime ]
DateTimeImmutable::add             real=-              tentative=DateTimeImmutable      - Tentative return [ DateTimeImmutable ]
DateTime::setMicrosecond           real=static         tentative=-                      - Return [ static ]
DateTime::getMicrosecond           real=int            tentative=-                      - Return [ int ]
Closure::call                      real=mixed          tentative=-                      - Return [ mixed ]
ReflectionClass::getStaticPropertyValue real=-              tentative=mixed                  - Tentative return [ mixed ]
NtrUser::m                         real=int            tentative=-                      - Return [ int ]
