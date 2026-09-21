--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
extract EXTR_* constants expand to php's enum values, and a literal flag means php's mode
--FILE--
<?php
foreach ([
    'EXTR_OVERWRITE', 'EXTR_SKIP', 'EXTR_PREFIX_SAME', 'EXTR_PREFIX_ALL',
    'EXTR_PREFIX_INVALID', 'EXTR_PREFIX_IF_EXISTS', 'EXTR_IF_EXISTS',
] as $exfv_name) {
    echo $exfv_name, '=', constant($exfv_name), "\n";
}
// A literal 1 is EXTR_SKIP, not EXTR_OVERWRITE: the collision keeps $exfv_a.
$exfv_probe = function () {
    $a = 'kept';
    $n = extract(['a' => 'imported'], 1);
    return "$n|$a";
};
echo $exfv_probe(), "\n";
?>
--EXPECT--
EXTR_OVERWRITE=0
EXTR_SKIP=1
EXTR_PREFIX_SAME=2
EXTR_PREFIX_ALL=3
EXTR_PREFIX_INVALID=4
EXTR_PREFIX_IF_EXISTS=5
EXTR_IF_EXISTS=6
0|kept
--CLEAN--
<?php
