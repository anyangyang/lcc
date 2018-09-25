#include "c.h"
#include <stdio.h>

static char rcsid[] = "$Id$";

#define equalp(x) v.x == p->sym.u.c.v.x

/**
 * 符号表的结构体定义
 */
struct table {
	int level;  // 标识作用域
	Table previous;
	struct entry {
		struct symbol sym;
		struct entry *link;
	} *buckets[256];
	Symbol all;
};
#define HASHSIZE NELEMS(((Table)0)->buckets)
static struct table
	cns = { CONSTANTS },
	ext = { GLOBAL },
	ids = { GLOBAL },
	tys = { GLOBAL };
Table constants   = &cns;
Table externals   = &ext;
Table identifiers = &ids;
Table globals     = &ids;
Table types       = &tys;
Table labels;
int level = GLOBAL;  // 全局变量 ，和对应的表一起表示一个作用域
static int tempid;
List loci, symbols;

/**
 * 初始化一个新的作用域符号表
 * param arena： 新的符号表需要分配的内存大小
 */
Table newtable(int arena) {
	Table new;

	NEW0(new, arena);
	return new;
}

/**
 * 新建嵌套作用域的符号表，链接到外层作用域的符号表
 * param tp: 外层作用域符号表的指针
 * param level： 嵌套作用域符号表的作用域等级
 * return： 已经初始化完成的作用域符号表
 */
Table table(Table tp, int level) {
	/**
	 * func 定义在 c.h 中，是枚举中的一个元素，值为 1
	 * 表示内存分配区的下表
	 */
	Table new = newtable(FUNC);
	new->previous = tp;
	new->level = level;
	if (tp)
		new->all = tp->all;
	return new;
}

void foreach(Table tp, int lev, void (*apply)(Symbol, void *), void *cl) {
	assert(tp);
	while (tp && tp->level > lev)
		tp = tp->previous;
	if (tp && tp->level == lev) {
		Symbol p;
		Coordinate sav;
		sav = src;
		for (p = tp->all; p && p->scope == lev; p = p->up) {
			src = p->src;
			(*apply)(p, cl);
		}
		src = sav;
	}
}

/**
 * 进入作用域，全局变量 level 递增
 */
void enterscope(void) {
	if (++level == LOCAL)
		tempid = 0;
}
/**
 * 退出作用域，全局变量 level 递减
 * 相应的 types（table）和 identifiers（table）也随之撤销
 */
void exitscope(void) {
	rmtypes(level);  // 带标记带类型，从类型缓冲中移除
	if (types->level == level)
		types = types->previous;
	if (identifiers->level == level) {
		if (Aflag >= 2) {
			int n = 0;
			Symbol p;
			for (p = identifiers->all; p && p->scope == level; p = p->up)
				if (++n > 127) {
					warning("more than 127 identifiers declared in a block\n");
					break;
				}
		}
		identifiers = identifiers->previous;
	}
	assert(level >= GLOBAL);  // 验证全局作用域不能被移除
	--level;
}

/**
 * 给给定的 name 分配一个符号，并将该符号加入到到给定作用域值对应的符号表中
 * param name： 给定的 name
 * param level： name 对应的 Symbol 的 scope
 * param arena： symbol 的成员字段 entry 的大小
 * return： 新增加的符号的地址
 */
Symbol install(const char *name, Table *tpp, int level, int arena) {
	Table tp = *tpp;
	struct entry *p;
	// HASHSIZE 定义在当前文件中，表示 table 中 entry 的数量
	// 计算当前 name 在 table bucket 位置
	unsigned h = (unsigned long)name&(HASHSIZE-1);

	assert(level == 0 || level >= tp->level);
	if (level > 0 && tp->level < level)
		tp = *tpp = table(tp, level);
	NEW0(p, arena);
	p->sym.name = (char *)name;
	p->sym.scope = level;  // 当前 name 的作用域
	p->sym.up = tp->all;   // 在 all 所代表的 链表中加入一个节点
	tp->all = &p->sym;
	p->link = tp->buckets[h]; //在 table 的 hashTable 对应 bucket 的链表上加上一个 entry
	tp->buckets[h] = p;
	return &p->sym;
}

