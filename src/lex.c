#include "c.h"
#include <float.h>
#include <errno.h>

static char rcsid[] = "$Id$";

#define MAXTOKEN 32  // 除标识符、字符串、数字常量外，其他所有单词的长度不超过 32 个字节

enum { BLANK=01,  NEWLINE=02, LETTER=04,
       DIGIT=010, HEX=020,    OTHER=040 };

/**
 * 判断读入的 ascii 对应的十进制数作为 map 数组的下表
 */
static unsigned char map[256] = { /* 000 nul */	0,
				   /* 001 soh */	0,
				   /* 002 stx */	0,
				   /* 003 etx */	0,
				   /* 004 eot */	0,
				   /* 005 enq */	0,
				   /* 006 ack */	0,
				   /* 007 bel */	0,
				   /* 010 bs  */	0,
				   /* 011 ht  */	BLANK,
				   /* 012 nl  */	NEWLINE,
				   /* 013 vt  */	BLANK,
				   /* 014 ff  */	BLANK,
				   /* 015 cr  */	0,
				   /* 016 so  */	0,
				   /* 017 si  */	0,
				   /* 020 dle */	0,
				   /* 021 dc1 */	0,
				   /* 022 dc2 */	0,
				   /* 023 dc3 */	0,
				   /* 024 dc4 */	0,
				   /* 025 nak */	0,
				   /* 026 syn */	0,
				   /* 027 etb */	0,
				   /* 030 can */	0,
				   /* 031 em  */	0,
				   /* 032 sub */	0,
				   /* 033 esc */	0,
				   /* 034 fs  */	0,
				   /* 035 gs  */	0,
				   /* 036 rs  */	0,
				   /* 037 us  */	0,
				   /* 040 sp  */	BLANK,
				   /* 041 !   */	OTHER,
				   /* 042 "   */	OTHER,
				   /* 043 #   */	OTHER,
				   /* 044 $   */	0,
				   /* 045 %   */	OTHER,
				   /* 046 &   */	OTHER,
				   /* 047 '   */	OTHER,
				   /* 050 (   */	OTHER,
				   /* 051 )   */	OTHER,
				   /* 052 *   */	OTHER,
				   /* 053 +   */	OTHER,
				   /* 054 ,   */	OTHER,
				   /* 055 -   */	OTHER,
				   /* 056 .   */	OTHER,
				   /* 057 /   */	OTHER,
				   /* 060 0   */	DIGIT,
				   /* 061 1   */	DIGIT,
				   /* 062 2   */	DIGIT,
				   /* 063 3   */	DIGIT,
				   /* 064 4   */	DIGIT,
				   /* 065 5   */	DIGIT,
				   /* 066 6   */	DIGIT,
				   /* 067 7   */	DIGIT,
				   /* 070 8   */	DIGIT,
				   /* 071 9   */	DIGIT,
				   /* 072 :   */	OTHER,
				   /* 073 ;   */	OTHER,
				   /* 074 <   */	OTHER,
				   /* 075 =   */	OTHER,
				   /* 076 >   */	OTHER,
				   /* 077 ?   */	OTHER,
				   /* 100 @   */	0,
				   /* 101 A   */	LETTER|HEX,
				   /* 102 B   */	LETTER|HEX,
				   /* 103 C   */	LETTER|HEX,
				   /* 104 D   */	LETTER|HEX,
				   /* 105 E   */	LETTER|HEX,
				   /* 106 F   */	LETTER|HEX,
				   /* 107 G   */	LETTER,
				   /* 110 H   */	LETTER,
				   /* 111 I   */	LETTER,
				   /* 112 J   */	LETTER,
				   /* 113 K   */	LETTER,
				   /* 114 L   */	LETTER,
				   /* 115 M   */	LETTER,
				   /* 116 N   */	LETTER,
				   /* 117 O   */	LETTER,
				   /* 120 P   */	LETTER,
				   /* 121 Q   */	LETTER,
				   /* 122 R   */	LETTER,
				   /* 123 S   */	LETTER,
				   /* 124 T   */	LETTER,
				   /* 125 U   */	LETTER,
				   /* 126 V   */	LETTER,
				   /* 127 W   */	LETTER,
				   /* 130 X   */	LETTER,
				   /* 131 Y   */	LETTER,
				   /* 132 Z   */	LETTER,
				   /* 133 [   */	OTHER,
				   /* 134 \   */	OTHER,
				   /* 135 ]   */	OTHER,
				   /* 136 ^   */	OTHER,
				   /* 137 _   */	LETTER,
				   /* 140 `   */	0,
				   /* 141 a   */	LETTER|HEX,
				   /* 142 b   */	LETTER|HEX,
				   /* 143 c   */	LETTER|HEX,
				   /* 144 d   */	LETTER|HEX,
				   /* 145 e   */	LETTER|HEX,
				   /* 146 f   */	LETTER|HEX,
				   /* 147 g   */	LETTER,
				   /* 150 h   */	LETTER,
				   /* 151 i   */	LETTER,
				   /* 152 j   */	LETTER,
				   /* 153 k   */	LETTER,
				   /* 154 l   */	LETTER,
				   /* 155 m   */	LETTER,
				   /* 156 n   */	LETTER,
				   /* 157 o   */	LETTER,
				   /* 160 p   */	LETTER,
				   /* 161 q   */	LETTER,
				   /* 162 r   */	LETTER,
				   /* 163 s   */	LETTER,
				   /* 164 t   */	LETTER,
				   /* 165 u   */	LETTER,
				   /* 166 v   */	LETTER,
				   /* 167 w   */	LETTER,
				   /* 170 x   */	LETTER,
				   /* 171 y   */	LETTER,
				   /* 172 z   */	LETTER,
				   /* 173 {   */	OTHER,
				   /* 174 |   */	OTHER,
				   /* 175 }   */	OTHER,
				   /* 176 ~   */	OTHER, };
