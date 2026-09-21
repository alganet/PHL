--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Magic constants are case-insensitive; user constants are not
--FILE--
<?php
// Every spelling of a magic constant is the same constant.
echo var_export(__line__ === __LINE__, true), var_export(__Line__ === __LINE__, true), "\n";
echo var_export(__file__ === __FILE__, true), var_export(__FiLe__ === __FILE__, true), ' ',
    var_export(__dir__ === __DIR__, true), var_export(__Dir__ === dirname(__FILE__), true), "\n";

function mcciWhere(): string { return __function__ . '|' . __FUNCTION__ . '|' . __FuNcTiOn__; }
echo mcciWhere(), "\n";

class McciHolder
{
    public function method(): string { return __method__ . '|' . __METHOD__; }
    public function cls(): string { return __class__ . '|' . __CLASS__ . '|' . __Class__; }
}
$mcci = new McciHolder();
echo $mcci->method(), ' ', $mcci->cls(), "\n";

trait McciTrait { public function tag(): string { return __trait__ . '|' . __TRAIT__; } }
class McciUser { use McciTrait; }
echo (new McciUser)->tag(), "\n";

// __namespace__ is "" in the global scope, in either spelling.
echo var_export(__namespace__ === __NAMESPACE__, true), var_export(__NAMESPACE__, true), "\n";

// A class constant NAMED like a magic constant is an ordinary, case-SENSITIVE name.
class McciNames
{
    const __LINE__ = 'member';
    const __CLASS__ = 'also-member';
}
echo McciNames::__LINE__, ' ', McciNames::__CLASS__, "\n";

// User constants keep php's case sensitivity, underscores or not.
define('__MCCI_USER__', 'user-const');
echo __MCCI_USER__, ' ', var_export(defined('__mcci_user__'), true), "\n";
?>
--EXPECT--
truetrue
truetrue truetrue
mcciWhere|mcciWhere|mcciWhere
McciHolder::method|McciHolder::method McciHolder|McciHolder|McciHolder
McciTrait|McciTrait
true''
member also-member
user-const false
--CLEAN--
<?php
