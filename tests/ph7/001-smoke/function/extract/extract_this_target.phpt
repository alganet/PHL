--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
extract never re-assigns $this: Error where a store would land on it, skip where it would not
--FILE--
<?php
class ExtractThisProbe
{
    public function run()
    {
        $out = [];
        foreach ([EXTR_OVERWRITE, EXTR_SKIP, EXTR_IF_EXISTS, EXTR_PREFIX_IF_EXISTS, EXTR_PREFIX_SAME] as $flags) {
            try {
                $n = $flags >= EXTR_PREFIX_SAME
                    ? extract(['this' => 'clobbered'], $flags, 'p')
                    : extract(['this' => 'clobbered'], $flags);
                $out[] = "$flags:n=$n";
            } catch (\Error $e) {
                $out[] = "$flags:" . $e->getMessage();
            }
        }
        // $this survived every mode; only the prefixed form landed, as $p_this
        $out[] = get_class($this);
        $out[] = $p_this ?? 'MISS';
        return implode(' | ', $out);
    }
}
echo (new ExtractThisProbe)->run(), "\n";
// The mandatory "_" separator means no prefix can ever forge the name "this",
// so the prefixed store goes through: $thi_s, not $this.
$exth_try = function () {
    try {
        $n = extract(['s' => 1], EXTR_PREFIX_ALL, 'thi');
        return "$n|" . ($thi_s ?? 'MISS');
    } catch (\Error $e) {
        return $e->getMessage();
    }
};
echo $exth_try(), "\n";
?>
--EXPECT--
0:Cannot re-assign $this | 1:n=0 | 6:n=0 | 5:n=0 | 2:n=1 | ExtractThisProbe | clobbered
1|1
--CLEAN--
<?php