static struct symbol tval;  // 为 gettok 提供常量类型
static char cbuf[BUFSIZE+1];
static unsigned int wcbuf[BUFSIZE+1];

Coordinate src;		/* current source coordinate */
int t;
char *token;		/* current token */
Symbol tsym;		/* symbol table entry for current token */

static void *cput(int c, void *cl);
static void *wcput(int c, void *cl);
static void *scon(int q, void *put(int c, void *cl), void *cl);
static int backslash(int q);
static Symbol fcon(void);
static Symbol icon(unsigned long, int, int);
static void ppnumber(char *);

int gettok(void) {
	for (;;) {
    /*
     * cp 属于全局变量，会放在全局变量区中，
     * rcp 属于局部变量，会存放在寄存器中，减少一些读取内存的时间，增加效率
     */
		register unsigned char *rcp = cp;
		while (map[*rcp]&BLANK) // 忽略空白字符
			rcp++;

    // 验证缓冲区至少包含可用的单词，如果缓冲区少于 32 个字节，那么就需要读入数据
		if (limit - rcp < MAXTOKEN) {
			cp = rcp;
			fillbuf();
			rcp = cp;
		}
    // 记录当前单词的坐标
		src.file = file;
		src.x = (char *)rcp - line;
		src.y = lineno;
		cp = rcp + 1;
		switch (*rcp++) {
		case '/':
        // 消除 /* */ 类型的注释
        if (*rcp == '*') {
			  	int c = 0;  // c 用于记录当前 rcp 的前一个字符
			  	for (rcp++; *rcp != '/' || c != '*'; )
			  		if (map[*rcp]&NEWLINE) {   // 读到换行符
			  			if (rcp < limit)  // 缓冲区不为空
			  				c = *rcp;
			  			cp = rcp + 1;
			  			nextline();   // 填充缓冲区
			  			rcp = cp;
			  			if (rcp == limit)  // 缓冲区空
			  				break;
			  		} else
			  			c = *rcp++;
			  	if (rcp < limit)
			  		rcp++;
			  	else
			  		error("unclosed comment\n");
			  	cp = rcp;
			  	continue;
			  }
			  return '/';
		case '<':
			if (*rcp == '=') return cp++, LEQ;
			if (*rcp == '<') return cp++, LSHIFT;
			return '<';
		case '>':
			if (*rcp == '=') return cp++, GEQ;
			if (*rcp == '>') return cp++, RSHIFT;
			return '>';
		case '-':
			if (*rcp == '>') return cp++, DEREF;
			if (*rcp == '-') return cp++, DECR;
			return '-';
		case '=': return *rcp == '=' ? cp++, EQL    : '=';
		case '!': return *rcp == '=' ? cp++, NEQ    : '!';
		case '|': return *rcp == '|' ? cp++, OROR   : '|';
		case '&': return *rcp == '&' ? cp++, ANDAND : '&';
		case '+': return *rcp == '+' ? cp++, INCR   : '+';
		case ';': case ',': case ':':
		case '*': case '~': case '%': case '^': case '?':
		case '[': case ']': case '{': case '}': case '(': case ')':
			return rcp[-1];  // 返回当前读入的字符
		case '\n': case '\v': case '\r': case '\f':
			nextline();
			if (cp == limit) {
				tsym = NULL;
				return EOI;
			}
			continue;

    /**
     * 开始识别关键字和变量
     * 首先是识别 if 关键子和 int 变量
     */
		case 'i':
			if (rcp[0] == 'f'
			&& !(map[rcp[1]]&(DIGIT|LETTER))) {
				cp = rcp + 1;
				return IF;
			}
			if (rcp[0] == 'n'
			&&  rcp[1] == 't'
			&& !(map[rcp[2]]&(DIGIT|LETTER))) {
				cp = rcp + 2;
				tsym = inttype->u.sym; // 当前单词的 Symbol，是一个常量
				return INT;
			}
			goto id;  // 如果不满足上面的规则，那么表示就是一个变量
		case 'h': case 'j': case 'k': case 'm': case 'n': case 'o':
		case 'p': case 'q': case 'x': case 'y': case 'z':
		case 'A': case 'B': case 'C': case 'D': case 'E': case 'F':
		case 'G': case 'H': case 'I': case 'J': case 'K':
		case 'M': case 'N': case 'O': case 'P': case 'Q': case 'R':
		case 'S': case 'T': case 'U': case 'V': case 'W': case 'X':
		case 'Y': case 'Z':
		id:
			if (limit - rcp < MAXLINE) { //填充缓冲区
				cp = rcp - 1;   // 已经读入了一个字符，所以需要将 这个字符回退，然后再填充缓冲区
				fillbuf();
				rcp = ++cp;  // 因为已经读入了一个字符，才会走到这里，所以把读入的那个字符加上去
			}
			assert(cp == rcp);
			token = (char *)rcp - 1; // 记录当前变量字符在缓冲区的首地址
			while (map[*rcp]&(DIGIT|LETTER))
				rcp++;
			token = stringn(token, (char *)rcp - token);  //将读入的变量的字符串保存到
			tsym = lookup(token, identifiers);  // 加入到符号表中
			cp = rcp;
			return ID;

    /**
     * 数字的识别：float-constant, interge-constant, enumeration-constant(ID), character-constant
     * interge-constant(十六进制，二进制，十进制数值常量)
     */
		case '0': case '1': case '2': case '3': case '4':
		case '5': case '6': case '7': case '8': case '9': {
			unsigned long n = 0;
			if (limit - rcp < MAXLINE) {
				cp = rcp - 1;
				fillbuf();
				rcp = ++cp;
			}
			assert(cp == rcp);  // 这里的 cp 非常重要，cp 被指向开始的那个字符
			token = (char *)rcp - 1;  // 记录当前变量字符在缓冲区的首地址
      // 读取十六进制数
			if (*token == '0' && (*rcp == 'x' || *rcp == 'X')) {
				int d, overflow = 0;
				while (*++rcp) {
					if (map[*rcp]&DIGIT)
						d = *rcp - '0';
					else if (*rcp >= 'a' && *rcp <= 'f')
						d = *rcp - 'a' + 10;
					else if (*rcp >= 'A' && *rcp <= 'F')
						d = *rcp - 'A' + 10;
					else
						break;
					if (n&~(~0UL >> 4)) // 判断是否溢出
						overflow = 1;
					else
						n = (n<<4) + d;
				}
				if ((char *)rcp - token <= 2)
					error("invalid hexadecimal constant `%S'\n", token, (char *)rcp-token);
				cp = rcp;
				tsym = icon(n, overflow, 16);
			} else if (*token == '0') {  // 读取 8 进制数
				int err = 0, overflow = 0;
				for ( ; map[*rcp]&DIGIT; rcp++) {
					if (*rcp == '8' || *rcp == '9') // 8 和 9 在八进制不应该出现
						err = 1;
					if (n&~(~0UL >> 3))
						overflow = 1;
					else
						n = (n<<3) + (*rcp - '0');
				}
				if (*rcp == '.' || *rcp == 'e' || *rcp == 'E') {
					cp = rcp;
					tsym = fcon();  // float 类型，fcon() 处理 . e E 的后缀
					return FCON;
				}
				cp = rcp;
				tsym = icon(n, overflow, 8);
				if (err)
					error("invalid octal constant `%S'\n", token, (char*)cp-token);
			} else {  // 读取十进制
				int overflow = 0;
				for (n = *token - '0'; map[*rcp]&DIGIT; ) {
					int d = *rcp++ - '0';
					if (n > (ULONG_MAX - d)/10)  // ULONG_MAX 表示最大的无符号数，这里用作判断数字是否溢出
						overflow = 1;
					else
						n = 10*n + d;
				}
        // 处理小数后缀
				if (*rcp == '.' || *rcp == 'e' || *rcp == 'E') {
					cp = rcp;
					tsym = fcon();
					return FCON;
				}
				cp = rcp;
				tsym = icon(n, overflow, 10);
			}
			return ICON;
		}
		case '.':
			if (rcp[0] == '.' && rcp[1] == '.') {
				cp += 2;
				return ELLIPSIS;  // 省略，用于可变参数
			}
			if ((map[*rcp]&DIGIT) == 0) // 不是数字
				return '.';
      // 开始处理是数字情况，类似 .12345, . 之前的 0 被省略了
			if (limit - rcp < MAXLINE) {
				cp = rcp - 1;
				fillbuf();
				rcp = ++cp;
			}
			assert(cp == rcp);
			cp = rcp - 1;
			token = (char *)cp;
			tsym = fcon();
			return FCON;

    /* 开始处理字符串 */
    // 先处理宽字符常量(example: L'杭')和宽字符串常量(example: L"杭州")
    // ps: lcc 将宽字符作为普通的 ascii 实现
		case 'L':
			if (*rcp == '\'') {
				unsigned int *s = scon(*cp, wcput, wcbuf);
				if (s - wcbuf > 2)
					warning("excess characters in wide-character literal ignored\n");
				tval.type = widechar;
				tval.u.c.v.u = wcbuf[0];
				tsym = &tval;
				return ICON;
      // 处理宽字符串常量
			} else if (*rcp == '"') {
				unsigned int *s = scon(*cp, wcput, wcbuf);
				tval.type = array(widechar, s - wcbuf, 0);
				tval.u.c.v.p = wcbuf;
				tsym = &tval;
				return SCON;
			} else
				goto id;
    // 处理字符
		case '\'': {
			char *s = scon(*--cp, cput, cbuf);
			if (s - cbuf > 2)
				warning("excess characters in multibyte character literal ignored\n");
			tval.type = inttype;
			if (chartype->op == INT)
				tval.u.c.v.i = extend(cbuf[0], chartype);
			else
				tval.u.c.v.i = cbuf[0]&0xFF;
			tsym = &tval;
			return ICON;
			}
		case '"': {
			char *s = scon(*--cp, cput, cbuf);
			tval.type = array(chartype, s - cbuf, 0);
			tval.u.c.v.p = cbuf;
			tsym = &tval;
			return SCON;
			}

    // auto 关键字的识别
		case 'a':
			if (rcp[0] == 'u'
			&&  rcp[1] == 't'
			&&  rcp[2] == 'o'
			&& !(map[rcp[3]]&(DIGIT|LETTER))) {
				cp = rcp + 3;
				return AUTO;
			}
			goto id;

    // break 关键字的识别
		case 'b':
			if (rcp[0] == 'r'
			&&  rcp[1] == 'e'
			&&  rcp[2] == 'a'
			&&  rcp[3] == 'k'
			&& !(map[rcp[4]]&(DIGIT|LETTER))) {
				cp = rcp + 4;
				return BREAK;
			}
			goto id;
    // case、cahr、const、continue 等关键字的识别
		case 'c':
			if (rcp[0] == 'a'
			&&  rcp[1] == 's'
			&&  rcp[2] == 'e'
			&& !(map[rcp[3]]&(DIGIT|LETTER))) {
				cp = rcp + 3;
				return CASE;
			}
			if (rcp[0] == 'h'
			&&  rcp[1] == 'a'
			&&  rcp[2] == 'r'
			&& !(map[rcp[3]]&(DIGIT|LETTER))) {
				cp = rcp + 3;
				tsym = chartype->u.sym;
				return CHAR;
			}
			if (rcp[0] == 'o'
			&&  rcp[1] == 'n'
			&&  rcp[2] == 's'
			&&  rcp[3] == 't'
			&& !(map[rcp[4]]&(DIGIT|LETTER))) {
				cp = rcp + 4;
				return CONST;
			}
			if (rcp[0] == 'o'
			&&  rcp[1] == 'n'
			&&  rcp[2] == 't'
			&&  rcp[3] == 'i'
			&&  rcp[4] == 'n'
			&&  rcp[5] == 'u'
			&&  rcp[6] == 'e'
			&& !(map[rcp[7]]&(DIGIT|LETTER))) {
				cp = rcp + 7;
				return CONTINUE;
			}
			goto id;
    // default,double,do 等关键字的识别
		case 'd':
			if (rcp[0] == 'e'
			&&  rcp[1] == 'f'
			&&  rcp[2] == 'a'
			&&  rcp[3] == 'u'
			&&  rcp[4] == 'l'
			&&  rcp[5] == 't'
			&& !(map[rcp[6]]&(DIGIT|LETTER))) {
				cp = rcp + 6;
				return DEFAULT;
			}
			if (rcp[0] == 'o'
			&&  rcp[1] == 'u'
			&&  rcp[2] == 'b'
			&&  rcp[3] == 'l'
			&&  rcp[4] == 'e'
			&& !(map[rcp[5]]&(DIGIT|LETTER))) {
				cp = rcp + 5;
				tsym = doubletype->u.sym;
				return DOUBLE;
			}
			if (rcp[0] == 'o'
			&& !(map[rcp[1]]&(DIGIT|LETTER))) {
				cp = rcp + 1;
				return DO;
			}
			goto id;

    // else, enum, extern 等关键字的识别
		case 'e':
			if (rcp[0] == 'l'
			&&  rcp[1] == 's'
			&&  rcp[2] == 'e'
			&& !(map[rcp[3]]&(DIGIT|LETTER))) {
				cp = rcp + 3;
				return ELSE;
			}
			if (rcp[0] == 'n'
			&&  rcp[1] == 'u'
			&&  rcp[2] == 'm'
			&& !(map[rcp[3]]&(DIGIT|LETTER))) {
				cp = rcp + 3;
				return ENUM;
			}
			if (rcp[0] == 'x'
			&&  rcp[1] == 't'
			&&  rcp[2] == 'e'
			&&  rcp[3] == 'r'
			&&  rcp[4] == 'n'
			&& !(map[rcp[5]]&(DIGIT|LETTER))) {
				cp = rcp + 5;
				return EXTERN;
			}
			goto id;
    // float, for 等关键字识别
		case 'f':
			if (rcp[0] == 'l'
			&&  rcp[1] == 'o'
			&&  rcp[2] == 'a'
			&&  rcp[3] == 't'
			&& !(map[rcp[4]]&(DIGIT|LETTER))) {
				cp = rcp + 4;
				tsym = floattype->u.sym;
				return FLOAT;
			}
			if (rcp[0] == 'o'
			&&  rcp[1] == 'r'
			&& !(map[rcp[2]]&(DIGIT|LETTER))) {
				cp = rcp + 2;
				return FOR;
			}
			goto id;
    // goto 等关键字的识别
		case 'g':
			if (rcp[0] == 'o'
			&&  rcp[1] == 't'
			&&  rcp[2] == 'o'
			&& !(map[rcp[3]]&(DIGIT|LETTER))) {
				cp = rcp + 3;
				return GOTO;
			}
			goto id;
    // long 等关键字的识别
		case 'l':
			if (rcp[0] == 'o'
			&&  rcp[1] == 'n'
			&&  rcp[2] == 'g'
			&& !(map[rcp[3]]&(DIGIT|LETTER))) {
				cp = rcp + 3;
				return LONG;
			}
			goto id;
    // register, return 等关键字的识别
		case 'r':
			if (rcp[0] == 'e'
			&&  rcp[1] == 'g'
			&&  rcp[2] == 'i'
			&&  rcp[3] == 's'
			&&  rcp[4] == 't'
			&&  rcp[5] == 'e'
			&&  rcp[6] == 'r'
			&& !(map[rcp[7]]&(DIGIT|LETTER))) {
				cp = rcp + 7;
				return REGISTER;
			}
			if (rcp[0] == 'e'
			&&  rcp[1] == 't'
			&&  rcp[2] == 'u'
			&&  rcp[3] == 'r'
			&&  rcp[4] == 'n'
			&& !(map[rcp[5]]&(DIGIT|LETTER))) {
				cp = rcp + 5;
				return RETURN;
			}
			goto id;
    //short, signed, sizeof, static, struct, switch 等关键字的识别
		case 's':
			if (rcp[0] == 'h'
			&&  rcp[1] == 'o'
			&&  rcp[2] == 'r'
			&&  rcp[3] == 't'
			&& !(map[rcp[4]]&(DIGIT|LETTER))) {
				cp = rcp + 4;
				return SHORT;
			}
			if (rcp[0] == 'i'
			&&  rcp[1] == 'g'
			&&  rcp[2] == 'n'
			&&  rcp[3] == 'e'
			&&  rcp[4] == 'd'
			&& !(map[rcp[5]]&(DIGIT|LETTER))) {
				cp = rcp + 5;
				return SIGNED;
			}
			if (rcp[0] == 'i'
			&&  rcp[1] == 'z'
			&&  rcp[2] == 'e'
			&&  rcp[3] == 'o'
			&&  rcp[4] == 'f'
			&& !(map[rcp[5]]&(DIGIT|LETTER))) {
				cp = rcp + 5;
				return SIZEOF;
			}
			if (rcp[0] == 't'
			&&  rcp[1] == 'a'
			&&  rcp[2] == 't'
			&&  rcp[3] == 'i'
			&&  rcp[4] == 'c'
			&& !(map[rcp[5]]&(DIGIT|LETTER))) {
				cp = rcp + 5;
				return STATIC;
			}
			if (rcp[0] == 't'
			&&  rcp[1] == 'r'
			&&  rcp[2] == 'u'
			&&  rcp[3] == 'c'
			&&  rcp[4] == 't'
			&& !(map[rcp[5]]&(DIGIT|LETTER))) {
				cp = rcp + 5;
				return STRUCT;
			}
			if (rcp[0] == 'w'
			&&  rcp[1] == 'i'
			&&  rcp[2] == 't'
			&&  rcp[3] == 'c'
			&&  rcp[4] == 'h'
			&& !(map[rcp[5]]&(DIGIT|LETTER))) {
				cp = rcp + 5;
				return SWITCH;
			}
			goto id;
    //typedef 等关键字的识别
		case 't':
			if (rcp[0] == 'y'
			&&  rcp[1] == 'p'
			&&  rcp[2] == 'e'
			&&  rcp[3] == 'd'
			&&  rcp[4] == 'e'
			&&  rcp[5] == 'f'
			&& !(map[rcp[6]]&(DIGIT|LETTER))) {
				cp = rcp + 6;
				return TYPEDEF;
			}
			goto id;
    //union, usigined 等关键字的识别
		case 'u':
			if (rcp[0] == 'n'
			&&  rcp[1] == 'i'
			&&  rcp[2] == 'o'
			&&  rcp[3] == 'n'
			&& !(map[rcp[4]]&(DIGIT|LETTER))) {
				cp = rcp + 4;
				return UNION;
			}
			if (rcp[0] == 'n'
			&&  rcp[1] == 's'
			&&  rcp[2] == 'i'
			&&  rcp[3] == 'g'
			&&  rcp[4] == 'n'
			&&  rcp[5] == 'e'
			&&  rcp[6] == 'd'
			&& !(map[rcp[7]]&(DIGIT|LETTER))) {
				cp = rcp + 7;
				return UNSIGNED;
			}
			goto id;
    //void, volatile 等关键字的识别
		case 'v':
			if (rcp[0] == 'o'
			&&  rcp[1] == 'i'
			&&  rcp[2] == 'd'
			&& !(map[rcp[3]]&(DIGIT|LETTER))) {
				cp = rcp + 3;
				tsym = voidtype->u.sym;
				return VOID;
			}
			if (rcp[0] == 'o'
			&&  rcp[1] == 'l'
			&&  rcp[2] == 'a'
			&&  rcp[3] == 't'
			&&  rcp[4] == 'i'
			&&  rcp[5] == 'l'
			&&  rcp[6] == 'e'
			&& !(map[rcp[7]]&(DIGIT|LETTER))) {
				cp = rcp + 7;
				return VOLATILE;
			}
			goto id;
    //while 等关键字的识别
		case 'w':
			if (rcp[0] == 'h'
			&&  rcp[1] == 'i'
			&&  rcp[2] == 'l'
			&&  rcp[3] == 'e'
			&& !(map[rcp[4]]&(DIGIT|LETTER))) {
				cp = rcp + 4;
				return WHILE;
			}
			goto id;
    //_typecode, _firstarg 等关键字的识别
		case '_':
			if (rcp[0] == '_'
			&&  rcp[1] == 't'
			&&  rcp[2] == 'y'
			&&  rcp[3] == 'p'
			&&  rcp[4] == 'e'
			&&  rcp[5] == 'c'
			&&  rcp[6] == 'o'
			&&  rcp[7] == 'd'
			&&  rcp[8] == 'e'
			&& !(map[rcp[9]]&(DIGIT|LETTER))) {
				cp = rcp + 9;
				return TYPECODE;
			}
			if (rcp[0] == '_'
			&&  rcp[1] == 'f'
			&&  rcp[2] == 'i'
			&&  rcp[3] == 'r'
			&&  rcp[4] == 's'
			&&  rcp[5] == 't'
			&&  rcp[6] == 'a'
			&&  rcp[7] == 'r'
			&&  rcp[8] == 'g'
			&& !(map[rcp[9]]&(DIGIT|LETTER))) {
				cp = rcp + 9;
				return FIRSTARG;
			}
			goto id;
		default:
			if ((map[cp[-1]]&BLANK) == 0)
				if (cp[-1] < ' ' || cp[-1] >= 0177)
					error("illegal character `\\0%o'\n", cp[-1]);
				else
					error("illegal character `%c'\n", cp[-1]);
		}
	}
}

