--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
metaphone encodes english phonemes the way php's traditional flavour does
--FILE--
<?php
// The traditional metaphone rules, php's flavour: 'X' encodes "sh", '0' "th".
foreach (["Thompson", "phone", "aggregate", "accuracy", "Knight", "gnome",
          "wright", "xavier", "czar", "judge", "rough", "cough", "dumb",
          "psychology", "school", "science", "Schmidt", "chemistry",
          "characters", "station", "vision", "quick", "rhythm", "lamb",
          "watch", "kitchen", "exhaust", "yellow", "day", "oedipus"] as $w) {
    echo $w, ": ", metaphone($w), "\n";
}
// A phrase is one run; non-letters break words but produce nothing.
var_dump(metaphone("wh questions"));
var_dump(metaphone("123 4main5"));
var_dump(metaphone(""));
var_dump(metaphone("!@#$"));
?>
--EXPECT--
Thompson: 0MPSN
phone: FN
aggregate: AKRKT
accuracy: AKKRS
Knight: NFT
gnome: NM
wright: RFT
xavier: SFR
czar: KSR
judge: JJ
rough: RF
cough: KF
dumb: TM
psychology: PSXLJ
school: SXL
science: SNS
Schmidt: SXMTT
chemistry: XMSTR
characters: XRKTRS
station: STXN
vision: FXN
quick: KK
rhythm: R0M
lamb: LM
watch: WX
kitchen: KXN
exhaust: EKSHST
yellow: YL
day: T
oedipus: OTPS
string(6) "WKSXNS"
string(2) "MN"
string(0) ""
string(0) ""
