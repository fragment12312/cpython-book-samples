""" Python Character Mapping Codec cp1256 generated from 'WindowsBestFit/cp1256.txt' with gencodec.py.

"""#"

import codecs

### Codec APIs

class Codec(codecs.Codec):

    def encode(self, input, errors='strict'):
        return codecs.charmap_encode(input, errors, encoding_table)

    def decode(self, input, errors='strict'):
        return codecs.charmap_decode(input, errors, decoding_table)

class IncrementalEncoder(codecs.IncrementalEncoder):
    def encode(self, input, final=False):
        return codecs.charmap_encode(input, self.errors, encoding_table)[0]

class IncrementalDecoder(codecs.IncrementalDecoder):
    def decode(self, input, final=False):
        return codecs.charmap_decode(input, self.errors, decoding_table)[0]

class StreamWriter(Codec, codecs.StreamWriter):
    pass

class StreamReader(Codec, codecs.StreamReader):
    pass

### encodings module API

def getregentry():
    return codecs.CodecInfo(
        name='cp1256',
        encode=Codec().encode,
        decode=Codec().decode,
        incrementalencoder=IncrementalEncoder,
        incrementaldecoder=IncrementalDecoder,
        streamreader=StreamReader,
        streamwriter=StreamWriter,
    )


### Decoding Table