/**
 * 对于 interge-constant 的处理
 * 根据后缀来判断类型
 *  1、U(u) 和 L(l) 都出现： unsigned long
 *  2、只出现 U(u)： unsigned，如果溢出，则为  unsigned long
 *  3、只出现 L(l): long, 但是当 n 大于 MAX_LONG, 那么就是 unsigned long
 *  4、没有任何后缀:  longtype, 如果溢出，则为  unsigned long
 *  5、等等，具体的请看下面的代码
 */
static Symbol icon(unsigned long n, int overflow, int base) {

  // 确定 n 的类型
	if ((*cp=='u'||*cp=='U') && (cp[1]=='l'||cp[1]=='L')
	||  (*cp=='l'||*cp=='L') && (cp[1]=='u'||cp[1]=='U')) {
		tval.type = unsignedlong;
		cp += 2;
	} else if (*cp == 'u' || *cp == 'U') {
		if (overflow || n > unsignedtype->u.sym->u.limits.max.i)
			tval.type = unsignedlong;
		else
			tval.type = unsignedtype;
		cp += 1;
	} else if (*cp == 'l' || *cp == 'L') {
		if (overflow || n > longtype->u.sym->u.limits.max.i)
			tval.type = unsignedlong;
		else
			tval.type = longtype;
		cp += 1;
	} else if (overflow || n > longtype->u.sym->u.limits.max.i)
		tval.type = unsignedlong;
	else if (n > inttype->u.sym->u.limits.max.i)
		tval.type = longtype;
	else if (base != 10 && n > inttype->u.sym->u.limits.max.i)
		tval.type = unsignedtype;
	else
		tval.type = inttype;

  /**
   * 确定 n 的类型之后，将 n 的值保存在 tval(Symbol)中
   * 至于这里为什么 switch 的 case 只有 INT 和 UNSIGNED，可以查看 types.case 的 118 - 132 行
   */
	switch (tval.type->op) {
	case INT:
		if (overflow || n > tval.type->u.sym->u.limits.max.i) {
			warning("overflow in constant `%S'\n", token,
				(char*)cp - token);
			tval.u.c.v.i = tval.type->u.sym->u.limits.max.i;
		} else
			tval.u.c.v.i = n;
		break;
	case UNSIGNED:
		if (overflow || n > tval.type->u.sym->u.limits.max.u) {
			warning("overflow in constant `%S'\n", token,
				(char*)cp - token);
			tval.u.c.v.u = tval.type->u.sym->u.limits.max.u;
		} else
			tval.u.c.v.u = n;
		break;
	default: assert(0);
	}
	ppnumber("integer"); // 数字预处理
	return &tval;  // 返回终结符的地址
}

