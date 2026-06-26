--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Nullable return types (?T, T|null) accept an explicit null and still reject mismatches
--FILE--
<?php
class RnX {}

function rn_q(): ?RnX { return null; }
var_export(rn_q()); echo "\n";

function rn_class_null(): RnX|null { return new RnX; }
echo get_class(rn_class_null()), "\n";

function rn_scalar_null(): int|null { return null; }
var_export(rn_scalar_null()); echo "\n";

function rn_scalar_val(): int|null { return 7; }
var_export(rn_scalar_val()); echo "\n";

// A nullable return still rejects a wrong non-null value.
function rn_bad(): ?RnX { return 42; }
try { rn_bad(); } catch (\TypeError $e) { echo "nullable-mismatch TE\n"; }

// A non-nullable return still rejects null.
function rn_nonnull(): RnX { return new RnX; }
echo get_class(rn_nonnull()), "\n";
function rn_nonnull_bad(): RnX { return null; }
try { rn_nonnull_bad(); } catch (\TypeError $e) { echo "nonnull-null TE\n"; }
?>
--EXPECT--
NULL
RnX
NULL
7
nullable-mismatch TE
RnX
nonnull-null TE
--CLEAN--
<?php
