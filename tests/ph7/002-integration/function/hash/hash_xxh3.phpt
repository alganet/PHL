--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
hash(): XXH3 in both widths, its two option sources and its streaming
--DESCRIPTION--
Composer hashes every dependency rule with xxh3 on PHP > 8.1, so a `composer
update` with no network at all died on `hash(): Argument #1 ($algo) must be a
valid hashing algorithm` before it could solve anything. The digests below cover
every branch of the algorithm -- the six length paths, the secret block, the
staging buffer -- and the three secret sources php exposes.
--FILE--
<?php
$data = '';
for ($i = 0; $i < 1200; $i++) {
    $data .= chr(($i * 37 + ($i >> 3)) & 0xff);
}
$secret = str_repeat('SeCrEt', 30);

echo "-- the extension's own inventory\n";
$algos = hash_algos();
$at = array_search('xxh32', $algos, true);
echo implode(', ', array_slice($algos, $at, 4)), "\n";
var_dump(in_array('xxh3', hash_hmac_algos(), true), in_array('xxh128', hash_hmac_algos(), true));
var_dump(strlen(hash('xxh3', '', true)), strlen(hash('xxh128', '', true)));

echo "-- every length path, unseeded / seeded / with a secret\n";
foreach ([0, 1, 2, 3, 4, 7, 8, 9, 15, 16, 17, 32, 64, 96, 128, 129, 160, 240,
          241, 256, 512, 700, 1023, 1024, 1025, 1200] as $n) {
    $in = substr($data, 0, $n);
    printf("%-5d %-16s %-16s %-16s %s\n", $n,
        hash('xxh3', $in),
        hash('xxh3', $in, false, ['seed' => 42]),
        hash('xxh3', $in, false, ['seed' => -7]),
        hash('xxh3', $in, false, ['secret' => $secret]));
}

echo "-- the 128-bit width, whose low half IS the 64-bit answer\n";
foreach ([0, 3, 8, 16, 64, 240, 241, 1024] as $n) {
    $in = substr($data, 0, $n);
    $wide = hash('xxh128', $in);
    printf("%-5d %s %s\n", $n, $wide, var_export(substr($wide, 16) === hash('xxh3', $in), true));
}

echo "-- streaming answers what one call does, at every chunk size\n";
$same = true;
foreach ([1, 7, 63, 64, 65, 127, 128, 255, 256, 257, 1000] as $chunk) {
    foreach ([0, 100, 240, 241, 256, 512, 700, 1024, 1200] as $n) {
        $in = substr($data, 0, $n);
        foreach ([[], ['seed' => 42], ['secret' => $secret]] as $options) {
            $ctx = hash_init('xxh128', 0, '', $options);
            for ($o = 0; $o < strlen($in); $o += $chunk) {
                hash_update($ctx, substr($in, $o, $chunk));
            }
            if (hash_final($ctx) !== hash('xxh128', $in, false, $options)) {
                printf("  MISMATCH chunk=%d len=%d options=%s\n", $chunk, $n, json_encode($options));
                $same = false;
            }
        }
    }
}
var_dump($same);

echo "-- hash_copy, hash_file and the raw form\n";
$ctx = hash_init('xxh3');
hash_update($ctx, 'abcdef');
$copy = hash_copy($ctx);
hash_update($copy, 'ghi');
hash_update($ctx, 'XYZ');
printf("%s %s\n", hash_final($ctx), hash_final($copy));
$file = sys_get_temp_dir() . '/phlt-xxh3-' . getmypid() . '.bin';
file_put_contents($file, substr($data, 0, 900));
printf("%s %s %s\n", hash_file('xxh3', $file), bin2hex(hash_file('xxh128', $file, true)),
    hash_file('xxh3', $file, false, ['secret' => $secret]));
unlink($file);
printf("%s %s\n", bin2hex(hash('xxh3', 'abc', true)), bin2hex(hash('xxh128', 'abc', true)));