/**
 * 数字预处理
 */
static void ppnumber(char *which) {
	unsigned char *rcp = cp--;
  // 跳过 E、e、-、+
	for ( ; (map[*cp]&(DIGIT|LETTER)) || *cp == '.'; cp++)
		if ((cp[0] == 'E' || cp[0] == 'e')
		&&  (cp[1] == '-' || cp[1] == '+'))
			cp++;
	if (cp > rcp)
		error("`%S' is a preprocessing number but an invalid %s constant\n", token,

			(char*)cp-token, which);
}

/**
 * 处理 . e E 的后缀
 * 根据后缀来判断类型：
 *  1、f 或者 F： float
 *  2、l 或者 L: long double
 *  3、没有后缀： double
 */
static Symbol fcon(void) {
	if (*cp == '.')
		do
			cp++;
		while (map[*cp]&DIGIT);

  // 类似 1e-2 = 1 * 10^(12)
	if (*cp == 'e' || *cp == 'E') {
		if (*++cp == '-' || *cp == '+')  // 前面先做了自加，所以后面不需要自加
			cp++;
		if (map[*cp]&DIGIT)  //判断 e 或者 E 之后的第一位是否为数字
			do
				cp++;
			while (map[*cp]&DIGIT);
		else
			error("invalid floating constant `%S'\n", token,(char*)cp - token);
	}

	errno = 0;
	tval.u.c.v.d = strtod(token, NULL);
	if (errno == ERANGE)
		warning("overflow in floating constant `%S'\n", token,(char*)cp - token);

  // 根据后缀判断 n 的类型
	if (*cp == 'f' || *cp == 'F') {
		++cp;
		if (tval.u.c.v.d > floattype->u.sym->u.limits.max.d)
			warning("overflow in floating constant `%S'\n", token,
				(char*)cp - token);
		tval.type = floattype;
	} else if (*cp == 'l' || *cp == 'L') {
		cp++;
		tval.type = longdouble;
	} else {
		if (tval.u.c.v.d > doubletype->u.sym->u.limits.max.d)
			warning("overflow in floating constant `%S'\n", token,
				(char*)cp - token);
		tval.type = doubletype;
	}
	ppnumber("floating");
	return &tval;
}

