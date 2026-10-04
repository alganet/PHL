<?php
/**
 * SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Cuts src/ph7/vm_dom_eucjp.h -- the HTML document's EUC-JP index -- from
 * php's own HTML decoder and encoder.
 *
 *   XDEBUG_MODE=off php build-aux/gen_domenc_eucjp.php > src/ph7/vm_dom_eucjp.h
 *
 * The oracle is `Dom\HTMLDocument` and nothing else: mbstring and iconv carry
 * EUC-JP tables of their own that answer differently, and the HTML document's
 * two DIRECTIONS do not agree either -- php DECODES the third code set behind
 * the 0x8F lead and cannot ENCODE one cell of it -- so each face is swept from
 * the door it serves and neither is inverted out of the other.
 *
 * EUC-JP is why gen_domenc_dbcs.php refuses it: a byte is a character, or
 * opens a PAIR, or -- 0x8F alone -- opens a THREE-byte sequence naming a cell
 * in a second table. Nothing about that framing is assumed here. Which bytes
 * open a pair is measured with (b,0xA1); which of those takes a third byte is
 * measured with (b,0xA1,0xA1); and both cell tables are swept per byte
 * combination rather than through the standard's pointer arithmetic, which an
 * encoding of this shape has already been caught not following.
 *
 * Both sweeps are comma-separated -- no trail byte that names a cell is 0x2C,
 * and a broken sequence re-reads only an ASCII byte, so the separator survives
 * every answer. ENCODE is swept through a COMMENT node, which saveHtml()
 * writes without escaping, so the bytes are the encoder's.
 */
const T0 = 0x40, T1 = 0xFE;             /* the trail window swept */
const NCOL = T1 - T0 + 1;
const LEAD0 = 0x81, LEAD1 = 0xFE;
const NROW = LEAD1 - LEAD0 + 1;
const LABEL = 'EUC-JP';

function cps(string $s): array {
	$o = [];
	for( $i = 0 ; $i < mb_strlen($s,'UTF-8') ; $i++ ){
		$o[] = mb_ord(mb_substr($s,$i,1,'UTF-8'),'UTF-8');
	}
	return $o;
}
function parts(string $src,int $want): array {
	$d = Dom\HTMLDocument::createFromString("<!doctype html><p>$src</p>",0,LABEL);
	$p = explode(',', $d->getElementsByTagName('p')[0]->textContent);
	if( count($p) - 1 !== $want ){
		fwrite(STDERR,LABEL.': '.(count($p)-1)." parts for $want cells\n");
		exit(1);
	}
	return $p;
}
/* --- which bytes open a pair, and what the others answer alone ----------- */
$src = ''; $bs = [];
for( $b = 0x80 ; $b <= 0xFF ; $b++ ){ $src .= chr($b).chr(0xA1).','; $bs[] = $b; }
$p = parts($src,count($bs));
$byte = [];                             /* 0xFFFF = a lead, 0 = U+FFFD */
foreach( $bs as $i => $b ){
	$c = cps($p[$i]);
	if( count($c) === 1 ){
		$byte[$b] = 0xFFFF;             /* the 0xA1 was eaten with it */
	}elseif( count($c) === 2 ){
		$byte[$b] = $c[0] === 0xFFFD ? 0 : $c[0];
		if( $byte[$b] > 0xFFFF ){ fwrite(STDERR,LABEL.": byte $b is outside the BMP\n"); exit(1); }
	}else{
		fwrite(STDERR,sprintf("%s: byte %02X answered %d characters\n",LABEL,$b,count($c)));
		exit(1);
	}
}
/* --- and which of those leads takes a THIRD byte ------------------------- */
$src = ''; $ls = [];
foreach( $byte as $b => $v ){
	if( $v !== 0xFFFF ) continue;
	$src .= chr($b).chr(0xA1).chr(0xA1).','; $ls[] = $b;
}
$p = parts($src,count($ls));
$wide = [];
foreach( $ls as $i => $b ){
	$n = count(cps($p[$i]));
	/* One character for three bytes is a three-byte lead; two is a pair plus
	 * the stray 0xA1 that follows it. */
	if( $n === 1 ){ $wide[] = $b; }
	elseif( $n !== 2 ){
		fwrite(STDERR,sprintf("%s: lead %02X answered %d characters for three bytes\n",LABEL,$b,$n));
		exit(1);
	}
}
if( count($wide) !== 1 ){
	fwrite(STDERR,LABEL.': '.count($wide)." three-byte leads, expected one\n");
	exit(1);
}
$W = $wide[0];
/* --- and which bytes the three-byte lead accepts in the MIDDLE ----------- */
/* Not every byte that can open a pair can sit behind the wide lead, and an
 * empty row is not the same answer as a refused one: a refused middle byte
 * ends the sequence after TWO bytes, leaving the third to open a character of
 * its own, where an accepted one eats it. The probe is (W,b,0xA1,0xA1) --
 * refused, the trailing pair survives and decodes; accepted, it is eaten as
 * the cell's third byte and a stray lead. */
