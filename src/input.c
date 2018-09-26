#include "c.h"

static char rcsid[] = "$Id$";

static void pragma(void);
static void resynch(void);

/**
 * bsize 有三种状态：
 * 小于 0： 表示没有字符被读入，可能遇到了读入错误
 * 等于 0：表示到达输入末尾
 * 大于 0：表示读入了 bsize 个字符
 */
static int bsize;
static unsigned char buffer[MAXLINE+1 + BUFSIZE+1];
unsigned char *cp;	/* current input character */
char *file;		/* current input file name */
char *firstfile;	/* first input file */
unsigned char *limit;	/* points to last character + 1 */
char *line;		/* 当前行的起始位置 */
int lineno;		/* line number of current line */

/**
 * 读取一行数据
 */
void nextline(void) {
	do {
		if (cp >= limit) { //缓冲区空
			fillbuf();
			if (cp >= limit)  // 填充缓冲区之后，缓冲区还是空
				cp = limit;
			if (cp == limit)
				return;
		} else {
			lineno++;
			/* 过滤前置的空格和制表符（tab键引起）*/
			for (line = (char *)cp; *cp==' ' || *cp=='\t'; cp++)
				;
			// 如果读入的字符是 #，那么就要进行文件预处理
			if (*cp == '#') {
				resynch();
				nextline();
			}
		}
	} while (*cp == '\n' && cp == limit);
}

/**
 * 填充缓冲区
 */
void fillbuf(void) {
	if (bsize == 0) // 判断是否到达末尾
		return;
	if (cp >= limit)  // cp >= limit, 表示输入缓冲区为空
		cp = &buffer[MAXLINE+1];
	else
		{
			int n = limit - cp;
			unsigned char *s = &buffer[MAXLINE+1] - n;  // 将当前未读完的字符往前移
			assert(s >= buffer);
			line = (char *)s - ((char *)cp - line);  // 这里暂时不懂
			while (cp < limit)
				*s++ = *cp++;
			cp = &buffer[MAXLINE+1] - n;  // cp 指向移动完成之后的第一个字符的位置
		}
	if (feof(stdin)) //读到末尾
		bsize = 0;
	else
		bsize = fread(&buffer[MAXLINE+1], 1, BUFSIZE, stdin); // 读取最多 4096 个字节，最后一次读取可能不会超过 4096
	if (bsize < 0) {
		error("read error\n");
		exit(EXIT_FAILURE);
	}
	limit = &buffer[MAXLINE+1+bsize];  // 重新计算 limit
	*limit = '\n';
}

void input_init(int argc, char *argv[]) {
	static int inited;

	if (inited)
		return;
	inited = 1;
	main_init(argc, argv);
	limit = cp = &buffer[MAXLINE+1];  //MAXLINE：输入缓冲区中尚未处理的尾部可包含的最大字符数
	bsize = -1;
	lineno = 0;
	file = NULL;
	fillbuf();
	if (cp >= limit) // 如果缓冲区为空
		cp = limit;
	nextline();
}

/* ident - handle #ident "string" */
static void ident(void) {
	while (*cp != '\n' && *cp != '\0')
		cp++;
}

/* pragma - handle #pragma ref id... */
static void pragma(void) {
	if ((t = gettok()) == ID && strcmp(token, "ref") == 0)
		for (;;) {
			while (*cp == ' ' || *cp == '\t')
				cp++;
			if (*cp == '\n' || *cp == 0)
				break;
			if ((t = gettok()) == ID && tsym) {
				tsym->ref++;
				use(tsym, src);
			}
		}
}

/* resynch - set line number/file name in # n [ "file" ], #pragma, etc. */
static void resynch(void) {
	for (cp++; *cp == ' ' || *cp == '\t'; )
		cp++;
	if (limit - cp < MAXLINE) // 如果当前缓冲区小于 512 个字节，读入数据，填充缓冲区
		fillbuf();
	if (strncmp((char *)cp, "pragma", 6) == 0) {
		cp += 6;
		pragma();  // Pragma 预处理：设定编译器的状态，或者指示编译器完成一系列动作
	} else if (strncmp((char *)cp, "ident", 5) == 0) {
		cp += 5;
		ident();
	} else if (*cp >= '0' && *cp <= '9') {
	line:	for (lineno = 0; *cp >= '0' && *cp <= '9'; )
			lineno = 10*lineno + *cp++ - '0';
		lineno--;
		while (*cp == ' ' || *cp == '\t')
			cp++;
		if (*cp == '"') {
			file = (char *)++cp;
			while (*cp && *cp != '"' && *cp != '\n')
				cp++;
			file = stringn(file, (char *)cp - file);
			if (*cp == '\n')
				warning("missing \" in preprocessor line\n");
			if (firstfile == 0)
				firstfile = file;
		}
	} else if (strncmp((char *)cp, "line", 4) == 0) {
		for (cp += 4; *cp == ' ' || *cp == '\t'; )
			cp++;
		if (*cp >= '0' && *cp <= '9')
			goto line;
		if (Aflag >= 2)
			warning("unrecognized control line\n");
	} else if (Aflag >= 2 && *cp != '\n')
		warning("unrecognized control line\n");
	while (*cp)
		if (*cp++ == '\n')
			if (cp == limit + 1) {
				nextline();
				if (cp == limit)
					break;
			} else
				break;
}