/*
 * 将字符放入对应的缓存中
 * param c: 字符对应的 ascii
 * param cl: buffer
 */
static void *cput(int c, void *cl) {
	char *s = cl;

	if (c < 0 || c > 255)
		warning("overflow in escape sequence with resulting value `%d'\n", c);
	*s++ = c;
	return s;
}

/**
 * 与 cput 方法类似
 */
static void *wcput(int c, void *cl) {
	unsigned int *s = cl;

	*s++ = c;
	return s;
}

/**
 * 处理字符串
 * param q： \' 或者 “ 的 ascii码
 * 根据 q 来判读是字符还是字符串，最后读到到字符保存到 cl 中
 */
static void *scon(int q, void *put(int c, void *cl), void *cl) {
	int n = 0, nbad = 0;
	do {
		cp++;
		while (*cp != q) { // 判断本次读取是否结束
			int c;
			if (map[*cp]&NEWLINE) {  // 判断是否读到换行符
				if (cp < limit)   //
					break;
				cp++;
				nextline();
				if (cp == limit)
					break;
				continue;
			}
			c = *cp++;
			if (c == '\\') {  // c语言中 \\ 跟一个换行符表示换行
				if (map[*cp]&NEWLINE) {
					if (cp++ < limit) // 这里的换行，可能被预处理处理掉了，所以继续 continue
						continue;
					nextline();
				}
        // 不是换行时的处理
				if (limit - cp < MAXTOKEN)
					fillbuf();
				c = backslash(q);
			} else if (c < 0 || c > 255 || map[c] == 0)
				nbad++;
			if (n++ < BUFSIZE)
				cl = put(c, cl);
		}
		if (*cp == q)
			cp++;
		else
			error("missing %c\n", q);
		if (q == '"' && put == wcput && getchr() == 'L') {
			if (limit - cp < 2)
				fillbuf();
			if (cp[1] == '"')
				cp++;
		}
	} while (q == '"' && getchr() == '"');
  // 字符串读取完成，将 \0 加入到字符串末尾
	cl = put(0, cl);
	if (n >= BUFSIZE) // 超长字符串 > 4096
		error("%s literal too long\n", q == '"' ? "string" : "character");
	if (Aflag >= 2 && q == '"' && n > 509)
		warning("more than 509 characters in a string literal\n");
	if (Aflag >= 2 && nbad > 0)
		warning("%s literal contains non-portable characters\n",
			q == '"' ? "string" : "character");
	return cl;
}