$src = ''; $bs = [];
for( $b = 0x80 ; $b <= 0xFF ; $b++ ){ $src .= chr($W).chr($b).chr(0xA1).chr(0xA1).','; $bs[] = $b; }
$p = parts($src,count($bs));
$refused = cps($p[0]);                  /* any refused byte answers alike */
$row0 = 0; $row1 = 0;
foreach( $bs as $i => $b ){
	if( cps($p[$i]) === $refused ) continue;
	if( $row1 && $b !== $row1 + 1 ){
		fwrite(STDERR,sprintf("%s: the %02X row window is not one run\n",LABEL,$W));
		exit(1);
	}
	if( !$row0 ){ $row0 = $b; }
	$row1 = $b;
}
if( !$row0 ){ fwrite(STDERR,LABEL.": no middle byte is accepted\n"); exit(1); }
/* --- the pairs ----------------------------------------------------------- */
$cell = array_fill(0,NROW * NCOL,0);
for( $L = LEAD0 ; $L <= LEAD1 ; $L++ ){
	if( $byte[$L] !== 0xFFFF || $L === $W ) continue;
	$src = ''; $ts = [];
	for( $t = T0 ; $t <= T1 ; $t++ ){ $src .= chr($L).chr($t).','; $ts[] = $t; }
	$p = parts($src,count($ts));
	foreach( $ts as $i => $t ){
		$c = cps($p[$i]);
		if( count($c) === 0 || $c[0] === 0xFFFD ) continue;   /* no character here */
		if( count($c) > 1 || $c[0] > 0xFFFF ){
			fwrite(STDERR,sprintf("%s: cell %02X%02X is not one BMP code point\n",LABEL,$L,$t));
			exit(1);
		}
		$cell[($L - LEAD0) * NCOL + $t - T0] = $c[0];
	}
}
/* --- the third code set, behind the 0x8F lead ---------------------------- */
$aux = array_fill(0,NROW * NCOL,0);
for( $L = $row0 ; $L <= $row1 ; $L++ ){
	$src = ''; $ts = [];
	for( $t = T0 ; $t <= T1 ; $t++ ){ $src .= chr($W).chr($L).chr($t).','; $ts[] = $t; }
	$p = parts($src,count($ts));
	foreach( $ts as $i => $t ){
		$c = cps($p[$i]);
		if( count($c) === 0 || $c[0] === 0xFFFD ) continue;
		if( count($c) > 1 || $c[0] > 0xFFFF ){
			fwrite(STDERR,sprintf("%s: cell %02X%02X%02X is not one BMP code point\n",LABEL,$W,$L,$t));
			exit(1);
		}
		$aux[($L - LEAD0) * NCOL + $t - T0] = $c[0];
	}
}
/* --- the encoder, swept over the BMP ------------------------------------- */
$d = Dom\HTMLDocument::createEmpty(LABEL);
$cpl = []; $txt = '';
for( $cp = 0x80 ; $cp <= 0xFFFF ; $cp++ ){
	if( $cp >= 0xD800 && $cp <= 0xDFFF ) continue;
	$cpl[] = $cp; $txt .= mb_chr($cp,'UTF-8').',';
}
$p = explode(',', substr($d->saveHtml($d->createComment($txt)),4,-3));
if( count($p) - 1 !== count($cpl) ){ fwrite(STDERR,LABEL.": encode sweep lost cells\n"); exit(1); }
$eCp = []; $eB = [];
foreach( $cpl as $i => $cp ){
	$g = $p[$i];
	if( $g === '?' ) continue;                        /* php cannot spell it */
	if( strlen($g) === 1 ){ $eCp[] = $cp; $eB[] = ord($g); continue; }
	if( strlen($g) !== 2 ){
		/* A three-byte spelling would need a width this table does not have;
		 * php's EUC-JP encoder writes `?` for every 0x8F cell instead. */
		fwrite(STDERR,sprintf("%s: U+%04X encoded to %d bytes\n",LABEL,$cp,strlen($g)));
		exit(1);
	}
	$eCp[] = $cp; $eB[] = (ord($g[0]) << 8) | ord($g[1]);
}
fwrite(STDERR,sprintf("%s three-byte lead %02X rows %02X..%02X, %d pairs, %d %02X cells, %d encoded\n",
	LABEL,$W,$row0,$row1,count(array_filter($cell)),count(array_filter($aux)),$W,count($eCp)));
