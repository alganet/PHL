# src/sx/sxblowfish.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 168/170 lines (98.82%)

[Root index](../../index.md) | [Directory index](index.md)

|      Hits | Line | Source |
| --------: | ---: | :--- |
|         - |    1 | `/**` |
|         - |    2 | ` * SPDX-FileCopyrightText: 1997 Niels Provos <provos@physnet.uni-hamburg.de>` |
|         - |    3 | ` * SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>` |
|         - |    4 | ` * SPDX-License-Identifier: BSD-3-Clause` |
|         - |    5 | ` *` |
|         - |    6 | ` * Blowfish + Eksblowfish (bcrypt) — the crypto core behind the PHP password_*` |
|         - |    7 | ` * builtins. The algorithm follows Provos & Mazieres, "A Future-Adaptable` |
|         - |    8 | ` * Password Scheme" (OpenBSD bcrypt). PHL emits the corrected "$2y" variant:` |
|         - |    9 | ` * key bytes are read as unsigned (no sign extension).` |
|         - |   10 | ` */` |
|         - |   11 | `#include "sxtypes.h"` |
|         - |   12 | `#include "sxmacros.h"` |
|         - |   13 | `#include "sxstr.h"` |
|         - |   14 | `#include "sxblowfish.h"` |
|         - |   15 |  |
|         - |   16 | `#define BLF_N    16              /* Number of Subkeys */` |
|         - |   17 |  |
|         - |   18 | `typedef struct {` |
|         - |   19 | `	sxu32 S[4][256];             /* S-Boxes */` |
|         - |   20 | `	sxu32 P[BLF_N + 2];          /* Subkeys */` |
|         - |   21 | `} blf_ctx;` |
|         - |   22 |  |
|         - |   23 | `/* The standard Blowfish initial subkeys/S-boxes: the fractional part of pi. */` |
|         - |   24 | `static const sxu32 ORIG_P[BLF_N + 2] = {` |
|         - |   25 | `	0x243f6a88L, 0x85a308d3L, 0x13198a2eL, 0x03707344L,` |
|         - |   26 | `	0xa4093822L, 0x299f31d0L, 0x082efa98L, 0xec4e6c89L,` |
|         - |   27 | `	0x452821e6L, 0x38d01377L, 0xbe5466cfL, 0x34e90c6cL,` |
|         - |   28 | `	0xc0ac29b7L, 0xc97c50ddL, 0x3f84d5b5L, 0xb5470917L,` |
|         - |   29 | `	0x9216d5d9L, 0x8979fb1bL` |
|         - |   30 | `};` |
|         - |   31 |  |
|         - |   32 | `static const sxu32 ORIG_S[4][256] = {` |
|         - |   33 | `	{` |
|         - |   34 | `		0xd1310ba6L, 0x98dfb5acL, 0x2ffd72dbL, 0xd01adfb7L,` |
|         - |   35 | `		0xb8e1afedL, 0x6a267e96L, 0xba7c9045L, 0xf12c7f99L,` |
|         - |   36 | `		0x24a19947L, 0xb3916cf7L, 0x0801f2e2L, 0x858efc16L,` |
|         - |   37 | `		0x636920d8L, 0x71574e69L, 0xa458fea3L, 0xf4933d7eL,` |
|         - |   38 | `		0x0d95748fL, 0x728eb658L, 0x718bcd58L, 0x82154aeeL,` |
|         - |   39 | `		0x7b54a41dL, 0xc25a59b5L, 0x9c30d539L, 0x2af26013L,` |
|         - |   40 | `		0xc5d1b023L, 0x286085f0L, 0xca417918L, 0xb8db38efL,` |
|         - |   41 | `		0x8e79dcb0L, 0x603a180eL, 0x6c9e0e8bL, 0xb01e8a3eL,` |
|         - |   42 | `		0xd71577c1L, 0xbd314b27L, 0x78af2fdaL, 0x55605c60L,` |
|         - |   43 | `		0xe65525f3L, 0xaa55ab94L, 0x57489862L, 0x63e81440L,` |
|         - |   44 | `		0x55ca396aL, 0x2aab10b6L, 0xb4cc5c34L, 0x1141e8ceL,` |
|         - |   45 | `		0xa15486afL, 0x7c72e993L, 0xb3ee1411L, 0x636fbc2aL,` |
|         - |   46 | `		0x2ba9c55dL, 0x741831f6L, 0xce5c3e16L, 0x9b87931eL,` |
|         - |   47 | `		0xafd6ba33L, 0x6c24cf5cL, 0x7a325381L, 0x28958677L,` |
|         - |   48 | `		0x3b8f4898L, 0x6b4bb9afL, 0xc4bfe81bL, 0x66282193L,` |
|         - |   49 | `		0x61d809ccL, 0xfb21a991L, 0x487cac60L, 0x5dec8032L,` |
|         - |   50 | `		0xef845d5dL, 0xe98575b1L, 0xdc262302L, 0xeb651b88L,` |
|         - |   51 | `		0x23893e81L, 0xd396acc5L, 0x0f6d6ff3L, 0x83f44239L,` |
|         - |   52 | `		0x2e0b4482L, 0xa4842004L, 0x69c8f04aL, 0x9e1f9b5eL,` |
|         - |   53 | `		0x21c66842L, 0xf6e96c9aL, 0x670c9c61L, 0xabd388f0L,` |
|         - |   54 | `		0x6a51a0d2L, 0xd8542f68L, 0x960fa728L, 0xab5133a3L,` |
|         - |   55 | `		0x6eef0b6cL, 0x137a3be4L, 0xba3bf050L, 0x7efb2a98L,` |
|         - |   56 | `		0xa1f1651dL, 0x39af0176L, 0x66ca593eL, 0x82430e88L,` |
|         - |   57 | `		0x8cee8619L, 0x456f9fb4L, 0x7d84a5c3L, 0x3b8b5ebeL,` |
|         - |   58 | `		0xe06f75d8L, 0x85c12073L, 0x401a449fL, 0x56c16aa6L,` |
|         - |   59 | `		0x4ed3aa62L, 0x363f7706L, 0x1bfedf72L, 0x429b023dL,` |
|         - |   60 | `		0x37d0d724L, 0xd00a1248L, 0xdb0fead3L, 0x49f1c09bL,` |
|         - |   61 | `		0x075372c9L, 0x80991b7bL, 0x25d479d8L, 0xf6e8def7L,` |
|         - |   62 | `		0xe3fe501aL, 0xb6794c3bL, 0x976ce0bdL, 0x04c006baL,` |
|         - |   63 | `		0xc1a94fb6L, 0x409f60c4L, 0x5e5c9ec2L, 0x196a2463L,` |
|         - |   64 | `		0x68fb6fafL, 0x3e6c53b5L, 0x1339b2ebL, 0x3b52ec6fL,` |
|         - |   65 | `		0x6dfc511fL, 0x9b30952cL, 0xcc814544L, 0xaf5ebd09L,` |
|         - |   66 | `		0xbee3d004L, 0xde334afdL, 0x660f2807L, 0x192e4bb3L,` |
|         - |   67 | `		0xc0cba857L, 0x45c8740fL, 0xd20b5f39L, 0xb9d3fbdbL,` |
|         - |   68 | `		0x5579c0bdL, 0x1a60320aL, 0xd6a100c6L, 0x402c7279L,` |
|         - |   69 | `		0x679f25feL, 0xfb1fa3ccL, 0x8ea5e9f8L, 0xdb3222f8L,` |
|         - |   70 | `		0x3c7516dfL, 0xfd616b15L, 0x2f501ec8L, 0xad0552abL,` |
|         - |   71 | `		0x323db5faL, 0xfd238760L, 0x53317b48L, 0x3e00df82L,` |
|         - |   72 | `		0x9e5c57bbL, 0xca6f8ca0L, 0x1a87562eL, 0xdf1769dbL,` |
|         - |   73 | `		0xd542a8f6L, 0x287effc3L, 0xac6732c6L, 0x8c4f5573L,` |
|         - |   74 | `		0x695b27b0L, 0xbbca58c8L, 0xe1ffa35dL, 0xb8f011a0L,` |
|         - |   75 | `		0x10fa3d98L, 0xfd2183b8L, 0x4afcb56cL, 0x2dd1d35bL,` |
|         - |   76 | `		0x9a53e479L, 0xb6f84565L, 0xd28e49bcL, 0x4bfb9790L,` |
|         - |   77 | `		0xe1ddf2daL, 0xa4cb7e33L, 0x62fb1341L, 0xcee4c6e8L,` |
|         - |   78 | `		0xef20cadaL, 0x36774c01L, 0xd07e9efeL, 0x2bf11fb4L,` |
|         - |   79 | `		0x95dbda4dL, 0xae909198L, 0xeaad8e71L, 0x6b93d5a0L,` |
|         - |   80 | `		0xd08ed1d0L, 0xafc725e0L, 0x8e3c5b2fL, 0x8e7594b7L,` |
|         - |   81 | `		0x8ff6e2fbL, 0xf2122b64L, 0x8888b812L, 0x900df01cL,` |
|         - |   82 | `		0x4fad5ea0L, 0x688fc31cL, 0xd1cff191L, 0xb3a8c1adL,` |
|         - |   83 | `		0x2f2f2218L, 0xbe0e1777L, 0xea752dfeL, 0x8b021fa1L,` |
|         - |   84 | `		0xe5a0cc0fL, 0xb56f74e8L, 0x18acf3d6L, 0xce89e299L,` |
|         - |   85 | `		0xb4a84fe0L, 0xfd13e0b7L, 0x7cc43b81L, 0xd2ada8d9L,` |
|         - |   86 | `		0x165fa266L, 0x80957705L, 0x93cc7314L, 0x211a1477L,` |
|         - |   87 | `		0xe6ad2065L, 0x77b5fa86L, 0xc75442f5L, 0xfb9d35cfL,` |
|         - |   88 | `		0xebcdaf0cL, 0x7b3e89a0L, 0xd6411bd3L, 0xae1e7e49L,` |
|         - |   89 | `		0x00250e2dL, 0x2071b35eL, 0x226800bbL, 0x57b8e0afL,` |
|         - |   90 | `		0x2464369bL, 0xf009b91eL, 0x5563911dL, 0x59dfa6aaL,` |
|         - |   91 | `		0x78c14389L, 0xd95a537fL, 0x207d5ba2L, 0x02e5b9c5L,` |
|         - |   92 | `		0x83260376L, 0x6295cfa9L, 0x11c81968L, 0x4e734a41L,` |
|         - |   93 | `		0xb3472dcaL, 0x7b14a94aL, 0x1b510052L, 0x9a532915L,` |
|         - |   94 | `		0xd60f573fL, 0xbc9bc6e4L, 0x2b60a476L, 0x81e67400L,` |
|         - |   95 | `		0x08ba6fb5L, 0x571be91fL, 0xf296ec6bL, 0x2a0dd915L,` |
|         - |   96 | `		0xb6636521L, 0xe7b9f9b6L, 0xff34052eL, 0xc5855664L,` |
|         - |   97 | `		0x53b02d5dL, 0xa99f8fa1L, 0x08ba4799L, 0x6e85076aL` |
|         - |   98 | `	}, {` |
|         - |   99 | `		0x4b7a70e9L, 0xb5b32944L, 0xdb75092eL, 0xc4192623L,` |
|         - |  100 | `		0xad6ea6b0L, 0x49a7df7dL, 0x9cee60b8L, 0x8fedb266L,` |
|         - |  101 | `		0xecaa8c71L, 0x699a17ffL, 0x5664526cL, 0xc2b19ee1L,` |
|         - |  102 | `		0x193602a5L, 0x75094c29L, 0xa0591340L, 0xe4183a3eL,` |
|         - |  103 | `		0x3f54989aL, 0x5b429d65L, 0x6b8fe4d6L, 0x99f73fd6L,` |
|         - |  104 | `		0xa1d29c07L, 0xefe830f5L, 0x4d2d38e6L, 0xf0255dc1L,` |
|         - |  105 | `		0x4cdd2086L, 0x8470eb26L, 0x6382e9c6L, 0x021ecc5eL,` |
|         - |  106 | `		0x09686b3fL, 0x3ebaefc9L, 0x3c971814L, 0x6b6a70a1L,` |
|         - |  107 | `		0x687f3584L, 0x52a0e286L, 0xb79c5305L, 0xaa500737L,` |
|         - |  108 | `		0x3e07841cL, 0x7fdeae5cL, 0x8e7d44ecL, 0x5716f2b8L,` |
|         - |  109 | `		0xb03ada37L, 0xf0500c0dL, 0xf01c1f04L, 0x0200b3ffL,` |
|         - |  110 | `		0xae0cf51aL, 0x3cb574b2L, 0x25837a58L, 0xdc0921bdL,` |
|         - |  111 | `		0xd19113f9L, 0x7ca92ff6L, 0x94324773L, 0x22f54701L,` |
|         - |  112 | `		0x3ae5e581L, 0x37c2dadcL, 0xc8b57634L, 0x9af3dda7L,` |
|         - |  113 | `		0xa9446146L, 0x0fd0030eL, 0xecc8c73eL, 0xa4751e41L,` |
|         - |  114 | `		0xe238cd99L, 0x3bea0e2fL, 0x3280bba1L, 0x183eb331L,` |
|         - |  115 | `		0x4e548b38L, 0x4f6db908L, 0x6f420d03L, 0xf60a04bfL,` |
|         - |  116 | `		0x2cb81290L, 0x24977c79L, 0x5679b072L, 0xbcaf89afL,` |
|         - |  117 | `		0xde9a771fL, 0xd9930810L, 0xb38bae12L, 0xdccf3f2eL,` |
|         - |  118 | `		0x5512721fL, 0x2e6b7124L, 0x501adde6L, 0x9f84cd87L,` |
|         - |  119 | `		0x7a584718L, 0x7408da17L, 0xbc9f9abcL, 0xe94b7d8cL,` |
|         - |  120 | `		0xec7aec3aL, 0xdb851dfaL, 0x63094366L, 0xc464c3d2L,` |
|         - |  121 | `		0xef1c1847L, 0x3215d908L, 0xdd433b37L, 0x24c2ba16L,` |
|         - |  122 | `		0x12a14d43L, 0x2a65c451L, 0x50940002L, 0x133ae4ddL,` |
|         - |  123 | `		0x71dff89eL, 0x10314e55L, 0x81ac77d6L, 0x5f11199bL,` |
|         - |  124 | `		0x043556f1L, 0xd7a3c76bL, 0x3c11183bL, 0x5924a509L,` |
|         - |  125 | `		0xf28fe6edL, 0x97f1fbfaL, 0x9ebabf2cL, 0x1e153c6eL,` |
|         - |  126 | `		0x86e34570L, 0xeae96fb1L, 0x860e5e0aL, 0x5a3e2ab3L,` |
|         - |  127 | `		0x771fe71cL, 0x4e3d06faL, 0x2965dcb9L, 0x99e71d0fL,` |
|         - |  128 | `		0x803e89d6L, 0x5266c825L, 0x2e4cc978L, 0x9c10b36aL,` |
|         - |  129 | `		0xc6150ebaL, 0x94e2ea78L, 0xa5fc3c53L, 0x1e0a2df4L,` |
|         - |  130 | `		0xf2f74ea7L, 0x361d2b3dL, 0x1939260fL, 0x19c27960L,` |
|         - |  131 | `		0x5223a708L, 0xf71312b6L, 0xebadfe6eL, 0xeac31f66L,` |
|         - |  132 | `		0xe3bc4595L, 0xa67bc883L, 0xb17f37d1L, 0x018cff28L,` |
|         - |  133 | `		0xc332ddefL, 0xbe6c5aa5L, 0x65582185L, 0x68ab9802L,` |
|         - |  134 | `		0xeecea50fL, 0xdb2f953bL, 0x2aef7dadL, 0x5b6e2f84L,` |
|         - |  135 | `		0x1521b628L, 0x29076170L, 0xecdd4775L, 0x619f1510L,` |
|         - |  136 | `		0x13cca830L, 0xeb61bd96L, 0x0334fe1eL, 0xaa0363cfL,` |
|         - |  137 | `		0xb5735c90L, 0x4c70a239L, 0xd59e9e0bL, 0xcbaade14L,` |
|         - |  138 | `		0xeecc86bcL, 0x60622ca7L, 0x9cab5cabL, 0xb2f3846eL,` |
|         - |  139 | `		0x648b1eafL, 0x19bdf0caL, 0xa02369b9L, 0x655abb50L,` |
|         - |  140 | `		0x40685a32L, 0x3c2ab4b3L, 0x319ee9d5L, 0xc021b8f7L,` |
|         - |  141 | `		0x9b540b19L, 0x875fa099L, 0x95f7997eL, 0x623d7da8L,` |
|         - |  142 | `		0xf837889aL, 0x97e32d77L, 0x11ed935fL, 0x16681281L,` |
|         - |  143 | `		0x0e358829L, 0xc7e61fd6L, 0x96dedfa1L, 0x7858ba99L,` |
|         - |  144 | `		0x57f584a5L, 0x1b227263L, 0x9b83c3ffL, 0x1ac24696L,` |
|         - |  145 | `		0xcdb30aebL, 0x532e3054L, 0x8fd948e4L, 0x6dbc3128L,` |
|         - |  146 | `		0x58ebf2efL, 0x34c6ffeaL, 0xfe28ed61L, 0xee7c3c73L,` |
|         - |  147 | `		0x5d4a14d9L, 0xe864b7e3L, 0x42105d14L, 0x203e13e0L,` |
|         - |  148 | `		0x45eee2b6L, 0xa3aaabeaL, 0xdb6c4f15L, 0xfacb4fd0L,` |
|         - |  149 | `		0xc742f442L, 0xef6abbb5L, 0x654f3b1dL, 0x41cd2105L,` |
|         - |  150 | `		0xd81e799eL, 0x86854dc7L, 0xe44b476aL, 0x3d816250L,` |
|         - |  151 | `		0xcf62a1f2L, 0x5b8d2646L, 0xfc8883a0L, 0xc1c7b6a3L,` |
|         - |  152 | `		0x7f1524c3L, 0x69cb7492L, 0x47848a0bL, 0x5692b285L,` |
|         - |  153 | `		0x095bbf00L, 0xad19489dL, 0x1462b174L, 0x23820e00L,` |
|         - |  154 | `		0x58428d2aL, 0x0c55f5eaL, 0x1dadf43eL, 0x233f7061L,` |
|         - |  155 | `		0x3372f092L, 0x8d937e41L, 0xd65fecf1L, 0x6c223bdbL,` |
|         - |  156 | `		0x7cde3759L, 0xcbee7460L, 0x4085f2a7L, 0xce77326eL,` |
|         - |  157 | `		0xa6078084L, 0x19f8509eL, 0xe8efd855L, 0x61d99735L,` |
|         - |  158 | `		0xa969a7aaL, 0xc50c06c2L, 0x5a04abfcL, 0x800bcadcL,` |
|         - |  159 | `		0x9e447a2eL, 0xc3453484L, 0xfdd56705L, 0x0e1e9ec9L,` |
|         - |  160 | `		0xdb73dbd3L, 0x105588cdL, 0x675fda79L, 0xe3674340L,` |
|         - |  161 | `		0xc5c43465L, 0x713e38d8L, 0x3d28f89eL, 0xf16dff20L,` |
|         - |  162 | `		0x153e21e7L, 0x8fb03d4aL, 0xe6e39f2bL, 0xdb83adf7L` |
|         - |  163 | `	}, {` |
|         - |  164 | `		0xe93d5a68L, 0x948140f7L, 0xf64c261cL, 0x94692934L,` |
|         - |  165 | `		0x411520f7L, 0x7602d4f7L, 0xbcf46b2eL, 0xd4a20068L,` |
|         - |  166 | `		0xd4082471L, 0x3320f46aL, 0x43b7d4b7L, 0x500061afL,` |
|         - |  167 | `		0x1e39f62eL, 0x97244546L, 0x14214f74L, 0xbf8b8840L,` |
|         - |  168 | `		0x4d95fc1dL, 0x96b591afL, 0x70f4ddd3L, 0x66a02f45L,` |
|         - |  169 | `		0xbfbc09ecL, 0x03bd9785L, 0x7fac6dd0L, 0x31cb8504L,` |
|         - |  170 | `		0x96eb27b3L, 0x55fd3941L, 0xda2547e6L, 0xabca0a9aL,` |
|         - |  171 | `		0x28507825L, 0x530429f4L, 0x0a2c86daL, 0xe9b66dfbL,` |
|         - |  172 | `		0x68dc1462L, 0xd7486900L, 0x680ec0a4L, 0x27a18deeL,` |
|         - |  173 | `		0x4f3ffea2L, 0xe887ad8cL, 0xb58ce006L, 0x7af4d6b6L,` |
|         - |  174 | `		0xaace1e7cL, 0xd3375fecL, 0xce78a399L, 0x406b2a42L,` |
|         - |  175 | `		0x20fe9e35L, 0xd9f385b9L, 0xee39d7abL, 0x3b124e8bL,` |
|         - |  176 | `		0x1dc9faf7L, 0x4b6d1856L, 0x26a36631L, 0xeae397b2L,` |
|         - |  177 | `		0x3a6efa74L, 0xdd5b4332L, 0x6841e7f7L, 0xca7820fbL,` |
|         - |  178 | `		0xfb0af54eL, 0xd8feb397L, 0x454056acL, 0xba489527L,` |
|         - |  179 | `		0x55533a3aL, 0x20838d87L, 0xfe6ba9b7L, 0xd096954bL,` |
|         - |  180 | `		0x55a867bcL, 0xa1159a58L, 0xcca92963L, 0x99e1db33L,` |
|         - |  181 | `		0xa62a4a56L, 0x3f3125f9L, 0x5ef47e1cL, 0x9029317cL,` |
|         - |  182 | `		0xfdf8e802L, 0x04272f70L, 0x80bb155cL, 0x05282ce3L,` |
|         - |  183 | `		0x95c11548L, 0xe4c66d22L, 0x48c1133fL, 0xc70f86dcL,` |
|         - |  184 | `		0x07f9c9eeL, 0x41041f0fL, 0x404779a4L, 0x5d886e17L,` |
|         - |  185 | `		0x325f51ebL, 0xd59bc0d1L, 0xf2bcc18fL, 0x41113564L,` |
|         - |  186 | `		0x257b7834L, 0x602a9c60L, 0xdff8e8a3L, 0x1f636c1bL,` |
|         - |  187 | `		0x0e12b4c2L, 0x02e1329eL, 0xaf664fd1L, 0xcad18115L,` |
|         - |  188 | `		0x6b2395e0L, 0x333e92e1L, 0x3b240b62L, 0xeebeb922L,` |
|         - |  189 | `		0x85b2a20eL, 0xe6ba0d99L, 0xde720c8cL, 0x2da2f728L,` |
|         - |  190 | `		0xd0127845L, 0x95b794fdL, 0x647d0862L, 0xe7ccf5f0L,` |
|         - |  191 | `		0x5449a36fL, 0x877d48faL, 0xc39dfd27L, 0xf33e8d1eL,` |
|         - |  192 | `		0x0a476341L, 0x992eff74L, 0x3a6f6eabL, 0xf4f8fd37L,` |
|         - |  193 | `		0xa812dc60L, 0xa1ebddf8L, 0x991be14cL, 0xdb6e6b0dL,` |
|         - |  194 | `		0xc67b5510L, 0x6d672c37L, 0x2765d43bL, 0xdcd0e804L,` |
|         - |  195 | `		0xf1290dc7L, 0xcc00ffa3L, 0xb5390f92L, 0x690fed0bL,` |
|         - |  196 | `		0x667b9ffbL, 0xcedb7d9cL, 0xa091cf0bL, 0xd9155ea3L,` |
|         - |  197 | `		0xbb132f88L, 0x515bad24L, 0x7b9479bfL, 0x763bd6ebL,` |
|         - |  198 | `		0x37392eb3L, 0xcc115979L, 0x8026e297L, 0xf42e312dL,` |
|         - |  199 | `		0x6842ada7L, 0xc66a2b3bL, 0x12754cccL, 0x782ef11cL,` |
|         - |  200 | `		0x6a124237L, 0xb79251e7L, 0x06a1bbe6L, 0x4bfb6350L,` |
|         - |  201 | `		0x1a6b1018L, 0x11caedfaL, 0x3d25bdd8L, 0xe2e1c3c9L,` |
|         - |  202 | `		0x44421659L, 0x0a121386L, 0xd90cec6eL, 0xd5abea2aL,` |
|         - |  203 | `		0x64af674eL, 0xda86a85fL, 0xbebfe988L, 0x64e4c3feL,` |
|         - |  204 | `		0x9dbc8057L, 0xf0f7c086L, 0x60787bf8L, 0x6003604dL,` |
|         - |  205 | `		0xd1fd8346L, 0xf6381fb0L, 0x7745ae04L, 0xd736fcccL,` |
|         - |  206 | `		0x83426b33L, 0xf01eab71L, 0xb0804187L, 0x3c005e5fL,` |
|         - |  207 | `		0x77a057beL, 0xbde8ae24L, 0x55464299L, 0xbf582e61L,` |
|         - |  208 | `		0x4e58f48fL, 0xf2ddfda2L, 0xf474ef38L, 0x8789bdc2L,` |
|         - |  209 | `		0x5366f9c3L, 0xc8b38e74L, 0xb475f255L, 0x46fcd9b9L,` |
|         - |  210 | `		0x7aeb2661L, 0x8b1ddf84L, 0x846a0e79L, 0x915f95e2L,` |
|         - |  211 | `		0x466e598eL, 0x20b45770L, 0x8cd55591L, 0xc902de4cL,` |
|         - |  212 | `		0xb90bace1L, 0xbb8205d0L, 0x11a86248L, 0x7574a99eL,` |
|         - |  213 | `		0xb77f19b6L, 0xe0a9dc09L, 0x662d09a1L, 0xc4324633L,` |
|         - |  214 | `		0xe85a1f02L, 0x09f0be8cL, 0x4a99a025L, 0x1d6efe10L,` |
|         - |  215 | `		0x1ab93d1dL, 0x0ba5a4dfL, 0xa186f20fL, 0x2868f169L,` |
|         - |  216 | `		0xdcb7da83L, 0x573906feL, 0xa1e2ce9bL, 0x4fcd7f52L,` |
|         - |  217 | `		0x50115e01L, 0xa70683faL, 0xa002b5c4L, 0x0de6d027L,` |
|         - |  218 | `		0x9af88c27L, 0x773f8641L, 0xc3604c06L, 0x61a806b5L,` |
|         - |  219 | `		0xf0177a28L, 0xc0f586e0L, 0x006058aaL, 0x30dc7d62L,` |
|         - |  220 | `		0x11e69ed7L, 0x2338ea63L, 0x53c2dd94L, 0xc2c21634L,` |
|         - |  221 | `		0xbbcbee56L, 0x90bcb6deL, 0xebfc7da1L, 0xce591d76L,` |
|         - |  222 | `		0x6f05e409L, 0x4b7c0188L, 0x39720a3dL, 0x7c927c24L,` |
|         - |  223 | `		0x86e3725fL, 0x724d9db9L, 0x1ac15bb4L, 0xd39eb8fcL,` |
|         - |  224 | `		0xed545578L, 0x08fca5b5L, 0xd83d7cd3L, 0x4dad0fc4L,` |
|         - |  225 | `		0x1e50ef5eL, 0xb161e6f8L, 0xa28514d9L, 0x6c51133cL,` |
|         - |  226 | `		0x6fd5c7e7L, 0x56e14ec4L, 0x362abfceL, 0xddc6c837L,` |
|         - |  227 | `		0xd79a3234L, 0x92638212L, 0x670efa8eL, 0x406000e0L` |
|         - |  228 | `	}, {` |
|         - |  229 | `		0x3a39ce37L, 0xd3faf5cfL, 0xabc27737L, 0x5ac52d1bL,` |
|         - |  230 | `		0x5cb0679eL, 0x4fa33742L, 0xd3822740L, 0x99bc9bbeL,` |
|         - |  231 | `		0xd5118e9dL, 0xbf0f7315L, 0xd62d1c7eL, 0xc700c47bL,` |
|         - |  232 | `		0xb78c1b6bL, 0x21a19045L, 0xb26eb1beL, 0x6a366eb4L,` |
|         - |  233 | `		0x5748ab2fL, 0xbc946e79L, 0xc6a376d2L, 0x6549c2c8L,` |
|         - |  234 | `		0x530ff8eeL, 0x468dde7dL, 0xd5730a1dL, 0x4cd04dc6L,` |
|         - |  235 | `		0x2939bbdbL, 0xa9ba4650L, 0xac9526e8L, 0xbe5ee304L,` |
|         - |  236 | `		0xa1fad5f0L, 0x6a2d519aL, 0x63ef8ce2L, 0x9a86ee22L,` |
|         - |  237 | `		0xc089c2b8L, 0x43242ef6L, 0xa51e03aaL, 0x9cf2d0a4L,` |
|         - |  238 | `		0x83c061baL, 0x9be96a4dL, 0x8fe51550L, 0xba645bd6L,` |
|         - |  239 | `		0x2826a2f9L, 0xa73a3ae1L, 0x4ba99586L, 0xef5562e9L,` |
|         - |  240 | `		0xc72fefd3L, 0xf752f7daL, 0x3f046f69L, 0x77fa0a59L,` |
|         - |  241 | `		0x80e4a915L, 0x87b08601L, 0x9b09e6adL, 0x3b3ee593L,` |
|         - |  242 | `		0xe990fd5aL, 0x9e34d797L, 0x2cf0b7d9L, 0x022b8b51L,` |
|         - |  243 | `		0x96d5ac3aL, 0x017da67dL, 0xd1cf3ed6L, 0x7c7d2d28L,` |
|         - |  244 | `		0x1f9f25cfL, 0xadf2b89bL, 0x5ad6b472L, 0x5a88f54cL,` |
|         - |  245 | `		0xe029ac71L, 0xe019a5e6L, 0x47b0acfdL, 0xed93fa9bL,` |
|         - |  246 | `		0xe8d3c48dL, 0x283b57ccL, 0xf8d56629L, 0x79132e28L,` |
|         - |  247 | `		0x785f0191L, 0xed756055L, 0xf7960e44L, 0xe3d35e8cL,` |
|         - |  248 | `		0x15056dd4L, 0x88f46dbaL, 0x03a16125L, 0x0564f0bdL,` |
|         - |  249 | `		0xc3eb9e15L, 0x3c9057a2L, 0x97271aecL, 0xa93a072aL,` |
|         - |  250 | `		0x1b3f6d9bL, 0x1e6321f5L, 0xf59c66fbL, 0x26dcf319L,` |
|         - |  251 | `		0x7533d928L, 0xb155fdf5L, 0x03563482L, 0x8aba3cbbL,` |
|         - |  252 | `		0x28517711L, 0xc20ad9f8L, 0xabcc5167L, 0xccad925fL,` |
|         - |  253 | `		0x4de81751L, 0x3830dc8eL, 0x379d5862L, 0x9320f991L,` |
|         - |  254 | `		0xea7a90c2L, 0xfb3e7bceL, 0x5121ce64L, 0x774fbe32L,` |
|         - |  255 | `		0xa8b6e37eL, 0xc3293d46L, 0x48de5369L, 0x6413e680L,` |
|         - |  256 | `		0xa2ae0810L, 0xdd6db224L, 0x69852dfdL, 0x09072166L,` |
|         - |  257 | `		0xb39a460aL, 0x6445c0ddL, 0x586cdecfL, 0x1c20c8aeL,` |
|         - |  258 | `		0x5bbef7ddL, 0x1b588d40L, 0xccd2017fL, 0x6bb4e3bbL,` |
|         - |  259 | `		0xdda26a7eL, 0x3a59ff45L, 0x3e350a44L, 0xbcb4cdd5L,` |
|         - |  260 | `		0x72eacea8L, 0xfa6484bbL, 0x8d6612aeL, 0xbf3c6f47L,` |
|         - |  261 | `		0xd29be463L, 0x542f5d9eL, 0xaec2771bL, 0xf64e6370L,` |
|         - |  262 | `		0x740e0d8dL, 0xe75b1357L, 0xf8721671L, 0xaf537d5dL,` |
|         - |  263 | `		0x4040cb08L, 0x4eb4e2ccL, 0x34d2466aL, 0x0115af84L,` |
|         - |  264 | `		0xe1b00428L, 0x95983a1dL, 0x06b89fb4L, 0xce6ea048L,` |
|         - |  265 | `		0x6f3f3b82L, 0x3520ab82L, 0x011a1d4bL, 0x277227f8L,` |
|         - |  266 | `		0x611560b1L, 0xe7933fdcL, 0xbb3a792bL, 0x344525bdL,` |
|         - |  267 | `		0xa08839e1L, 0x51ce794bL, 0x2f32c9b7L, 0xa01fbac9L,` |
|         - |  268 | `		0xe01cc87eL, 0xbcc7d1f6L, 0xcf0111c3L, 0xa1e8aac7L,` |
|         - |  269 | `		0x1a908749L, 0xd44fbd9aL, 0xd0dadecbL, 0xd50ada38L,` |
|         - |  270 | `		0x0339c32aL, 0xc6913667L, 0x8df9317cL, 0xe0b12b4fL,` |
|         - |  271 | `		0xf79e59b7L, 0x43f5bb3aL, 0xf2d519ffL, 0x27d9459cL,` |
|         - |  272 | `		0xbf97222cL, 0x15e6fc2aL, 0x0f91fc71L, 0x9b941525L,` |
|         - |  273 | `		0xfae59361L, 0xceb69cebL, 0xc2a86459L, 0x12baa8d1L,` |
|         - |  274 | `		0xb6c1075eL, 0xe3056a0cL, 0x10d25065L, 0xcb03a442L,` |
|         - |  275 | `		0xe0ec6e0eL, 0x1698db3bL, 0x4c98a0beL, 0x3278e964L,` |
|         - |  276 | `		0x9f1f9532L, 0xe0d392dfL, 0xd3a0342bL, 0x8971f21eL,` |
|         - |  277 | `		0x1b0a7441L, 0x4ba3348cL, 0xc5be7120L, 0xc37632d8L,` |
|         - |  278 | `		0xdf359f8dL, 0x9b992f2eL, 0xe60b6f47L, 0x0fe3f11dL,` |
|         - |  279 | `		0xe54cda54L, 0x1edad891L, 0xce6279cfL, 0xcd3e7e6fL,` |
|         - |  280 | `		0x1618b166L, 0xfd2c1d05L, 0x848fd2c5L, 0xf6fb2299L,` |
|         - |  281 | `		0xf523f357L, 0xa6327623L, 0x93a83531L, 0x56cccd02L,` |
|         - |  282 | `		0xacf08162L, 0x5a75ebb5L, 0x6e163697L, 0x88d273ccL,` |
|         - |  283 | `		0xde966292L, 0x81b949d0L, 0x4c50901bL, 0x71c65614L,` |
|         - |  284 | `		0xe6c6c7bdL, 0x327a140aL, 0x45e1d006L, 0xc3f27b9aL,` |
|         - |  285 | `		0xc9aa53fdL, 0x62a80f00L, 0xbb25bfe2L, 0x35bdd2f6L,` |
|         - |  286 | `		0x71126905L, 0xb2040222L, 0xb6cbcf7cL, 0xcd769c2bL,` |
|         - |  287 | `		0x53113ec0L, 0x1640e3d3L, 0x38abbd60L, 0x2547adf0L,` |
|         - |  288 | `		0xba38209cL, 0xf746ce76L, 0x77afa1c5L, 0x20756060L,` |
|         - |  289 | `		0x85cbfe4eL, 0x8ae88dd8L, 0x7aaaf9b0L, 0x4cf9aa7eL,` |
|         - |  290 | `		0x1948c25cL, 0x02fb8a8cL, 0x01c36ae4L, 0xd6ebe1f9L,` |
|         - |  291 | `		0x90d4f869L, 0xa65cdea0L, 0x3f09252dL, 0xc208e69fL,` |
|         - |  292 | `		0xb74e6132L, 0xce77e25bL, 0x578fdfe3L, 0x3ac372e6L` |
|         - |  293 | `	}` |
|         - |  294 | `};` |
|         - |  295 |  |
|         - |  296 | `/* The Blowfish F function: 4-byte split, two S-box adds with one xor. */` |
| 214277345 |  297 | `static sxu32 Blowfish_F(blf_ctx *c,sxu32 x){` |
| 214277345 |  298 | `	sxu32 a = (x >> 24) & 0xff;` |
| 214277345 |  299 | `	sxu32 b = (x >> 16) & 0xff;` |
| 214277345 |  300 | `	sxu32 cc = (x >> 8) & 0xff;` |
| 214277345 |  301 | `	sxu32 d = x & 0xff;` |
| 214277345 |  302 | `	sxu32 y = c->S[0][a] + c->S[1][b];` |
| 214277345 |  303 | `	y = y ^ c->S[2][cc];` |
| 214277345 |  304 | `	y = y + c->S[3][d];` |
| 214277345 |  305 | `	return y;` |
|         1 |  306 | `}` |
|         - |  307 | `/* Encrypt one 64-bit block (xl\|\|xr) in place. */` |
|  13392335 |  308 | `static void Blowfish_encipher(blf_ctx *c,sxu32 *xl,sxu32 *xr){` |
|  13392335 |  309 | `	sxu32 Xl = *xl,Xr = *xr,temp;` |
|         - |  310 | `	int i;` |
| 227669679 |  311 | `	for( i = 0; i < BLF_N; i++ ){` |
| 214277345 |  312 | `		Xl = Xl ^ c->P[i];` |
| 214277345 |  313 | `		Xr = Blowfish_F(c,Xl) ^ Xr;` |
| 214277345 |  314 | `		temp = Xl; Xl = Xr; Xr = temp;   /* swap */` |
| 107138673 |  315 | `	}` |
|  13392335 |  316 | `	temp = Xl; Xl = Xr; Xr = temp;       /* undo the final swap */` |
|  13392335 |  317 | `	Xr = Xr ^ c->P[BLF_N];` |
|  13392335 |  318 | `	Xl = Xl ^ c->P[BLF_N + 1];` |
|  13392335 |  319 | `	*xl = Xl; *xr = Xr;` |
|  13392335 |  320 | `}` |
|         - |  321 | `/* Encrypt nBlocks consecutive 64-bit blocks (ECB). */` |
|      1921 |  322 | `static void Blowfish_encrypt(blf_ctx *c,sxu32 *data,int nBlocks){` |
|         - |  323 | `	int i;` |
|      7681 |  324 | `	for( i = 0; i < nBlocks; i++ ){` |
|      5761 |  325 | `		Blowfish_encipher(c,&data[i*2],&data[i*2+1]);` |
|      2881 |  326 | `	}` |
|      1921 |  327 | `}` |
|         - |  328 | `/* Read 4 bytes (big-endian) from data, cycling at databytes; advance *pCur.` |
|         - |  329 | ` * Bytes are unsigned (the corrected "$2y" behaviour — no sign extension). */` |
|    493933 |  330 | `static sxu32 Blowfish_stream2word(const unsigned char *data,sxu32 databytes,sxu32 *pCur){` |
|         - |  331 | `	int i;` |
|    493933 |  332 | `	sxu32 temp = 0,j = *pCur;` |
|   2469661 |  333 | `	for( i = 0; i < 4; i++,j++ ){` |
|   1975729 |  334 | `		if( j >= databytes ){ j = 0; }` |
|   1975729 |  335 | `		temp = (temp << 8) \| (sxu32)data[j];` |
|    987865 |  336 | `	}` |
|    493933 |  337 | `	*pCur = j;` |
|    493933 |  338 | `	return temp;` |
|         1 |  339 | `}` |
|        31 |  340 | `static void Blowfish_initstate(blf_ctx *c){` |
|        31 |  341 | `	SyMemcpy((const void *)ORIG_S,(void *)c->S,sizeof(c->S));` |
|        31 |  342 | `	SyMemcpy((const void *)ORIG_P,(void *)c->P,sizeof(c->P));` |
|        31 |  343 | `}` |
|         - |  344 | `/* Standard (unsalted) key expansion. */` |
|     25665 |  345 | `static void Blowfish_expand0state(blf_ctx *c,const unsigned char *key,sxu32 keybytes){` |
|         - |  346 | `	int i,k;` |
|         - |  347 | `	sxu32 j,datal,datar,temp;` |
|     25665 |  348 | `	j = 0;` |
|    487617 |  349 | `	for( i = 0; i < BLF_N + 2; i++ ){` |
|    461953 |  350 | `		temp = Blowfish_stream2word(key,keybytes,&j);` |
|    461953 |  351 | `		c->P[i] = c->P[i] ^ temp;` |
|    230977 |  352 | `	}` |
|     25665 |  353 | `	datal = datar = 0;` |
|    256641 |  354 | `	for( i = 0; i < BLF_N + 2; i += 2 ){` |
|    230977 |  355 | `		Blowfish_encipher(c,&datal,&datar);` |
|    230977 |  356 | `		c->P[i] = datal; c->P[i+1] = datar;` |
|    115489 |  357 | `	}` |
|    128321 |  358 | `	for( i = 0; i < 4; i++ ){` |
|  13242625 |  359 | `		for( k = 0; k < 256; k += 2 ){` |
|  13139969 |  360 | `			Blowfish_encipher(c,&datal,&datar);` |
|  13139969 |  361 | `			c->S[i][k] = datal; c->S[i][k+1] = datar;` |
|   6569985 |  362 | `		}` |
|     51329 |  363 | `	}` |
|     25665 |  364 | `}` |
|         - |  365 | `/* Salted "expensive" key expansion (the bcrypt ExpandKey). */` |
|        30 |  366 | `static void Blowfish_expandstate(blf_ctx *c,const unsigned char *data,sxu32 databytes,` |
|         1 |  367 | `	const unsigned char *key,sxu32 keybytes){` |
|         - |  368 | `	int i,k;` |
|         - |  369 | `	sxu32 j,datal,datar,temp;` |
|        31 |  370 | `	j = 0;` |
|       571 |  371 | `	for( i = 0; i < BLF_N + 2; i++ ){` |
|       541 |  372 | `		temp = Blowfish_stream2word(key,keybytes,&j);` |
|       541 |  373 | `		c->P[i] = c->P[i] ^ temp;` |
|       271 |  374 | `	}` |
|        31 |  375 | `	datal = datar = 0;` |
|        31 |  376 | `	j = 0;` |
|       301 |  377 | `	for( i = 0; i < BLF_N + 2; i += 2 ){` |
|       271 |  378 | `		datal ^= Blowfish_stream2word(data,databytes,&j);` |
|       271 |  379 | `		datar ^= Blowfish_stream2word(data,databytes,&j);` |
|       271 |  380 | `		Blowfish_encipher(c,&datal,&datar);` |
|       271 |  381 | `		c->P[i] = datal; c->P[i+1] = datar;` |
|       136 |  382 | `	}` |
|       151 |  383 | `	for( i = 0; i < 4; i++ ){` |
|     15481 |  384 | `		for( k = 0; k < 256; k += 2 ){` |
|     15361 |  385 | `			datal ^= Blowfish_stream2word(data,databytes,&j);` |
|     15361 |  386 | `			datar ^= Blowfish_stream2word(data,databytes,&j);` |
|     15361 |  387 | `			Blowfish_encipher(c,&datal,&datar);` |
|     15361 |  388 | `			c->S[i][k] = datal; c->S[i][k+1] = datar;` |
|      7681 |  389 | `		}` |
|        61 |  390 | `	}` |
|        31 |  391 | `}` |
|         - |  392 |  |
|         - |  393 | `/* bcrypt-base64 alphabet: '.','/', then A-Z, a-z, 0-9. */` |
|         - |  394 | `static const char zB64[] = "./ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";` |
|         - |  395 | `/* Map a bcrypt-base64 character to its 0..63 value, or 255 if invalid. */` |
|       397 |  396 | `static int BcryptB64Value(int c){` |
|       397 |  397 | `	const char *p = zB64;` |
|       397 |  398 | `	int i = 0;` |
|      9298 |  399 | `	for(; i < 64; i++ ){` |
|      9298 |  400 | `		if( p[i] == c ){ return i; }` |
|      4729 |  401 | `	}` |
|       ! 0 |  402 | `	return 255;` |
|       199 |  403 | `}` |
|         - |  404 | `/* Encode nIn bytes as bcrypt-base64 into zOut (no padding); returns char count. */` |
|        61 |  405 | `static int BcryptB64Encode(char *zOut,const unsigned char *pIn,sxu32 nIn){` |
|        61 |  406 | `	sxu32 i = 0;` |
|        61 |  407 | `	int n = 0;` |
|         - |  408 | `	unsigned int c1,c2;` |
|       421 |  409 | `	while( i < nIn ){` |
|       421 |  410 | `		c1 = pIn[i++];` |
|       421 |  411 | `		zOut[n++] = zB64[(c1 >> 2) & 0x3f];` |
|       421 |  412 | `		c1 = (c1 & 0x03) << 4;` |
|       421 |  413 | `		if( i >= nIn ){ zOut[n++] = zB64[c1 & 0x3f]; break; }` |
|       391 |  414 | `		c2 = pIn[i++];` |
|       391 |  415 | `		c1 \|= (c2 >> 4) & 0x0f;` |
|       391 |  416 | `		zOut[n++] = zB64[c1 & 0x3f];` |
|       391 |  417 | `		c1 = (c2 & 0x0f) << 2;` |
|       391 |  418 | `		if( i >= nIn ){ zOut[n++] = zB64[c1 & 0x3f]; break; }` |
|       361 |  419 | `		c2 = pIn[i++];` |
|       361 |  420 | `		c1 \|= (c2 >> 6) & 0x03;` |
|       361 |  421 | `		zOut[n++] = zB64[c1 & 0x3f];` |
|       361 |  422 | `		zOut[n++] = zB64[c2 & 0x3f];` |
|         1 |  423 | `	}` |
|        61 |  424 | `	return n;` |
|         1 |  425 | `}` |
|        19 |  426 | `PH7_PRIVATE sxi32 SyBcryptB64Decode(const char *zIn,sxu32 nIn,unsigned char *pOut,sxu32 nOut){` |
|        19 |  427 | `	sxu32 i = 0,o = 0;` |
|         - |  428 | `	int c1,c2,c3,c4;` |
|       109 |  429 | `	while( o < nOut ){` |
|       109 |  430 | `		if( i + 1 >= nIn ){ return SXERR_INVALID; }` |
|       109 |  431 | `		c1 = BcryptB64Value(zIn[i]); c2 = BcryptB64Value(zIn[i+1]);` |
|       109 |  432 | `		if( c1 == 255 \|\| c2 == 255 ){ return SXERR_INVALID; }` |
|       109 |  433 | `		pOut[o++] = (unsigned char)((c1 << 2) \| ((c2 & 0x30) >> 4));` |
|       109 |  434 | `		if( o >= nOut ){ break; }` |
|        91 |  435 | `		if( i + 2 >= nIn ){ return SXERR_INVALID; }` |
|        91 |  436 | `		c3 = BcryptB64Value(zIn[i+2]);` |
|        91 |  437 | `		if( c3 == 255 ){ return SXERR_INVALID; }` |
|        91 |  438 | `		pOut[o++] = (unsigned char)(((c2 & 0x0f) << 4) \| ((c3 & 0x3c) >> 2));` |
|        91 |  439 | `		if( o >= nOut ){ break; }` |
|        91 |  440 | `		if( i + 3 >= nIn ){ return SXERR_INVALID; }` |
|        91 |  441 | `		c4 = BcryptB64Value(zIn[i+3]);` |
|        91 |  442 | `		if( c4 == 255 ){ return SXERR_INVALID; }` |
|        91 |  443 | `		pOut[o++] = (unsigned char)(((c3 & 0x03) << 6) \| c4);` |
|        91 |  444 | `		i += 4;` |
|         1 |  445 | `	}` |
|        19 |  446 | `	return SXRET_OK;` |
|        10 |  447 | `}` |
|         - |  448 |  |
|        30 |  449 | `PH7_PRIVATE sxi32 SyBcryptHash(const unsigned char *pPwd,sxu32 nPwd,sxu32 nCost,` |
|         1 |  450 | `	const unsigned char aSalt[16],char zOut[60]){` |
|         - |  451 | `	blf_ctx state;` |
|         - |  452 | `	/* "OrpheanBeholderScryDoubt" = 24 bytes = 6 big-endian words. */` |
|         - |  453 | `	static const unsigned char zMagic[24] = {` |
|         - |  454 | `		'O','r','p','h','e','a','n','B','e','h','o','l','d','e','r',` |
|         - |  455 | `		'S','c','r','y','D','o','u','b','t'` |
|         - |  456 | `	};` |
|         - |  457 | `	sxu32 cdata[6];` |
|         - |  458 | `	unsigned char zCipher[24];` |
|         - |  459 | `	unsigned char zKey[73];` |
|         - |  460 | `	sxu32 keylen,j,rounds,k;` |
|         - |  461 | `	int i,n;` |
|        31 |  462 | `	if( nCost < 4 \|\| nCost > 31 ){` |
|       ! 0 |  463 | `		return SXERR_INVALID;` |
|         - |  464 | `	}` |
|         - |  465 | `	/* Key = password bytes + a trailing NUL, capped at 72 bytes total. The whole` |
|         - |  466 | `	 * buffer is zeroed first: only [0,keylen) is ever read (cyclically), but a` |
|         - |  467 | `	 * full init keeps -Wmaybe-uninitialized quiet when the loop is inlined. */` |
|        31 |  468 | `	SyZero(zKey,(sxu32)sizeof(zKey));` |
|        31 |  469 | `	keylen = nPwd + 1;` |
|        31 |  470 | `	if( keylen > 72 ){ keylen = 72; }` |
|       455 |  471 | `	for( j = 0; j < keylen; j++ ){` |
|       425 |  472 | `		zKey[j] = (j < nPwd) ? pPwd[j] : 0;` |
|       213 |  473 | `	}` |
|         - |  474 | `	/* EksBlowfishSetup */` |
|        31 |  475 | `	Blowfish_initstate(&state);` |
|        31 |  476 | `	Blowfish_expandstate(&state,aSalt,16,zKey,keylen);` |
|        31 |  477 | `	rounds = (sxu32)((sxu64)1 << nCost);` |
|     12863 |  478 | `	for( k = 0; k < rounds; k++ ){` |
|     12833 |  479 | `		Blowfish_expand0state(&state,zKey,keylen);` |
|     12833 |  480 | `		Blowfish_expand0state(&state,aSalt,16);` |
|      6417 |  481 | `	}` |
|         - |  482 | `	/* Encrypt the magic string 64 times (3 blocks each). */` |
|        31 |  483 | `	j = 0;` |
|       211 |  484 | `	for( i = 0; i < 6; i++ ){` |
|       181 |  485 | `		cdata[i] = Blowfish_stream2word(zMagic,24,&j);` |
|        91 |  486 | `	}` |
|      1951 |  487 | `	for( k = 0; k < 64; k++ ){` |
|      1921 |  488 | `		Blowfish_encrypt(&state,cdata,3);` |
|       961 |  489 | `	}` |
|       211 |  490 | `	for( i = 0; i < 6; i++ ){` |
|       181 |  491 | `		zCipher[i*4]   = (unsigned char)((cdata[i] >> 24) & 0xff);` |
|       181 |  492 | `		zCipher[i*4+1] = (unsigned char)((cdata[i] >> 16) & 0xff);` |
|       181 |  493 | `		zCipher[i*4+2] = (unsigned char)((cdata[i] >> 8) & 0xff);` |
|       181 |  494 | `		zCipher[i*4+3] = (unsigned char)(cdata[i] & 0xff);` |
|        91 |  495 | `	}` |
|         - |  496 | `	/* Assemble "$2y$CC$" + base64(salt,16)=22 + base64(cipher,23)=31 = 60. */` |
|        31 |  497 | `	n = 0;` |
|        31 |  498 | `	zOut[n++] = '$'; zOut[n++] = '2'; zOut[n++] = 'y'; zOut[n++] = '$';` |
|        31 |  499 | `	zOut[n++] = (char)('0' + (nCost / 10));` |
|        31 |  500 | `	zOut[n++] = (char)('0' + (nCost % 10));` |
|        31 |  501 | `	zOut[n++] = '$';` |
|        31 |  502 | `	n += BcryptB64Encode(&zOut[n],aSalt,16);          /* 22 chars → n = 29 */` |
|        31 |  503 | `	BcryptB64Encode(&zOut[n],zCipher,23);             /* 31 chars (drop the 24th byte) */` |
|        31 |  504 | `	return SXRET_OK;` |
|        16 |  505 | `}` |
|         - |  506 |  |
