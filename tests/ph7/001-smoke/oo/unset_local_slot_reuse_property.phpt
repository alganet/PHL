--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A local freed by unset() must not clobber an object property that reuses its slot
--FILE--
<?php
final class UlsrpHolder {
    private string $code;
    public function __construct(string $c) { $this->code = $c; }
    public function code(): string { return $this->code; }
}
function ulsrp_build(): UlsrpHolder {
    $tmp = str_repeat('T', 64);   // reserves a local slot
    unset($tmp);                  // returns the slot to the free pool
    $h = new UlsrpHolder(str_repeat('x', 1892)); // property may reuse that slot
    return $h;
}
$h = ulsrp_build();
echo strlen($h->code()), "\n";
?>
--EXPECT--
1892