/**
 * 将 name 对应的节点从 src_table 迁移到 dest_table
 */
Symbol relocate(const char *name, Table src, Table dst) {
	struct entry *p, **q;
	Symbol *r;
	unsigned h = (unsigned long)name&(HASHSIZE-1);

	for (q = &src->buckets[h]; *q; q = &(*q)->link)
		if (name == (*q)->sym.name)
			break;
	assert(*q);
	/*
	 Remove the entry from src's hash chain
	  and from its list of all symbols.
	*/
	p = *q;
	*q = (*q)->link;
	for (r = &src->all; *r && *r != &p->sym; r = &(*r)->up)
		;
	assert(*r == &p->sym);
	*r = p->sym.up;
	/*
	 Insert the entry into dst's hash chain
	  and into its list of all symbols.
	  Return the symbol-table entry.
	*/
	p->link = dst->buckets[h];
	dst->buckets[h] = p;
	p->sym.up = dst->all;
	dst->all = &p->sym;
	return &p->sym;
}

/**
 * 给定一个 name，从指定的标号表中开始搜索，如果在当前节点搜索不到
 * 就从当前符号表的上一个作用域的符号表进行搜素
 */
Symbol lookup(const char *name, Table tp) {
	struct entry *p;
	unsigned h = (unsigned long)name&(HASHSIZE-1);

	assert(tp);
	do
		for (p = tp->buckets[h]; p; p = p->link)
			if (name == p->sym.name)
				return &p->sym;
	while ((tp = tp->previous) != NULL);
	return NULL;
}

/**
 * 标号管理： 编译器产生的标号和源程序中的标号的内部表示统一采取整数的方式进行管理
 */
int genlabel(int n) {
	static int label = 1;  // 静态变量 ---> 全局变量

	label += n;
	return label - n;
}
/**
 * 在 label_table 中搜索 label（int）
 * 如果搜索到的话，返回 lable 对应的 symbol 地址
 * 如果搜索不到，就创建一个新的 symbol，挂在链表上
 */
Symbol findlabel(int lab) {
	struct entry *p;
	unsigned h = lab&(HASHSIZE-1);

	for (p = labels->buckets[h]; p; p = p->link)
		if (lab == p->sym.u.l.label)
			return &p->sym;
	NEW0(p, FUNC);
	p->sym.name = stringd(lab);
	p->sym.scope = LABELS;
	p->sym.up = labels->all;
	labels->all = &p->sym;
	p->link = labels->buckets[h];
	labels->buckets[h] = p;
	p->sym.generated = 1;
	p->sym.u.l.label = lab;
	(*IR->defsymbol)(&p->sym);
	return &p->sym;
}

/**
 * 常量表
 */
Symbol constant(Type ty, Value v) {
	struct entry *p;
	unsigned h = v.u&(HASHSIZE-1);
	static union { int x; char endian; } little = { 1 };

	ty = unqual(ty);
	for (p = constants->buckets[h]; p; p = p->link)
		if (eqtype(ty, p->sym.type, 1))
			switch (ty->op) {
			case INT:      if (equalp(i)) return &p->sym; break;
			case UNSIGNED: if (equalp(u)) return &p->sym; break;
			case FLOAT:
				if (v.d == 0.0) {
					float z1 = v.d, z2 = p->sym.u.c.v.d;
					char *b1 = (char *)&z1, *b2 = (char *)&z2;
					if (z1 == z2
					&& (!little.endian && b1[0] == b2[0]
					||   little.endian && b1[sizeof (z1)-1] == b2[sizeof (z2)-1]))
						return &p->sym;
				} else if (equalp(d))
					return &p->sym;
				break;
			case FUNCTION: if (equalp(g)) return &p->sym; break;
			case ARRAY:
			case POINTER:  if (equalp(p)) return &p->sym; break;
			default: assert(0);
			}
	NEW0(p, PERM);
	p->sym.name = vtoa(ty, v);
	p->sym.scope = CONSTANTS;
	p->sym.type = ty;
	p->sym.sclass = STATIC;
	p->sym.u.c.v = v;
	p->link = constants->buckets[h];
	p->sym.up = constants->all;
	constants->all = &p->sym;
	constants->buckets[h] = p;
	if (ty->u.sym && !ty->u.sym->addressed)
		(*IR->defsymbol)(&p->sym);
	p->sym.defined = 1;
	return &p->sym;
}
Symbol intconst(int n) {
	Value v;

	v.i = n;
	return constant(inttype, v);
}
Symbol genident(int scls, Type ty, int lev) {
	Symbol p;

	NEW0(p, lev >= LOCAL ? FUNC : PERM);  // lev 表示在哪个分配区进行分配
	p->name = stringd(genlabel(1)); // 生成标号，并从 string pool 获取对应字符串首地址
	p->scope = lev;
	p->sclass = scls;  // 扩展信息
	p->type = ty;
	p->generated = 1;
	if (lev == GLOBAL)
		(*IR->defsymbol)(p); //通知编译器后端，这些常量可以出现在 dag（无环路有向图）中
	return p;
}

