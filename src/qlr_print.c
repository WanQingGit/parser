/*
 * qlr_print.c
 *
 *  Created on: Jun 14, 2019
 *  Author: WanQing<1109162935@qq.com>
 */
#include "qlr.h"
#include "qtokenizer.h"
#include "qcontrol.h"
#include "qlogger.h"
void printGram(qstrbuf *buf, qgen *generator, lr_gram *grammar) {
	static char temp[1024];
	static char *NTFORM = "%s  ", *TFORM = "'%s' ";
	char *ptr = temp;
	ptr += sprintf(ptr, "%s: ", gsymbol(generator, grammar->left));
	for (int i = 0; i < grammar->right->length; i++) {
		int index = grammar->right->data[i].i;
		int id = index & IDMASK;
		if ((index & TFLAG) && id > TW_NIL) {
			ptr += sprintf(ptr, TFORM, gsymbol(generator, id));
		} else
			ptr += sprintf(ptr, NTFORM, gsymbol(generator, id));
	}
	*ptr = '\0';
	if (buf)
		STR.add(buf, temp, ptr - temp);
	else
		printf("%s ", temp);
}
void printfToken(qgen *gen, qbytes* tokens, int top) {
	for (int i = top; i < tokens->length; i++) {
		qtk *token = Bytes_get(tokens, i,qtk);
		printf("%s ", gsymbol(gen, token->type&IDMASK));
	}
}
void printStackInfo(qgen *gen, qvec stack, qvec stackInfo) {
	for (int i = 0; i < stack->length; i++) {
		printf("%d ", arr_data(stack, int, i));
	}
	printf(" | ");
	int i = stackInfo->length - 5;
	if (i < 0)
		i = 0;
	else
		printf("... ");
	for (; i < stackInfo->length; i++) {
		printf("%s ", gsymbol(gen, arr_data(stackInfo,int,i)));
//		printf("%s ", gsymbol(gen, arr_data(stackInfo,int,i)&IDMASK));
	}
}
void printfExpect(qgen *generator, int *rule) {
//	int len = analyzer->parser->symbols.length;
	int len = generator->symbols->length;
	printf("expect :");
	for (int i = 0; i < len; i++) {
		if (rule[i]) {
			printf("%s ", gsymbol(generator, i));
		}
	}
}
void printAllSymbol(qgen *gen) {
	int len = gen->symbols->length;
	for (int i = 0; i < len; i++) {
		printf("%d : %s\n", i, gsymbol(gen, i));
	}
}
void printClosures(LRAnalyzer *analyzer, qvec clos) {
	printf("\n");
	for (int i = 0; i < clos->length; i++) {
		printf("closure %d:\n", i);
		lr_closure *clo = cast(lr_closure *, clos->data[i].p);
		RBTree * cloItems = clo->clo_items;
		printClosure(analyzer, cloItems);
	}
}
void printItems(qgen *gen, RBTree *t) {
	printf("\n");
	RBNode *iter = RB.min(t);
	while (iter) {
		lr_item *item = cast(lr_item*, iter->key);
		printGram(NULL, gen, item->gram);
		printf("\n");
		iter = RB.next(iter);
	}
}
void printExpect(qstrbuf *buf, LRAnalyzer *an, RBTree *t) {
	qgen *gen = an->gen;
	RBNode *iter = RB.min(t);
	char temp[64];
	int i = t->length;
	STR.add(buf, ", ", 2);
	while (i--) {
		int index = cast(int, iter->key);
		if (i == 0)
			STR.add(buf, temp, sprintf(temp, "%s ", gsymbol(gen, index&IDMASK)));
		else
			STR.add(buf, temp, sprintf(temp, "%s / ", gsymbol(gen, index&IDMASK)));
		iter = RB.next(iter);
	}
	if (RB.search(an->all_expect, t) == NULL) {
		skyc_assert_(0);
	}
}
void printAllExpect(LRAnalyzer *analyzer) {
	printf("\nAllExpect:");
	// @formatter:off
	rb_each(analyzer->all_expect, {
			printExpect(&analyzer->logger->buf,analyzer,cast(RBTree*,_iter->key));
	});
	printf("\n");
}
// @formatter:on
void printItem(qstrbuf *strbuf, qgen *generator, lr_item *item) {
	lr_gram *grammar = item->gram;
	static char *NTFORM = "%s ", *TFORM = "'%s' ";
	char buf[1024];
	char *ptr = buf;
	ptr += sprintf(ptr, "%s -> ", gsymbol(generator, grammar->left));
	for (int i = 0; i < grammar->right->length; i++) {
		int index = grammar->right->data[i].i;
		int id = index & IDMASK;
		if (i == item->at && id != TW_NIL) {
			ptr += sprintf(ptr, "• ");
		}
		if (index & TFLAG && id > TW_NIL) {
			ptr += sprintf(ptr, TFORM, gsymbol(generator, id));
		} else
			ptr += sprintf(ptr, NTFORM, gsymbol(generator, id));
	}
	if (grammar->right->length == item->at) {
		ptr += sprintf(ptr, "• ");
	}
//	*ptr = '\0';
	STR.add(strbuf, buf, ptr - buf);
//	printf("%s ", buf);
}
void printCloItems(LRAnalyzer *analyzer, RBTree* cloItems) {
	qlogger *logger = analyzer->logger;
	RBNode *iter = RB.min(cloItems);
	while (iter) {
		lr_clo_item *cloItem = cast(lr_clo_item*, iter->key);
		printItem(&logger->buf, analyzer->gen, cloItem->item);
		printExpect(&logger->buf, analyzer, cloItem->expect);
		Log.add(logger, "\n", 1);
		iter = RB.next(iter);
	}
}
void printClosure(LRAnalyzer *analyzer, lr_closure* clo) {
	char temp[64];
	qlogger *logger = analyzer->logger;
	Log.add(logger, temp, sprintf(temp, "closure %d:\n", clo->index));
	printCloItems(analyzer,clo->clo_items);
}
