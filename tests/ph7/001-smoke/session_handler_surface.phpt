--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
The session save-handler surface declares php's tentative return types
--DESCRIPTION--
php's three save-handler interfaces and the SessionHandler class that
implements the first two declare a return type on every one of their sixteen
methods, and every one is the TENTATIVE kind — which is what lets a handler
written before php 8.1 keep answering whatever its store answers. The rows here
declared none at all, on the belief that php's stubs declare none, so
getTentativeReturnType() answered null on all sixteen and the exported line
printed no return at all. Two of them are not the obvious word: `read` answers
`string|false` rather than `string`, and `gc` answers `int|false` — the count of
records it removed, or false.
--FILE--
<?php
function shsRow($class) {
    $rc = new ReflectionClass($class);
    $names = [];
    foreach ($rc->getMethods() as $m) {
        if ($m->getDeclaringClass()->getName() !== $class) { continue; }
        $names[] = $m->getName();
    }
    sort($names);   /* php and PHL do not build the table in the same order */
    foreach ($names as $n) {
        $m = new ReflectionMethod($class, $n);
        $real = $m->hasReturnType() ? (string)$m->getReturnType() : '-';
        $tent = $m->hasTentativeReturnType() ? (string)$m->getTentativeReturnType() : '-';
        $args = [];
        foreach ($m->getParameters() as $p) {
            $args[] = ($p->hasType() ? (string)$p->getType() . ' ' : '') . '$' . $p->getName();
        }
        printf("%-40s real=%-6s tentative=%s\n",
            "$class::$n(" . implode(', ', $args) . ")", $real, $tent);
    }
}
shsRow('SessionHandlerInterface');
shsRow('SessionIdInterface');
shsRow('SessionUpdateTimestampHandlerInterface');
shsRow('SessionHandler');
--EXPECT--
SessionHandlerInterface::close()         real=-      tentative=bool
SessionHandlerInterface::destroy(string $id) real=-      tentative=bool
SessionHandlerInterface::gc(int $max_lifetime) real=-      tentative=int|false
SessionHandlerInterface::open(string $path, string $name) real=-      tentative=bool
SessionHandlerInterface::read(string $id) real=-      tentative=string|false
SessionHandlerInterface::write(string $id, string $data) real=-      tentative=bool
SessionIdInterface::create_sid()         real=-      tentative=string
SessionUpdateTimestampHandlerInterface::updateTimestamp(string $id, string $data) real=-      tentative=bool
SessionUpdateTimestampHandlerInterface::validateId(string $id) real=-      tentative=bool
SessionHandler::close()                  real=-      tentative=bool
SessionHandler::create_sid()             real=-      tentative=string
SessionHandler::destroy(string $id)      real=-      tentative=bool
SessionHandler::gc(int $max_lifetime)    real=-      tentative=int|false
SessionHandler::open(string $path, string $name) real=-      tentative=bool
SessionHandler::read(string $id)         real=-      tentative=string|false
SessionHandler::write(string $id, string $data) real=-      tentative=bool
