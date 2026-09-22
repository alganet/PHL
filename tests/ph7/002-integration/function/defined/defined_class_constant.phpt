--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
defined() answers class constants, enum cases and scope keywords like php
--FILE--
<?php
interface DcI { const IK = 1; }
class DcC implements DcI {
    const A = 2;
    private const P = 3;
    protected const Q = 4;
    static function inside() {
        var_dump(defined("DcC::P"), defined("DcC::Q"), defined("self::P"), defined("static::A"));
    }
}
class DcSub extends DcC {
    const B = 5;
    static function inside() {
        // private stays invisible one level down; protected does not
        var_dump(defined("DcC::P"), defined("DcC::Q"), defined("parent::A"), defined("static::B"));
    }
}
enum DcE: string { case Hearts = 'H'; const X = 6; }
// the plain form still works
define('DC_G', 1);
var_dump(defined("DC_G"), defined("DC_NOPE"));
// declared, inherited and interface constants
var_dump(defined("DcC::A"), defined("DcSub::A"), defined("DcSub::B"), defined("DcC::IK"), defined("DcI::IK"));
// enum cases and enum constants
var_dump(defined("DcE::Hearts"), defined("DcE::X"), defined("DcE::Nope"));
// visibility is answered against the CALLING scope
var_dump(defined("DcC::P"), defined("DcC::Q"));
DcC::inside();
DcSub::inside();
// the class name folds case, the constant name does not
var_dump(defined("dcc::A"), defined("DcC::a"), defined("dcc::a"));
// a leading global-namespace anchor is fine
var_dump(defined("\\DcC::A"));
// misses of every kind are false, never an error
var_dump(defined("DcNoSuchClass::A"), defined("DcC::NOPE"), defined("DcC::"), defined("::A"),
    defined("DcC::A::B"), defined("DcC:A"), defined(" DcC::A"));
// asking about a constant does not evaluate anything: the enum case still materializes
// on its own terms afterwards, and an on-demand initializer still runs
class DcLazy { const K = 1; const L = self::K + 6; }
var_dump(defined("DcLazy::L"), DcLazy::L, DcE::from('H') === DcE::Hearts);
?>
--EXPECT--
bool(true)
bool(false)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(false)
bool(false)
bool(false)
bool(true)
bool(true)
bool(true)
bool(true)
bool(false)
bool(true)
bool(true)
bool(true)
bool(true)
bool(false)
bool(false)
bool(true)
bool(false)
bool(false)
bool(false)
bool(false)
bool(false)
bool(false)
bool(false)
bool(true)
int(7)
bool(true)
--CLEAN--
<?php