decoding_table = (
    '\x00'      #  0x00 -> NULL
    '\x01'      #  0x01 -> START OF HEADING
    '\x02'      #  0x02 -> START OF TEXT
    '\x03'      #  0x03 -> END OF TEXT
    '\x04'      #  0x04 -> END OF TRANSMISSION
    '\x05'      #  0x05 -> ENQUIRY
    '\x06'      #  0x06 -> ACKNOWLEDGE
    '\x07'      #  0x07 -> BELL
    '\x08'      #  0x08 -> BACKSPACE
    '\t'        #  0x09 -> HORIZONTAL TABULATION
    '\n'        #  0x0A -> LINE FEED
    '\x0b'      #  0x0B -> VERTICAL TABULATION
    '\x0c'      #  0x0C -> FORM FEED
    '\r'        #  0x0D -> CARRIAGE RETURN
    '\x0e'      #  0x0E -> SHIFT OUT
    '\x0f'      #  0x0F -> SHIFT IN
    '\x10'      #  0x10 -> DATA LINK ESCAPE
    '\x11'      #  0x11 -> DEVICE CONTROL ONE
    '\x12'      #  0x12 -> DEVICE CONTROL TWO
    '\x13'      #  0x13 -> DEVICE CONTROL THREE
    '\x14'      #  0x14 -> DEVICE CONTROL FOUR
    '\x15'      #  0x15 -> NEGATIVE ACKNOWLEDGE
    '\x16'      #  0x16 -> SYNCHRONOUS IDLE
    '\x17'      #  0x17 -> END OF TRANSMISSION BLOCK
    '\x18'      #  0x18 -> CANCEL
    '\x19'      #  0x19 -> END OF MEDIUM
    '\x1a'      #  0x1A -> SUBSTITUTE
    '\x1b'      #  0x1B -> ESCAPE
    '\x1c'      #  0x1C -> FILE SEPARATOR
    '\x1d'      #  0x1D -> GROUP SEPARATOR
    '\x1e'      #  0x1E -> RECORD SEPARATOR
    '\x1f'      #  0x1F -> UNIT SEPARATOR
    ' '         #  0x20 -> SPACE
    '!'         #  0x21 -> EXCLAMATION MARK
    '"'         #  0x22 -> QUOTATION MARK
    '#'         #  0x23 -> NUMBER SIGN
    '$'         #  0x24 -> DOLLAR SIGN
    '%'         #  0x25 -> PERCENT SIGN
    '&'         #  0x26 -> AMPERSAND
    "'"         #  0x27 -> APOSTROPHE-QUOTE
    '('         #  0x28 -> OPENING PARENTHESIS
    ')'         #  0x29 -> CLOSING PARENTHESIS
    '*'         #  0x2A -> ASTERISK
    '+'         #  0x2B -> PLUS SIGN
    ','         #  0x2C -> COMMA
    '-'         #  0x2D -> HYPHEN-MINUS
    '.'         #  0x2E -> PERIOD
    '/'         #  0x2F -> SLASH
    '0'         #  0x30 -> DIGIT ZERO
    '1'         #  0x31 -> DIGIT ONE
    '2'         #  0x32 -> DIGIT TWO
    '3'         #  0x33 -> DIGIT THREE
    '4'         #  0x34 -> DIGIT FOUR
    '5'         #  0x35 -> DIGIT FIVE
    '6'         #  0x36 -> DIGIT SIX
    '7'         #  0x37 -> DIGIT SEVEN
    '8'         #  0x38 -> DIGIT EIGHT
    '9'         #  0x39 -> DIGIT NINE
    ':'         #  0x3A -> COLON
    ';'         #  0x3B -> SEMICOLON
    '<'         #  0x3C -> LESS-THAN SIGN
    '='         #  0x3D -> EQUALS SIGN
    '>'         #  0x3E -> GREATER-THAN SIGN
    '?'         #  0x3F -> QUESTION MARK
    '@'         #  0x40 -> COMMERCIAL AT
    'A'         #  0x41 -> LATIN CAPITAL LETTER A
    'B'         #  0x42 -> LATIN CAPITAL LETTER B
    'C'         #  0x43 -> LATIN CAPITAL LETTER C
    'D'         #  0x44 -> LATIN CAPITAL LETTER D
    'E'         #  0x45 -> LATIN CAPITAL LETTER E
    'F'         #  0x46 -> LATIN CAPITAL LETTER F
    'G'         #  0x47 -> LATIN CAPITAL LETTER G
    'H'         #  0x48 -> LATIN CAPITAL LETTER H
    'I'         #  0x49 -> LATIN CAPITAL LETTER I
    'J'         #  0x4A -> LATIN CAPITAL LETTER J
    'K'         #  0x4B -> LATIN CAPITAL LETTER K
    'L'         #  0x4C -> LATIN CAPITAL LETTER L
    'M'         #  0x4D -> LATIN CAPITAL LETTER M
    'N'         #  0x4E -> LATIN CAPITAL LETTER N
    'O'         #  0x4F -> LATIN CAPITAL LETTER O
    'P'         #  0x50 -> LATIN CAPITAL LETTER P
    'Q'         #  0x51 -> LATIN CAPITAL LETTER Q
    'R'         #  0x52 -> LATIN CAPITAL LETTER R
    'S'         #  0x53 -> LATIN CAPITAL LETTER S
    'T'         #  0x54 -> LATIN CAPITAL LETTER T
    'U'         #  0x55 -> LATIN CAPITAL LETTER U
    'V'         #  0x56 -> LATIN CAPITAL LETTER V
    'W'         #  0x57 -> LATIN CAPITAL LETTER W
    'X'         #  0x58 -> LATIN CAPITAL LETTER X
    'Y'         #  0x59 -> LATIN CAPITAL LETTER Y
    'Z'         #  0x5A -> LATIN CAPITAL LETTER Z
    '['         #  0x5B -> OPENING SQUARE BRACKET
    '\\'        #  0x5C -> BACKSLASH
    ']'         #  0x5D -> CLOSING SQUARE BRACKET
    '^'         #  0x5E -> SPACING CIRCUMFLEX
    '_'         #  0x5F -> SPACING UNDERSCORE
    '`'         #  0x60 -> SPACING GRAVE
    'a'         #  0x61 -> LATIN SMALL LETTER A
    'b'         #  0x62 -> LATIN SMALL LETTER B
    'c'         #  0x63 -> LATIN SMALL LETTER C
    'd'         #  0x64 -> LATIN SMALL LETTER D
    'e'         #  0x65 -> LATIN SMALL LETTER E
    'f'         #  0x66 -> LATIN SMALL LETTER F
    'g'         #  0x67 -> LATIN SMALL LETTER G
    'h'         #  0x68 -> LATIN SMALL LETTER H
    'i'         #  0x69 -> LATIN SMALL LETTER I
    'j'         #  0x6A -> LATIN SMALL LETTER J
    'k'         #  0x6B -> LATIN SMALL LETTER K
    'l'         #  0x6C -> LATIN SMALL LETTER L
    'm'         #  0x6D -> LATIN SMALL LETTER M
    'n'         #  0x6E -> LATIN SMALL LETTER N
    'o'         #  0x6F -> LATIN SMALL LETTER O
    'p'         #  0x70 -> LATIN SMALL LETTER P
    'q'         #  0x71 -> LATIN SMALL LETTER Q
    'r'         #  0x72 -> LATIN SMALL LETTER R
    's'         #  0x73 -> LATIN SMALL LETTER S
    't'         #  0x74 -> LATIN SMALL LETTER T
    'u'         #  0x75 -> LATIN SMALL LETTER U
    'v'         #  0x76 -> LATIN SMALL LETTER V
    'w'         #  0x77 -> LATIN SMALL LETTER W
    'x'         #  0x78 -> LATIN SMALL LETTER X
    'y'         #  0x79 -> LATIN SMALL LETTER Y
    'z'         #  0x7A -> LATIN SMALL LETTER Z
    '{'         #  0x7B -> OPENING CURLY BRACKET
    '|'         #  0x7C -> VERTICAL BAR
    '}'         #  0x7D -> CLOSING CURLY BRACKET
    '~'         #  0x7E -> TILDE
    '\x7f'      #  0x7F -> DELETE
    '\u20ac'    #  0x80 -> EURO SIGN
    '\u067e'    #  0x81 -> ARABIC TAA WITH THREE DOTS BELOW
    '\u201a'    #  0x82 -> LOW SINGLE COMMA QUOTATION MARK
    '\u0192'    #  0x83 -> LATIN SMALL LETTER SCRIPT F
    '\u201e'    #  0x84 -> LOW DOUBLE COMMA QUOTATION MARK
    '\u2026'    #  0x85 -> HORIZONTAL ELLIPSIS
    '\u2020'    #  0x86 -> DAGGER
    '\u2021'    #  0x87 -> DOUBLE DAGGER
    '\u02c6'    #  0x88 -> MODIFIER LETTER CIRCUMFLEX
    '\u2030'    #  0x89 -> PER MILLE SIGN
    '\u0679'    #  0x8A -> ARABIC LETTER TTEH
    '\u2039'    #  0x8B -> LEFT POINTING SINGLE GUILLEMET
    '\u0152'    #  0x8C -> LATIN CAPITAL LETTER O E
    '\u0686'    #  0x8D -> ARABIC HAA WITH MIDDLE THREE DOTS DOWNWARD
    '\u0698'    #  0x8E -> ARABIC RA WITH THREE DOTS ABOVE
    '\u0688'    #  0x8F -> ARABIC LETTER DDAL
    '\u06af'    #  0x90 -> ARABIC GAF
    '\u2018'    #  0x91 -> SINGLE TURNED COMMA QUOTATION MARK
    '\u2019'    #  0x92 -> SINGLE COMMA QUOTATION MARK
    '\u201c'    #  0x93 -> DOUBLE TURNED COMMA QUOTATION MARK
    '\u201d'    #  0x94 -> DOUBLE COMMA QUOTATION MARK
    '\u2022'    #  0x95 -> BULLET
    '\u2013'    #  0x96 -> EN DASH
    '\u2014'    #  0x97 -> EM DASH
    '\u06a9'    #  0x98 -> ARABIC LETTER KEHEH
    '\u2122'    #  0x99 -> TRADEMARK
    '\u0691'    #  0x9A -> ARABIC LETTER RREH
    '\u203a'    #  0x9B -> RIGHT POINTING SINGLE GUILLEMET
    '\u0153'    #  0x9C -> LATIN SMALL LETTER O E
    '\u200c'    #  0x9D -> ZERO WIDTH NON-JOINER
    '\u200d'    #  0x9E -> ZERO WIDTH JOINER
    '\u06ba'    #  0x9F -> ARABIC LETTER NOON GHUNNA
    '\xa0'      #  0xA0 -> NON-BREAKING SPACE
    '\u060c'    #  0xA1 -> ARABIC COMMA
    '\xa2'      #  0xA2 -> CENT SIGN
    '\xa3'      #  0xA3 -> POUND SIGN
    '\xa4'      #  0xA4 -> CURRENCY SIGN
    '\xa5'      #  0xA5 -> YEN SIGN
    '\xa6'      #  0xA6 -> BROKEN VERTICAL BAR
    '\xa7'      #  0xA7 -> SECTION SIGN
    '\xa8'      #  0xA8 -> SPACING DIAERESIS
    '\xa9'      #  0xA9 -> COPYRIGHT SIGN
    '\u06be'    #  0xAA -> ARABIC LETTER HEH DOACHASHMEE
    '\xab'      #  0xAB -> LEFT POINTING GUILLEMET
    '\xac'      #  0xAC -> NOT SIGN
    '\xad'      #  0xAD -> SOFT HYPHEN
    '\xae'      #  0xAE -> REGISTERED TRADE MARK SIGN
    '\xaf'      #  0xAF -> SPACING MACRON
    '\xb0'      #  0xB0 -> DEGREE SIGN
    '\xb1'      #  0xB1 -> PLUS-OR-MINUS SIGN
    '\xb2'      #  0xB2 -> SUPERSCRIPT DIGIT TWO
    '\xb3'      #  0xB3 -> SUPERSCRIPT DIGIT THREE
    '\xb4'      #  0xB4 -> SPACING ACUTE
    '\xb5'      #  0xB5 -> MICRO SIGN
    '\xb6'      #  0xB6 -> PARAGRAPH SIGN
    '\xb7'      #  0xB7 -> MIDDLE DOT
    '\xb8'      #  0xB8 -> SPACING CEDILLA
    '\xb9'      #  0xB9 -> SUPERSCRIPT DIGIT ONE
    '\u061b'    #  0xBA -> ARABIC SEMICOLON
    '\xbb'      #  0xBB -> RIGHT POINTING GUILLEMET
    '\xbc'      #  0xBC -> FRACTION ONE QUARTER
    '\xbd'      #  0xBD -> FRACTION ONE HALF
    '\xbe'      #  0xBE -> FRACTION THREE QUARTERS
    '\u061f'    #  0xBF -> ARABIC QUESTION MARK
    '\u06c1'    #  0xC0 -> ARABIC LETTER HEH GOAL
    '\u0621'    #  0xC1 -> ARABIC LETTER HAMZAH
    '\u0622'    #  0xC2 -> ARABIC LETTER MADDAH ON ALEF
    '\u0623'    #  0xC3 -> ARABIC LETTER HAMZAH ON ALEF
    '\u0624'    #  0xC4 -> ARABIC LETTER HAMZAH ON WAW
    '\u0625'    #  0xC5 -> ARABIC LETTER HAMZAH UNDER ALEF
    '\u0626'    #  0xC6 -> ARABIC LETTER HAMZAH ON YA
    '\u0627'    #  0xC7 -> ARABIC LETTER ALEF
    '\u0628'    #  0xC8 -> ARABIC LETTER BAA
    '\u0629'    #  0xC9 -> ARABIC LETTER TAA MARBUTAH
    '\u062a'    #  0xCA -> ARABIC LETTER TAA
    '\u062b'    #  0xCB -> ARABIC LETTER THAA
    '\u062c'    #  0xCC -> ARABIC LETTER JEEM
    '\u062d'    #  0xCD -> ARABIC LETTER HAA
    '\u062e'    #  0xCE -> ARABIC LETTER KHAA
    '\u062f'    #  0xCF -> ARABIC LETTER DAL
    '\u0630'    #  0xD0 -> ARABIC LETTER THAL
    '\u0631'    #  0xD1 -> ARABIC LETTER RA
    '\u0632'    #  0xD2 -> ARABIC LETTER ZAIN
    '\u0633'    #  0xD3 -> ARABIC LETTER SEEN
    '\u0634'    #  0xD4 -> ARABIC LETTER SHEEN
    '\u0635'    #  0xD5 -> ARABIC LETTER SAD
    '\u0636'    #  0xD6 -> ARABIC LETTER DAD
    '\xd7'      #  0xD7 -> MULTIPLICATION SIGN
    '\u0637'    #  0xD8 -> ARABIC LETTER TAH
    '\u0638'    #  0xD9 -> ARABIC LETTER DHAH
    '\u0639'    #  0xDA -> ARABIC LETTER AIN
    '\u063a'    #  0xDB -> ARABIC LETTER GHAIN
    '\u0640'    #  0xDC -> ARABIC TATWEEL
    '\u0641'    #  0xDD -> ARABIC LETTER FA
    '\u0642'    #  0xDE -> ARABIC LETTER QAF
    '\u0643'    #  0xDF -> ARABIC LETTER CAF
    '\xe0'      #  0xE0 -> LATIN SMALL LETTER A GRAVE
    '\u0644'    #  0xE1 -> ARABIC LETTER LAM
    '\xe2'      #  0xE2 -> LATIN SMALL LETTER A CIRCUMFLEX
    '\u0645'    #  0xE3 -> ARABIC LETTER MEEM
    '\u0646'    #  0xE4 -> ARABIC LETTER NOON
    '\u0647'    #  0xE5 -> ARABIC LETTER HA
    '\u0648'    #  0xE6 -> ARABIC LETTER WAW
    '\xe7'      #  0xE7 -> LATIN SMALL LETTER C CEDILLA
    '\xe8'      #  0xE8 -> LATIN SMALL LETTER E GRAVE
    '\xe9'      #  0xE9 -> LATIN SMALL LETTER E ACUTE
    '\xea'      #  0xEA -> LATIN SMALL LETTER E CIRCUMFLEX
    '\xeb'      #  0xEB -> LATIN SMALL LETTER E DIAERESIS
    '\u0649'    #  0xEC -> ARABIC LETTER ALEF MAQSURAH
    '\u064a'    #  0xED -> ARABIC LETTER YA
    '\xee'      #  0xEE -> LATIN SMALL LETTER I CIRCUMFLEX
    '\xef'      #  0xEF -> LATIN SMALL LETTER I DIAERESIS
    '\u064b'    #  0xF0 -> ARABIC FATHATAN
    '\u064c'    #  0xF1 -> ARABIC DAMMATAN
    '\u064d'    #  0xF2 -> ARABIC KASRATAN
    '\u064e'    #  0xF3 -> ARABIC FATHAH
    '\xf4'      #  0xF4 -> LATIN SMALL LETTER O CIRCUMFLEX
    '\u064f'    #  0xF5 -> ARABIC DAMMAH
    '\u0650'    #  0xF6 -> ARABIC KASRAH
    '\xf7'      #  0xF7 -> DIVISION SIGN
    '\u0651'    #  0xF8 -> ARABIC SHADDAH
    '\xf9'      #  0xF9 -> LATIN SMALL LETTER U GRAVE
    '\u0652'    #  0xFA -> ARABIC SUKUN
    '\xfb'      #  0xFB -> LATIN SMALL LETTER U CIRCUMFLEX
    '\xfc'      #  0xFC -> LATIN SMALL LETTER U DIAERESIS
    '\u200e'    #  0xFD -> LEFT-TO-RIGHT MARK
    '\u200f'    #  0xFE -> RIGHT-TO-LEFT MARK
    '\u06d2'    #  0xFF -> ARABIC LETTER YEH BARREE
)

### Encoding table
encoding_table = codecs.charmap_build(decoding_table)