/**
 * 临时变量
 */
Symbol temporary(int scls, Type ty) {
	Symbol p;

	NEW0(p, FUNC);
	p->name = stringd(++tempid);
	p->scope = level < LOCAL ? LOCAL : level;
	p->sclass = scls;
	p->type = ty;
	p->temporary = 1;
	p->generated = 1;
	return p;
}
/**
 * 编译器产生的临时变量，比如为了腾空寄存器
 * 由于不知道这个临时变量的类型，所以不能直接调用 temporaray
 */
Symbol newtemp(int sclass, int tc, int size) {
	Symbol p = temporary(sclass, btot(tc, size));

	(*IR->local)(p);
	p->defined = 1;
	return p;
}

Symbol allsymbols(Table tp) {
	return tp->all;
}

void locus(Table tp, Coordinate *cp) {
	loci    = append(cp, loci);
	symbols = append(allsymbols(tp), symbols);
}

void use(Symbol p, Coordinate src) {
	Coordinate *cp;

	NEW(cp, PERM);
	*cp = src;
	p->uses = append(cp, p->uses);
}
/* findtype - find type ty in identifiers */
Symbol findtype(Type ty) {
	Table tp = identifiers;
	int i;
	struct entry *p;

	assert(tp);
	do
		for (i = 0; i < HASHSIZE; i++)
			for (p = tp->buckets[i]; p; p = p->link)
				if (p->sym.type == ty && p->sym.sclass == TYPEDEF)
					return &p->sym;
	while ((tp = tp->previous) != NULL);
	return NULL;
}

/* mkstr - make a string constant */
Symbol mkstr(char *str) {
	Value v;
	Symbol p;

	v.p = str;
	p = constant(array(chartype, strlen(v.p) + 1, 0), v);
	if (p->u.c.loc == NULL)
		p->u.c.loc = genident(STATIC, p->type, GLOBAL);
	return p;
}

/* mksymbol - make a symbol for name, install in &globals if sclass==EXTERN */
Symbol mksymbol(int sclass, const char *name, Type ty) {
	Symbol p;

	if (sclass == EXTERN)
		p = install(string(name), &globals, GLOBAL, PERM);
	else {
		NEW0(p, PERM);
		p->name = string(name);
		p->scope = GLOBAL;
	}
	p->sclass = sclass;
	p->type = ty;
	(*IR->defsymbol)(p);
	p->defined = 1;
	return p;
}

/* vtoa - return string for the constant v of type ty */
char *vtoa(Type ty, Value v) {
	ty = unqual(ty);
	switch (ty->op) {
	case INT:      return stringd(v.i);
	case UNSIGNED: return stringf((v.u&~0x7FFF) ? "0x%X" : "%U", v.u);
	case FLOAT:    return stringf("%g", (double)v.d);
	case ARRAY:
		if (ty->type == chartype || ty->type == signedchar
		||  ty->type == unsignedchar)
			return v.p;
		return stringf("%p", v.p);
	case POINTER:  return stringf("%p", v.p);
	case FUNCTION: return stringf("%p", v.g);
	}
	assert(0); return NULL;
}