echo "-- what the two options refuse\n";
error_reporting(E_ALL);
set_error_handler(function (int $n, string $m): bool { echo "  [$n] $m\n"; return true; });
function refused(string $label, callable $f): void
{
    try { $r = $f(); } catch (Throwable $e) { $r = get_class($e) . ': ' . $e->getMessage(); }
    printf("%-34s %s\n", $label, var_export($r, true));
}
refused('a secret one byte too short', fn() => hash('xxh3', 'abc', false, ['secret' => str_repeat('x', 135)]));
refused('...and the shortest one taken', fn() => hash('xxh3', 'abc', false, ['secret' => str_repeat('x', 136)]));
refused('xxh128 says its own name', fn() => hash('xxh128', 'abc', false, ['secret' => 'no']));
refused('a secret BESIDE a seed', fn() => hash('xxh3', 'abc', false, ['secret' => $secret, 'seed' => 42]));
refused('a secret that is not a string', fn() => hash('xxh3', 'abc', false, ['secret' => 42]));
refused('the conflict outranks the length', fn() => hash('xxh3', 'abc', false, ['secret' => 'no', 'seed' => 1]));
refused('...whichever was written first', fn() => hash('xxh3', 'abc', false, ['seed' => 1, 'secret' => 'no']));
refused('a row that reads no secret', fn() => hash('xxh64', 'abc', false, ['secret' => 'short', 'seed' => 2]));
refused('the MAC doors refuse the family', fn() => hash_hmac('xxh3', 'abc', 'key'));
refused('...and so do the KDFs', fn() => hash_pbkdf2('xxh128', 'p', 's', 1, 8));
refused('a context cannot be serialized', function () { return serialize(hash_init('xxh3')); });
refused('...nor the wide one', function () { return serialize(hash_init('xxh128')); });
refused('while xxh64 still can be', function () { return strlen(serialize(hash_init('xxh64'))) > 0; });
restore_error_handler();
--EXPECT--
-- the extension's own inventory
xxh32, xxh64, xxh3, xxh128
bool(false)
bool(false)
int(8)
int(16)
-- every length path, unseeded / seeded / with a secret
0     2d06800538d394c2 b029411ff43d84d2 098c452a82e89e99 edd8e990d76e89c4
1     c44bdff4074eecdb 5cf10f10bf2dd245 57dff54e3e745c5b 9ea1dcde207dd7c8
2     638de1946d9ee402 8afd7d6e8c60c6e9 72ef736227918319 c050e35b28d18330
3     2b15aa0b3d075427 a7b8766f49bdf306 885b629789469e96 67473b23f61cb964
4     e41090fa396e2123 dfd06b0112e18aaa 7f27afcbab67a62b dacab20009ae2e92
7     be6069e4a00347e8 621e54a03698e02e b224b004adb1757c 8ade58800720d371
8     44db4d702e7af307 c8ebb5421b8dfa6e 85a35b84d6ec601d 57994ea1760072ad
9     fb5805e2c810928f 61307d04f0f4e410 cf6444cdd6c3a66b 535c0e2b6e427837
15    9355c3b75cdc6663 f757cc65bc19dabc 79c0bb3c246e49c5 cc58f267b67da15b
16    0cc4a004b4e7d545 d0044eb4b493c107 405156d123d38dfd bad94d7f7a6e5e00
17    597daee786ee77d9 9d67ed9b105350d1 5664941251d7e656 0eb5885666ccbe34
32    8c27b263d1b372e6 63f15990e57c3abb 30a42ecfff43b15c 6edaec8a1c5201f9
64    47e69db4981d5293 f6246fd75026141f 2241ed88af1c4659 dcc8b547a3537fdb
96    42a3374ebf4d0990 cbfd4ce274df4129 553f77b488bbcdcf b8d37e7164542a6c
128   40c5a99a6d11ddfe 58489e1ae7e1327f 4c58f97d9230bb89 f632799be4a7d963
129   76fd29cc5330101b 3d38a8e55c5c0634 f12afa240eb4af40 0f66371236831f1f
160   eef0cf755ba68703 9b766a4db8abeda1 a591ceaa0ccf2329 ebdc4aae9bc95ad4
240   8ee4d7f29db1fd68 7eca2548a969b9ec 15b92941c931975e 5e31d942e5a5967f
241   6a9412ffe6402fc9 bd851037c9bcfc97 b3636841345baccc 1d6da20c48bd7bb8
256   4dcfda2fd4449486 c038b1bfd0064207 440322742cb44776 27d6ef3bf4f6e35c
512   ad8001e2c8b3ae9d 914ff8b04f8d28f6 27c1ebbb1a913369 ecec6ad1cd09804f
700   361b90665335622e d9a400e16b9d9770 54bc86f4a6696f70 956c3d9d303a76c9
1023  3adcf0dfac5a5c2b dbf10e6902859161 84ef6128b14489b3 b000852246ab9da5
1024  d5d26df17d4d21b8 68ac5128b7cd93b7 48ebdd1d833c185f a3fe37bdf11d7b02
1025  8cacb41a8bcd3fd5 7dc56a53c56578f9 1a1be9e7766faa50 ddf21d8f4cd0c2c1
1200  8c3829fd6cd4341a 17eecc6769e8b767 626379109a24f415 ae134f179239fe0d
-- the 128-bit width, whose low half IS the 64-bit answer
0     99aa06d3014798d86001c324468d497f false
3     9853135c5862576e2b15aa0b3d075427 true
8     fb45186b670a4b1895fdbe3342dcacd9 false
16    a4a1654dd6ca78fb600b6c9de2840f3e false
64    468c116bcdb0770761b1fdcab3c251f9 false
240   1797701d1d125765de6ab6698fed9bc5 false
241   bfc6af27c222ded86a9412ffe6402fc9 true
1024  55b1e70be25e7808d5d26df17d4d21b8 true
-- streaming answers what one call does, at every chunk size
bool(true)
-- hash_copy, hash_file and the raw form
5e0c23170be3b3fa e0dde4fc174590a0
15ecf328164ddb16 7e334b2ddea40dd315ecf328164ddb16 7f64d07b70d59c39
78af5f94892f3950 06b05ab6733a618578af5f94892f3950
-- what the two options refuse
a secret one byte too short        'Error: xxh3: Secret length must be >= 136 bytes, 135 bytes passed'
...and the shortest one taken      'fd6e986c28fac4d3'
xxh128 says its own name           'Error: xxh128: Secret length must be >= 136 bytes, 2 bytes passed'
a secret BESIDE a seed             'Error: xxh3: Only one of seed or secret is to be passed for initialization'
  [8192] hash(): Passing a secret of a type other than string is deprecated because it implicitly converts to a string, potentially hiding bugs
a secret that is not a string      'Error: xxh3: Secret length must be >= 136 bytes, 2 bytes passed'
the conflict outranks the length   'Error: xxh3: Only one of seed or secret is to be passed for initialization'
...whichever was written first     'Error: xxh3: Only one of seed or secret is to be passed for initialization'
a row that reads no secret         '53a0b8b27057daf7'
the MAC doors refuse the family    'ValueError: hash_hmac(): Argument #1 ($algo) must be a valid cryptographic hashing algorithm'
...and so do the KDFs              'ValueError: hash_pbkdf2(): Argument #1 ($algo) must be a valid cryptographic hashing algorithm'
a context cannot be serialized     'Exception: HashContext for algorithm "xxh3" cannot be serialized'
...nor the wide one                'Exception: HashContext for algorithm "xxh128" cannot be serialized'
while xxh64 still can be           true