int getchr(void) {
	for (;;) {
		while (map[*cp]&BLANK) // 跳过空格
			cp++;
		if (!(map[*cp]&NEWLINE)) //不是换行符
			return *cp;
    // 当前字符属于换行符
		cp++;
		nextline();  // 读取一行
		if (cp == limit)
			return EOI;
	}
}
static int backslash(int q) {
	unsigned int c;

	switch (*cp++) {
	case 'a': return 7;
	case 'b': return '\b';
	case 'f': return '\f';
	case 'n': return '\n';
	case 'r': return '\r';
	case 't': return '\t';
	case 'v': return '\v';
	case '\'': case '"': case '\\': case '\?': break;
  // \x utf-8 编码的字符
	case 'x': {
		int overflow = 0;
		if ((map[*cp]&(DIGIT|HEX)) == 0) {
			if (*cp < ' ' || *cp == 0177)
				error("ill-formed hexadecimal escape sequence\n");
			else
				error("ill-formed hexadecimal escape sequence `\\x%c'\n", *cp);
			if (*cp != q)
				cp++;
			return 0;
		}

		for (c = 0; map[*cp]&(DIGIT|HEX); cp++) {
			if (c >> (8*widechar->size - 4))
				overflow = 1;
			if (map[*cp]&DIGIT)
				c = (c<<4) + *cp - '0';
			else
				c = (c<<4) + (*cp&~040) - 'A' + 10;
		}
		if (overflow)
			warning("overflow in hexadecimal escape sequence\n");
		return c&ones(8*widechar->size);
		}

	case '0': case '1': case '2': case '3':
	case '4': case '5': case '6': case '7':
		c = *(cp-1) - '0';
		if (*cp >= '0' && *cp <= '7') {
			c = (c<<3) + *cp++ - '0';
			if (*cp >= '0' && *cp <= '7')
				c = (c<<3) + *cp++ - '0';
		}
		return c;
	default:
		if (cp[-1] < ' ' || cp[-1] >= 0177)
			warning("unrecognized character escape sequence\n");
		else
			warning("unrecognized character escape sequence `\\%c'\n", cp[-1]);
	}
	return cp[-1];
}
