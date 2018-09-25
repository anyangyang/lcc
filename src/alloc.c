#include "c.h"
/**
 * 分配区的定义
 * param next：每个分配区都是由一组很大的内存块构成的链表，next 指向下一个节点
 * param avail： 指向当前内存块中可分配区域的首地址
 * param limit： 从 avail 开始，到 limit 为止，都是连续可分配的地址
 */
struct block {
	struct block *next;
	char *limit;
	char *avail;
};
union align {
	long l;
	char *p;
	double d;
	int (*f)(void);
};

/**
 * 内存块链表的头部定义
 */
union header {
	struct block b;
	union align a;
};
#ifdef PURIFY
union header *arena[3];  // 内存块链表头部初始化三个内存分配区

/**
 *
 */
void *allocate(unsigned long n, unsigned a) {
	union header *new = malloc(sizeof *new + n);
	/**
	 * NELEMS 是一个宏，定义在 c.h 文件中
	 * 用于计算 arena 数组的长度
	 */
	assert(a < NELEMS(arena));
	if (new == NULL) {
		error("insufficient memory\n");
		exit(1);
	}
	new->b.next = (void *)arena[a];
	arena[a] = new;
	return new + 1;
}

void deallocate(unsigned a) {
	union header *p, *q;

	assert(a < NELEMS(arena));
	for (p = arena[a]; p; p = q) {
		q = (void *)p->b.next;
		free(p);
	}
	arena[a] = NULL;
}

void *newarray(unsigned long m, unsigned long n, unsigned a) {
	return allocate(m*n, a);
}
#else
/**
 *
 */
static struct block
	 first[] = {  { NULL },  { NULL },  { NULL } },
	*arena[] = { &first[0], &first[1], &first[2] };
static struct block *freeblocks;  // 表示被释放的分配区

/**
 * 在分配区进行内存分配
 * param n： 需要的内存长度，单位为字节
 * param a： a 代表分配区的唯一标识
 */
void *allocate(unsigned long n, unsigned a) {
	struct block *ap;
	/**
	 * NELEMS 是一个宏，定义在 c.h 文件中
	 * 用于计算 arena 数组的长度
	 */
	assert(a < NELEMS(arena)); // 利用分配区的数组下标作为分配区的唯一标识，这里利用断言验证是否数组越界
	assert(n > 0);
	ap = arena[a];
	/**
	 * 计算内存对齐之后需要分配的内存长度，该内存长度可以存放任何类型的值
	 * roundup 定义在 c.h 文件中，计算内存对齐之后需要分配的内存长度
	 */
	n = roundup(n, sizeof (union align));

	/*
	 * 如果当前内存分配区的可用内存空间不够
	 * freeblocks：空的分配区，缺省值 NULL
	 */
	while (n > ap->limit - ap->avail) {
		/**
		 * 在这里可以看到分配区链表的形成过程
		 */
		if ((ap->next = freeblocks) != NULL) {
			freeblocks = freeblocks->next;
			ap = ap->next;
		} else {
			// 如果当前分配区的内存空间不够，且当前的内存分配区是最后一个分配区
			// 那么需要重新申请一个内存分配区，用于内存分配，这里保证内存对齐
			unsigned m = sizeof (union header) + n + roundup(10*1024, sizeof (union align));
			ap->next = malloc(m);  // 分配空间，完成挂载
			ap = ap->next;
			if (ap == NULL) {  // 重新申请分配区内存空间失败
				error("insufficient memory\n");
				exit(1);
			}
			ap->limit = (char *)ap + m;  // 新的分配区的限制地址
		}
		ap->avail = (char *)((union header *)ap + 1); //新的分配区的开始地址
		ap->next = NULL;
		arena[a] = ap;   // 在 header 修改当前下标 a 指向的内存分配区，将新分配的内存块添加到分配区链表的头部
	}
	ap->avail += n;  // 分配空间
	return ap->avail - n;  // 返回在分配区分配的内存空间的起始地址
}

void *newarray(unsigned long m, unsigned long n, unsigned a) {
	return allocate(m*n, a);
}

/**
 * 释放下标为 a 的分配区的内存空间
 */
void deallocate(unsigned a) {
	assert(a < NELEMS(arena));  // 验证分配区的内存下标是否越界
	/*
	 * 内存释放第一步：将下标 a 指向的分配区块作为链表的一个节点添加到 freeblocks 指向的链表上
	 */
	arena[a]->next = freeblocks;
	/*
	 * first[a] 是分配区的 header 节点，他指向 arana[a] ,所以现在 freeblocks 作为头结点指向 arena[a]
	 */
	freeblocks = first[a].next;
	// first[a] 作为头结点被重新初始化为 NULL，等待被重新分配
	first[a].next = NULL;
	arena[a] = &first[a];
}
#endif
