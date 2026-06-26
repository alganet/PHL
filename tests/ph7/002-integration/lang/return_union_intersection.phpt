--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Union and intersection return types are enforced
--FILE--
<?php
interface RA {}
interface RB {}
class RAB implements RA, RB {}
class RNotA {}

function r_union(): RA|RB { return new RAB; }
echo get_class(r_union()), "\n";

function r_union_bad(): RA|RB { return 42; }
try { r_union_bad(); } catch (\TypeError $e) { echo "union TE\n"; }

function r_isect(): RA&RB { return new RAB; }
echo get_class(r_isect()), "\n";

function r_isect_bad(): RA&RB { return new RNotA; }
try { r_isect_bad(); } catch (\TypeError $e) { echo "isect TE\n"; }

function r_dnf_null(): RA|RB|null { return null; }
var_export(r_dnf_null()); echo "\n";
?>
--EXPECT--
RAB
union TE
RAB
isect TE
NULL
--CLEAN--
<?php