/* --- emit ---------------------------------------------------------------- */
$w = function(array $a){
	$o = ''; $a = array_values($a); $n = count($a);
	for( $i = 0 ; $i < $n ; $i++ ){
		$o .= ($i % 12 === 0 ? "\t" : '').sprintf('0x%04X',$a[$i]).($i + 1 === $n ? '' : ',');
		if( $i % 12 === 11 || $i + 1 === $n ) $o .= "\n";
	}
	return $o;
};
echo <<<'TXT'
/**
 * SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * The HTML document's EUC-JP index. GENERATED -- do not hand-edit; re-cut
 * with `XDEBUG_MODE=off php build-aux/gen_domenc_eucjp.php`.
 *
 * This is NOT mbstring's or iconv's EUC-JP, and its two faces are not each
 * other's: every array here is swept from `Dom\HTMLDocument` itself, one face
 * at a time, because php DECODES the third code set behind the 0x8F lead and
 * ENCODES not one cell of it.
 *
 *   aDomEucjpByte   what a byte 0x80..0xFF answers ALONE -- a code point, 0
 *                   where the decoder answers U+FFFD, and 0xFFFF where the
 *                   byte opens a longer sequence instead.
 *   aDomEucjpCell   the PAIR table, row (lead - 0x81), column (trail - 0x40).
 *   aDomEucjpAux    the three-byte table behind 0x8F, indexed the same way by
 *                   the SECOND and THIRD bytes. 0x8F's own row in aDomEucjpCell
 *                   is empty, so the two never collide. PH7_DOM_EUCJP_ROW0 and
 *                   _ROW1 are the middle bytes it ACCEPTS: outside them the
 *                   sequence ends after two bytes and the third opens a
 *                   character of its own, which an empty row does not do.
 *   aDomEucjpEncCp  the ENCODER, ordered by code point for a binary search,
 *   aDomEucjpEncB   with the byte (below 0x100) or the lead<<8|trail it
 *                   writes. No entry is three bytes wide: php answers `?` for
 *                   every character that only the 0x8F set spells.
 *
 * Both tables are indexed by the BYTES rather than by the standard's pointer
 * because php's framing is not always the standard's.
 */

TXT;
printf("#define PH7_DOM_EUCJP_ENC %d\n",count($eCp));
printf("#define PH7_DOM_EUCJP_WIDE 0x%02X\n",$W);
printf("#define PH7_DOM_EUCJP_ROW0 0x%02X\n",$row0);
printf("#define PH7_DOM_EUCJP_ROW1 0x%02X\n",$row1);
printf("static const sxu16 aDomEucjpByte[128] = {\n%s};\n",$w($byte));
printf("static const sxu16 aDomEucjpCell[%d * %d] = {\n%s};\n",NROW,NCOL,$w($cell));
printf("static const sxu16 aDomEucjpAux[%d * %d] = {\n%s};\n",NROW,NCOL,$w($aux));
printf("static const sxu16 aDomEucjpEncCp[PH7_DOM_EUCJP_ENC] = {\n%s};\n",$w($eCp));
printf("static const sxu16 aDomEucjpEncB[PH7_DOM_EUCJP_ENC] = {\n%s};\n",$w($eB));
