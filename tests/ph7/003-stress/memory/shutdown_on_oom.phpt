--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
An out-of-memory fatal still runs registered shutdown functions (PHP semantics)
--DESCRIPTION--
Run under a small per-allocation cap (PHL_MAX_ALLOC, set by `make test-stress`).
The OOM fatal is non-catchable and halts execution, but register_shutdown_function
callbacks must still run, mirroring PHP.
--SKIPIF--
<?php if (!getenv('PHL_MAX_ALLOC')) { echo "skip needs PHL_MAX_ALLOC cap (run: make test-stress)"; } ?>
--FILE--
<?php
register_shutdown_function(function(){ echo "SHUTDOWN\n"; });
$s = str_repeat('A', 8000000);
echo "UNREACHABLE\n";
?>
--EXPECTF--
Error: PH7 is running out of memory in %s on line %d
SHUTDOWN
--CLEAN--
<?php
