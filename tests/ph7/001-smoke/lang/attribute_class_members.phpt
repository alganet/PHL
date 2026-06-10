--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Attributes on class, const, property, method and promoted param are inert
--FILE--
<?php
#[Entity] class AttrMembers {
    #[Marker] const TAG = "tag";
    #[Column] public $prop = "prop";
    public function __construct(#[Inject] public $promoted = "promoted") {}
    #[Route] public function method(){ return "method"; }
}
$o = new AttrMembers();
echo AttrMembers::TAG, "\n";
echo $o->prop, "\n";
echo $o->promoted, "\n";
echo $o->method(), "\n";
?>
--EXPECT--
tag
prop
promoted
method
--CLEAN--
<?php
?>
