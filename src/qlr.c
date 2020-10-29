/*
 * qlr.c
 *
 *  Created on: Jun 14, 2019
 *  Author: WanQing<1109162935@qq.com>
 */

#include "bytelist.h"
#include "qlr.h"
#include "qgrammar.h"
#include "qtokenizer.h"
#include "qio.h"
#include "qcontrol.h"
#include "qlogger.h"
#include "qmem.h"
/**
 * type_cloItem使用cmp_cloItem比较,只比较item，type_cloItem2使用cmp_cloItemDeep，需额外比较expect
 */
Type type_cloItem, type_cloItem2, typeItem, typeExpect, type_Closure;
/**
 * tableClo存取的at=0的lr_clo_item
 * tableClo,tableFirst，tableDeps 索引不含标志位，值 非终结符不含标志位，终结符含标志位
 * tableDeps中的依赖不排除ID_NUL
 */

void mergeExp(LRAnalyzer *analyzer, RBTree *exps, qvec res, qvec exp);
RBTree *insertCloItem(LRAnalyzer *analyzer, lr_clo_item *cloItem);
static bool hasNul(LRAnalyzer *analyzer, RBTree *deps, WordID idx);
static const RBTree *getFirst(LRAnalyzer *analyzer, RBTree *deps, WordID idx);
RBTree *insertExpect(LRAnalyzer *analyzer, RBTree *expect);
int cmpItem(lr_item* i1, lr_item *i2) {
	int res;
	if (i1->gram != i2->gram) {
		res = cmpGram(i1->gram, i2->gram);
		skyc_assert_(res);
		return res;
	}
	return i1->at - i2->at;
}

int cmpExpect(RBTree *a, RBTree *b) {
	if (a == b)
		return 0;
	int res = a->length - b->length;
	if (res)
		return res;
	RBNode *iterA = RB.min(a), *iterB = RB.min(b);
	while (iterA) {
		res = iterA->key - iterB->key;
		if (res)
			return res;
		iterA = RB.next(iterA), iterB = RB.next(iterB);
	}
	return 0;
}
void freeExpect(RBTree *expect) {
	RB.destroy(&expect, RB_NOT_FREE);
}
int cmp_cloItem(lr_clo_item *a, lr_clo_item *b) {
	return cmpItem(a->item, b->item);
}
int cmp_cloItemDeep(lr_clo_item *a, lr_clo_item *b) {
	if (a == b)
		return 0;
	int res;
	if (a->item != b->item) {
		res = cmpItem(a->item, b->item);
		sky_assert(res);
		return res;
	}
	if (a->expect != b->expect) {
		res = cmpExpect(a->expect, b->expect);
		skyc_assert_(res);
		return res;
	} else {
		return 0;
	}
}
int cmpClosure(lr_closure *c1, lr_closure *c2) {
	RBTree *a = c1->clo_items, *b = c2->clo_items;
	int res = a->length - b->length;
	if (res)
		return res;
	RBNode *iterA = RB.min(a), *iterB = RB.min(b);
	while (iterA) {
		lr_clo_item *itemA = cast(lr_clo_item *, iterA->key);
		lr_clo_item *itemB = cast(lr_clo_item *, iterB->key);
		if (itemA->item != itemB->item) {
			res = cmpItem(itemA->item, itemB->item);
			sky_assert(res);
			return res;
		}
		if (itemA->expect != itemB->expect) {
			res = cmpExpect(itemA->expect, itemB->expect);
			sky_assert(res);
			return res;
		}
		iterA = RB.next(iterA), iterB = RB.next(iterB);
	}
	return 0;
}

void forDebug(RBNode *o, RBNode *n) {
	printf("forDebug\n");
}
/**
 * odl已经注册了，不能随意改变
 */
void closureInsertFail(RBNode *o, RBNode *n) {
	lr_clo_item *old = cast(lr_clo_item*, o->key);
	lr_clo_item *none = cast(lr_clo_item*, n->key);
	if (old->expect == none->expect)
		return;
	LRAnalyzer *analyzer = cast(LRAnalyzer*, o->val);
	RBTree *newExp = RB.create(NULL, NULL);
	RBTree *exp = old->expect, *exp2 = none->expect;
	RB.insertAll(newExp, exp, NULL, NULL);
	RB.insertAll(newExp, exp2, NULL, NULL);
	if (newExp->length == exp->length) {
		RB.destroy(&newExp, NULL);
	} else {
		lr_clo_item *newCloItem = qmalloc(lr_clo_item);
		newCloItem->item = old->item;
		newCloItem->expect = insertExpect(analyzer, newExp);
		o->key = insertCloItem(analyzer, newCloItem);
	}
}
void serialRules(qbytes *bytes, LRAnalyzer *analyzer) {
	qvec rules = analyzer->rules;
	byte *ptr = Bytes_tail(bytes, byte);
	int nsymbol = analyzer->gen->symbols->length;
	checksize(bytes, 8);
	writeInt32(ptr, rules->length);
	writeInt32(ptr, nsymbol);
	bytes->length += 8;
	checksize(bytes, nsymbol * sizeof(int) * rules->length);
	ptr = Bytes_tail(bytes, byte);
	for (int i = 0; i < rules->length; i++) {
		writeBytes(ptr, rules->data[i].p, nsymbol * sizeof(int));
	}
	bytes->length += nsymbol * sizeof(int) * rules->length;
}
int deserialRules(byte *bytes, intptr_t *i) {
	byte *ptr = bytes;
	int nrule = readInt32(ptr);
	int nsymbol = readInt32(ptr);
	qvec rules = Arr.create(nrule);
	for (int i = 0; i < nrule; i++) {
		int *rule = qmalloc2(int, nsymbol);
		readBytes(rule, ptr, nsymbol * sizeof(int));
		Arr.append(rules, rule);
	}
	*i = rules;
	return ptr - bytes;
}
void serialAnalyzer(LRAnalyzer *analyzer, char *path) {
	FILE *fp = fopen(path, "w");
	qbytes *bytes = Bytes.create(1);
	typeList->serialize(bytes, analyzer->gen->grams);
	fwrite(bytes->data, 1, bytes->length, fp);

	bytes->length = 0;
	typeRBTree->serialize(bytes, analyzer->gen->nterminals);
	fwrite(bytes->data, 1, bytes->length, fp);

	bytes->length = 0;
	typeRBTree->serialize(bytes, analyzer->gen->terminals);
	fwrite(bytes->data, 1, bytes->length, fp);

	bytes->length = 0;
	serialRules(bytes, analyzer);
	fwrite(bytes->data, 1, bytes->length, fp);
	fclose(fp);
	Bytes.destroy(&bytes);
}
LRAnalyzer *deserialAnalyzer(char *path) {
	createTypeGram();
	LRAnalyzer *analyzer = qcalloc(LRAnalyzer);
	analyzer->gen = qcalloc(qgen);
	byte *bytes = file_read(path);
	byte *ptr = bytes;
	intptr_t i;

	ptr += typeList->deserial(ptr, &i);
	analyzer->gen->grams = i;
	ptr += typeRBTree->deserial(ptr, &i);
	RBTree *nterminals = analyzer->gen->nterminals = i;
	ptr += typeRBTree->deserial(ptr, &i);
	RBTree *terminals = analyzer->gen->terminals = i;
	ptr += deserialRules(ptr, &i);
	analyzer->rules = i;
	int nsymbol = nterminals->length + terminals->length;
	qvector *symbols = Arr.create(nsymbol);
	RBNode *node = RB.min(terminals);
	while (node) {
		qstr *s = cast(qstr*, node->val);
		s->info = node->key | TFLAG;
		symbols->data[node->key].p = s;
		node = RB.next(node);
	}
	node = RB.min(nterminals);
	while (node) {
		symbols->data[node->key].p = node->val;
		node = RB.next(node);
	}
	symbols->length = nsymbol;
	analyzer->gen->symbols = symbols;
	analyzer->tableFun = RB.create(NULL, NULL);

	return analyzer;
}
/**
 * 返回值：
 * 	true,expect如果必要需要添加默认值
 */
bool getExpect(LRAnalyzer *analyzer, lr_gram *grammar, int at, RBTree **exp_ptr) {
	RBTree *expect = RB.create(NULL, NULL);
	int i;
	for (i = at; i < grammar->right->length; i++) {
		WordID idx = grammar->right->data[i].i;
		if (idx != WI_NIL && idx & TFLAG) {
			RB.insert(expect, idx, NULL, NULL);
			break;
		} else {
			RB.insertAll(expect, FIRST(analyzer, idx), NULL, NULL);
			if (!NUL(analyzer, idx)) {
				break;
			}
		}
	}
	*exp_ptr = expect;
	return i == grammar->right->length;
}
RBTree *insertExpect(LRAnalyzer *analyzer, RBTree *expect) {
	RBNode *node;
	if (RB.insert(analyzer->all_expect, expect, analyzer->all_expect->length,
			&node) == false) {
		if (expect != node->key) {
			RB.destroy(&expect, NULL);
			expect = cast(RBTree*, node->key);
		}
	}
	return expect;
}
RBTree *insertCloItem(LRAnalyzer *analyzer, lr_clo_item *cloItem) {
	RBNode *node;
	if (RB.insert(analyzer->all_cloItem, cloItem, NULL, &node) == false) {
		if (cloItem != node->key) {
			qfree(cloItem);
			cloItem = node->key;
		}
	}
	skyc_assert_(cloItem->item->gram);
	return cloItem;
}
//id的约束在最后，则默认为lastExp
RBTree *countClosure(LRAnalyzer *analyzer, RBTree *deps, TypeWord id,
		RBTree *preExpDef, RBTree *lastExp) {
	if (RB.insert(deps, id, NULL, NULL) == false) {
		skyc_error("Can't depend on each other!");
	}
//	if(id==TW_test){
//		printf("TW_and_test\n");
//	}
	/**
	 * extraExp的值应当不受lastExp的影响，idItems为at=0的cloItem
	 */
	RBTree *extraExp = NULL, /**extraExp2 = NULL,*/*expect, *expect2, *expDef,
			*defClos, *t = NULL, *expTable;
	if (analyzer->tableDefExp[id]) {
		expDef = analyzer->tableDefExp[id];
		expTable = analyzer->tableExp[id];
		extraExp = RB.search(expTable, id)->val;
		defClos = analyzer->tableDefClo[id];
	} else {
		expTable = RB.create(NULL, NULL); //id
		expDef = RB.create(NULL, NULL); //idx
		defClos = RB.create(type_cloItem, NULL);
//		defClos = RB.create(NULL, NULL);
		analyzer->tableDefExp[id] = expDef;
		analyzer->tableExp[id] = expTable;
		analyzer->tableDefClo[id] = defClos;
		RBTree *t2 = analyzer->tableGram[id];
		RBNode *node = RB.min(t2), *node2;
		while (node) {
			lr_gram* grammar = cast(lr_gram*, node->key);
			WordID index = grammar->right->data[0].i;
			if (index & TFLAG) {
				goto next;
			}
			if (getExpect(analyzer, grammar, 1, &expect)) {
				RB.insert(expDef, index, NULL, NULL);
				if (preExpDef && RB.search(preExpDef, id)) {
					RB.insert(preExpDef, index, NULL, NULL); //只能传递一级
				}
				//RB.insertAll(expect, lastExp, NULL, NULL); //能达到末尾则插入默认的
				//这里不插入，用于保存，使得可复用
			}

			if (RB.insert(expTable, index, expect, &node2) == false) {
				RBTree *oldExp = cast(RBTree*, node2->val);
				RB.insertAll(oldExp, expect, NULL, NULL);
				RB.destroy(&expect, NULL);
			}
			next: node = RB.next(node);
		}	//node<-analyzer->tableGram[id]
		if ((node = RB.search(expTable, id)) != NULL) {
			extraExp = cast(RBTree*, node->val);
//			RB.delete(expTable, id);
		} else {
			extraExp = RB.create(NULL, NULL);
			RB.insert(expTable, id, extraExp, NULL);
		}
	}			//analyzer->tableDefExp[id]

	//---------------------
	if (expTable->length > 1) {
		RBNode *node = RB.min(expTable);
		while (node) {
			TypeWord id2 = node->key;
			//		sky_assert(id != id2);
			if (RB.search(deps, id2)) {
				//			sky_assert(RB.search(defClos,id2)==NULL);
				sky_assert(deps);
				node = RB.next(node);
			} else {
				/**
				 * 如果id2不在expDef中，是可以计算出来的
				 * 如果id不再prevExpDef中，就算在expDep中，跟前面的没有关系
				 */

				expect = cast(RBTree*, node->val);
				expect = insertExpect(analyzer, expect);
				RBTree *t2 = countClosure(analyzer, deps, id2, expDef, expect);
				RB.insertAll(defClos, t2, NULL, closureInsertFail);
				//			RB.insert(defClos, id2, t2, NULL);
				//			RBTree *t2 = countClosure(analyzer, deps, id2, expect);
				RBNode *node2 = RB.min(t2);
				while (node2) {
					lr_clo_item* cloItem = cast(lr_clo_item*, node2->key);
					lr_gram* gram = cloItem->item->gram;
					if (gram->right->data[0].int32 == id) {
						if (getExpect(analyzer, gram, 1, &expect2)) {
							RB.insertAll(expect2, cloItem->expect, NULL, NULL);
						}
						RB.insertAll(extraExp, expect2, NULL, NULL);
						RB.destroy(&expect2, NULL);
					}
					node2 = RB.next(node2);
				}
				RB.destroy(&t2, NULL);			//会内存泄露吧
				RBNode *node2del = node;
				node = RB.next(node2del);
				RB.delNode(expTable, node2del);
			}
		}
		//---------------------
	}

	if (preExpDef && RB.search(preExpDef, id)) {
		RB.insertAll(preExpDef, expDef, NULL, NULL);
	}

	skyc_assert_(lastExp);
	//---------------------
	RBTree *totalExp;
	RBTree *idItems = analyzer->tableItems[id];
	t = RB.create(type_cloItem, NULL);
	if (extraExp->length) {
		totalExp = RB.create(NULL, NULL);
		RB.insertAll(totalExp, lastExp, NULL, NULL);
		RB.insertAll(totalExp, extraExp, NULL, NULL);
		totalExp = insertExpect(analyzer, totalExp);
	} else
		totalExp = lastExp;
	RBNode *node = RB.min(idItems);
	while (node) {
		lr_item *mItem = cast(lr_item*, node->key);
//			sky_assert(mItem->gram->left == id);
//			if (extraExp && mItem->gram->right->data[0].int32 != WI_NIL) {
//				used = true;
//				expect = extraExp;
//			} else
//				expect = lastExp;
		lr_clo_item* cloItem = qmalloc(lr_clo_item);
		cloItem->item = mItem;
		cloItem->expect = totalExp;
		cloItem = insertCloItem(analyzer, cloItem);
		//item不可能相同，所以是不可能插入失败的
		sky_assert(RB.insert(t,cloItem,analyzer,NULL));
		node = RB.next(node);
	}
	node = RB.min(defClos);
	while (node) {
//		while (node2) {			//有个疑问，extra是id独有,还是expDef中都可有
		lr_clo_item* cloItem = node->key;
		TypeWord id2 = cloItem->item->gram->left;
		if (RB.search(expDef, id2)) {
//					expect = cloItem->expect;
			RBTree *newExp = RB.create(NULL, NULL);
			RB.insertAll(newExp, cloItem->expect, NULL, NULL);
			RB.insertAll(newExp, totalExp, NULL, NULL);
			newExp = insertExpect(analyzer, newExp);
			lr_clo_item* cloItem2 = qmalloc(lr_clo_item);
			cloItem2->item = cloItem->item;
			cloItem2->expect = newExp;
			cloItem = insertCloItem(analyzer, cloItem2);
		}
//					skyc_assert_(cloItem2->item->gram);
		RBNode *insertNode;
		if (RB.insert(t, cloItem, analyzer, &insertNode) == false) {
			RBNode tempNode = { cloItem, cloItem->expect };
			closureInsertFail(insertNode, &tempNode);
		}
		node = RB.next(node);
	}			//RB.min(defClos);
	RB.delete(deps, id);
	return t;
}

/**
 * 	递进 expect是不变化的
 */
void GO(LRAnalyzer *analyzer, RBTree *goTable, WordID idx,
		lr_clo_item *clo_item) {
	lr_item itemTemp;
	RBTree *cloItems;
	lr_closure* closure;
	sky_assert(idx != WI_NIL);
	RBNode* val = RB.search(goTable, idx);
	if (val == NULL) {
		cloItems = RB.create(type_cloItem, NULL);
		closure = qmalloc(lr_closure);
//		closure->index = closures.length;
		closure->clo_items = cloItems;
		sky_assert(RB.insert(goTable, idx, closure, NULL));
	} else {
		closure = cast(lr_closure*, val->val);
		cloItems = closure->clo_items;
	}
//	rules[id & IDMASK] = closure->index;
	itemTemp.at = clo_item->item->at + 1;
	itemTemp.gram = clo_item->item->gram;
	val = RB.search(analyzer->all_item, &itemTemp);
	lr_clo_item *clo_item2 = qmalloc(lr_clo_item);
	clo_item2->expect = clo_item->expect;
	clo_item2->item = cast(lr_item*, val->key);
	if (RB.insert(cloItems, clo_item2, NULL, &val) == false) { //不用判断，clo_item是唯一的,不用考虑expect
		qfree(clo_item2);
		skyc_error("GO ERROR!");
	}
}
/**
 * idx 终结符含标志位,可能包含的非终结符中含有NUL
 */
bool NUL(LRAnalyzer *analyzer, WordID idx) {
	TypeWord id = idx & IDMASK;
	if (analyzer->tableNul[id])
		return analyzer->tableNul[id] == 1 ? true : false;
	if (idx & TFLAG) {
		analyzer->tableNul[id] = 2;
		return false;
	}
	RBTree *deps = RB.create(NULL, NULL);
	bool res = hasNul(analyzer, deps, idx);
	RB.destroy(&deps, NULL);
	return res;
}
static bool hasNul(LRAnalyzer *analyzer, RBTree *deps, WordID idx) {
	byte *tableNul = analyzer->tableNul;
	TypeWord id = idx & IDMASK;
	bool depFlag;
	if (tableNul[id])
		return tableNul[id] == 1 ? true : false;
	if (idx & TFLAG) {
		tableNul[id] = 2;
		return false;
	}
	if (RB.insert(deps, id, NULL, NULL) == false) {
		sky_error("can't not depend on each other!", 0);
	}
	RBIter *iter = RB.getIter(analyzer->tableGram[id]);
	while (RB.iterNext(iter)) {
		lr_gram *gram = cast(lr_gram*, iter->cur->key);
		int i = 0;
		depFlag = false;
		for (; i < gram->right->length; i++) {
			WordID idx2 = gram->right->data[i].int32;
			if (idx2 == WI_NIL)
				continue;
			if (idx2 & TFLAG)
				break;
			else if (idx2 == id)
				break;
			else if (RB.search(deps, idx2)) {
				if (tableNul[idx2]) {
					if (tableNul[idx2] == 2)
						break;
				} else
					depFlag = true;
			} else if (hasNul(analyzer, deps, idx2) == false) {
				break;
			}
		}
		if (i == gram->right->length) {
			if (depFlag == false) {
				tableNul[id] = 1;
				break;
			} else
				tableNul[id] = 3;
		}
	}
	RB.releaseIter(iter);
	RB.delete(deps, id);
	if (tableNul[id] == 0)
		tableNul[id] = 2;
	else if (tableNul[id] == 3)
		tableNul[id] = 0;
	return tableNul[id] == 1 ? true : false;
}
void destroyGramdesc(int expid, qgramdesc *desc) {
	if (desc->u.alltks)
		Arr.destroy(&desc->u.alltks, NULL);
	skym_alloc_pool(_S, desc, sizeof(qgramdesc), 0);
}
bool analyse(LRAnalyzer *analyzer, qlexer *lex) {
	qbytes *tokens = lex->tkcache/*, *gramdescs = Bytes.create(sizeof(qgramdesc))*/;
	RBTree *exprs = RB.create(NULL, NULL);
	qgen *gen = analyzer->gen;
//stackStatus保存状态，stackInfo保存已进栈的终结符或非终结符的id，tokenInfo保存规约式
	qvec stackStatus = Arr.create(32), stackInfo = Arr.create(32), tokenInfo =
			Arr.create(16);
	int **rules = cast(int**, analyzer->rules->data); //32位系统运行会出错
	qvec grams = gen->grams, exp;
	bool isAccept = false;
	RBNode *funcNode; //, *node;
	RuleInfo ruleinfo;
	int expIdx = -1, top = 0, ncache;
	TypeWord type;
	Arr.push(stackStatus, 0);
	ruleinfo.tokens = tokens;
	ruleinfo.exprs = exprs;
	qgramdesc *desc = NULL;
	while (ncache = gen_tokens(lex, 3)) {
		while (top < tokens->length) {
			qtk *token = Bytes_get(tokens, top, qtk);
			type = token->type;
			int action = rules[arr_tail(stackStatus).i][type];
			if (action & RFLAG) {
				action &= RMASK;
				lr_gram *gram = cast(lr_gram *, grams->data[action].p);
				int gramlen = gram->right->length;
				funcNode = RB.search(analyzer->tableFun, gram->nval);
//				exp = Arr.create(gramlen); ////exp保存qgramdesc【的位置】
				printf("expr %d(%d) | ", ++expIdx, gramlen);
				printGram(NULL, gen, gram);
				printf("\n");
				exp = NULL;
				int nterminal = 0;
//				CTRL.stopif(gram->left == TW_stats_DOT_0);
				if (gram->right->data[0].int32 != WI_NIL) {
					exp = Arr.create(gramlen);
					tokenInfo->length -= gramlen;
					qval *data = tokenInfo->data + tokenInfo->length;
					for (int i = 0; i < gramlen; i++) {
						int expid = data[i].i;
						if (expid & EXPFLAG) {
							RBNode *node = RB.search(exprs, expid);
							if (node == NULL)
								continue;
							qgramdesc *gramDesc = node->val;
							if (gramDesc->gram->left != TW_STAT)
								nterminal += gramDesc->nterminal;
							switch (gramDesc->flag) {
							case TD_GRAM: //可能含有TD_TOCALL或TD_CALLED
								if (gramDesc->u.alltks->length) {
									Arr.addFromVec(exp, gramDesc->u.alltks);
								}
								Arr.destroy(&gramDesc->u.alltks, ARR_NOT_FREE);
								skym_alloc_pool(_S, gramDesc, sizeof(qgramdesc), 0);
								RB.delete(exprs, expid);
								break;
							case TD_KEEP:
							case TD_TOCALL:
							case TD_CALLED:
								Arr.append(exp, expid);
								break;
							default:
								skyc_error("gramDesc->flag");
								break;
							}
						} else if (expid != NILFLAG) {
							nterminal++;
							Arr.append(exp, expid);
						}
					}
					if (exp->length == 0) {
						Arr.destroy(&exp, NULL);
					}
				} else { //do nothing
				}
				if (funcNode) {
					RFunc *rf = funcNode->val;
					if (rf->callType & C_ACCELERABLE
							&& (exp == NULL || exp->length <= 1)) {
						if (exp) {
							Arr.append(tokenInfo, exp->data[0].i);
							Arr.destroy(&exp, NULL);
						} else {
							Arr.append(tokenInfo, NILFLAG);
						}
						printf("accelerable...\n");
					} else if (exp == NULL && rf->callType & C_ATONCE) {
						Arr.append(tokenInfo, NILFLAG);
						ruleinfo.desc = NULL;
						rf->func(analyzer->ctx, &ruleinfo);
					} else {
						qgramdesc *preCalled = NULL;
						for (int i = 0; i < exp->length; i++) {
							int expid = exp->data[i].i;
							if (expid & EXPFLAG) {
								qgramdesc *gramdesc = RB.search(exprs, expid)->val;
								if (gramdesc->flag == TD_TOCALL) {
									ruleinfo.parent = gram->nval;
									ruleinfo.preCalled = preCalled;
									ruleinfo.desc = gramdesc;
									ruleinfo.pos = i;
									ruleinfo.exp = exp;
									RFunc *regfun = gramdesc->data;
									regfun->func(analyzer->ctx, &ruleinfo);
									if (gramdesc->flag == TD_TOCALL)
										gramdesc->flag = TD_CALLED;
									preCalled = gramdesc;
								}
							}
						}
						if (rf->callType & C_MERGEABLE && exp->length <= 1) {
							desc = NULL;
							if (exp->length == 1) {
								Arr.append(tokenInfo, exp->data[0].i);
								Arr.destroy(&exp, NULL);
							} else {
								Arr.append(tokenInfo, NILFLAG);
							}
							printf("mergeable...\n");
						} else {
							desc = skym_alloc_pool(_S, NULL, NULL, sizeof(qgramdesc)); //在这里值被改变
							RB.insert(exprs, expIdx | EXPFLAG, desc, NULL);
							Arr.append(tokenInfo, expIdx | EXPFLAG);
							desc->u.alltks = exp;
							desc->nterminal = nterminal;
							desc->gram = gram;
						}
						if (rf->callType & C_ATONCE) {
							ruleinfo.desc = desc;
							ruleinfo.pos = top;
							rf->func(analyzer->ctx, &ruleinfo);
							if (desc->flag == TD_TOCALL)
								desc->flag = TD_CALLED;
						} else if (rf->callType & C_NEVER) {
							if (desc)
								desc->flag = TD_CALLED;
						} else {
							desc->data = rf;
//							desc->flag = TD_TOCALL;
						}
					} //expTokens->length != 1
				} else {	//funcNode
					if (exp == NULL) {
						Arr.append(tokenInfo, NILFLAG);
					} else if (exp->length == 1) {
						Arr.append(tokenInfo, exp->data[0].i);
						Arr.destroy(&exp, NULL);
					} else {
						skyc_assert_(exp->length > 1);
						qgramdesc *desc = skym_alloc_pool(_S, NULL, NULL,
								sizeof(qgramdesc)); //在这里值被改变
						RB.insert(exprs, expIdx | EXPFLAG, desc, NULL);
						desc->flag = TD_GRAM;
						desc->gram = gram;
						desc->nterminal = nterminal;
						desc->u.alltks = exp;
						Arr.append(tokenInfo, expIdx | EXPFLAG);
					}
				}
				if (gram->right->data[0].int32 != WI_NIL) {
					stackStatus->length -= gram->right->length;
					stackInfo->length -= gram->right->length;
				}
				action = rules[arr_tail(stackStatus).i][gram->left];
				Arr.push(stackStatus, action);
				Arr.push(stackInfo, cast(int, gram->left));
				if (gram->left == TW_START) {
					printf("accept!\n");
					isAccept = true;
					goto end;
				} else if (gram->left == TW_STAT || gram->left == TW_methdef) {
					if (analyzer->statHook)
						analyzer->statHook(analyzer, analyzer->ctx);
					int nummove = tokens->length - top;
					top -= nterminal;
					tokens->length = tokens->length - nterminal;
					memcpy(Bytes_get(tokens, top, void),
							Bytes_get(tokens, top + nterminal, void),
							tokens->datasize * (nummove));
					RBNode *node = RB.min(exprs);
					while (node) {
						RBNode *temp = node;
						node = RB.next(node);
						desc = temp->val;
						if (desc->flag != TD_GRAM && desc->flag != TD_KEEP) {
							RB.delNode(exprs, temp);
							if (desc->u.alltks)
								Arr.destroy(&desc->u.alltks, NULL);
							skym_alloc_pool(_S, desc, sizeof(qgramdesc), 0);
						}
					}
					printf("destroy redundant %d tokens!\n", nterminal);
				}
			} else if (action == 0) {
				printfExpect(gen, rules[arr_tail(stackStatus).i]);
				printf("but %s\n",gsymbol(gen,type));
				goto end;
			} else {
				Arr.push(stackInfo,cast(void*,type));
				Arr.push(stackStatus,action);
				Arr.push(tokenInfo,top);
				top++;
			}
			printStackInfo(gen, stackStatus, stackInfo);
			printf(" | ");
			printfToken(gen, tokens, top);
			printf("\n");
		}
	}
	end: {
		Arr.destroy(&stackInfo, ARR_NOT_FREE);
		Arr.destroy(&stackStatus, ARR_NOT_FREE);
		Arr.destroy(&tokenInfo, ARR_NOT_FREE);
		RB.destroy(&exprs, destroyGramdesc);
		return isAccept;
	}
}
void LR1(LRAnalyzer *analyzer) {
	char str1[] = "规约：", str2[] = "ε规约：", strTemp[64];
	lr_item itemTemp;
	qgen *gen = analyzer->gen;
	qlogger *logger = analyzer->logger;
	RBTree *items = analyzer->tableItems[TW_START];
	RBTree* clo = RB.create(type_cloItem, NULL);
	RBTree* expect = RB.create(NULL, NULL);
	RB.insert(expect, TW_END, NULL, NULL);
	RB.insert(analyzer->all_expect, expect, analyzer->all_expect->length, NULL);
	RBIter *it = RB.getIter(items);
	qvec goIdx = Arr.create(0), goCloItem = Arr.create(0);
	while (RB.iterNext(it)) {
		lr_item *item = (lr_item *) it->cur->key;
		if (item->gram->left == TW_START) {
			lr_clo_item *clo_item = qmalloc(lr_clo_item); //非必须，start的不会被改动
			clo_item->item = item;
			clo_item->expect = expect;
			RB.insert(clo, clo_item, NULL, NULL);
			sky_assert(RB.insert(analyzer->all_cloItem,clo_item,NULL,NULL));
		}
	}
	RB.releaseIter(it);
	lr_closure* closure = qmalloc(lr_closure);
	closure->clo_items = clo;
	closure->index = 0;
	CLOSURE(analyzer, clo);
	qvec closures = analyzer->closures;
	RB.insert(analyzer->all_closure, closure, 0, NULL);
	Arr.append(closures, closure);
	RBTree *goTable = RB.create(NULL, NULL);
	for (int i = 0; i < closures->length; i++) {
		goIdx->length = goCloItem->length = 0;
		closure = cast(lr_closure*, closures->data[i].p);
//		if (i == 159)
//				printClosure(analyzer, closure);
		sky_assert(closure->index == i);
		clo = closure->clo_items;
		printClosure(analyzer, closure);
		int *rules = qcalloc2(int, gen->symbols->length);
		Arr.append(analyzer->rules, rules);
		RBNode *node = RB.min(clo);
		while (node) {
			lr_clo_item *clo_item = cast(lr_clo_item *, node->key);
			lr_gram *gram = clo_item->item->gram;
			if (clo_item->item->at == gram->right->length) { //规约
				RBNode *exp = RB.min(clo_item->expect);
				Log.add(logger, str1, sizeof(str1) - 1);
				printItem(&logger->buf, gen, clo_item->item);
				printExpect(&logger->buf, analyzer, clo_item->expect);
				Log.add(logger, "\n", 1);
				while (exp) {
					skyc_assert_(rules[exp->key & IDMASK]==0);
					rules[exp->key & IDMASK] = gram->nval | RFLAG;
					exp = RB.next(exp);
				}
			} else {
				WordID idx = gram->right->data[clo_item->item->at].i; //一般ID_NUL在尾端
				if (idx == WI_NIL) {
					sky_assert(gram->right->length == 1 && clo_item->item->at == 0);
					RBNode *expIter = RB.min(clo_item->expect);
					Log.add(logger, str2, sizeof(str2) - 1);
					itemTemp.at = 1;
					itemTemp.gram = gram;
					printItem(&logger->buf, gen, &itemTemp);
					printExpect(&logger->buf, analyzer, clo_item->expect);
					Log.add(logger, "\n", 1);
					while (expIter) {
						skyc_assert_(rules[expIter->key & IDMASK]==0);
						rules[expIter->key & IDMASK] = gram->nval | RFLAG;
						expIter = RB.next(expIter);
					}
				} else {
					Arr.append(goIdx, cast(int, idx));
					Arr.append(goCloItem, clo_item);
//					GO(goTable, idx, clo_item);
				}
			}
			node = RB.next(node);
		} //RB.min(clo)
		for (int k = 0; k < goIdx->length; k++) {
			WordID idx = goIdx->data[k].int32;
			TypeWord id = idx & IDMASK;
			if (rules[id] == 0) { //(rules[id] & RFLAG) == 0
				lr_clo_item *clo_item = cast(lr_clo_item *, goCloItem->data[k].p);
				GO(analyzer, goTable, idx, clo_item);
			}
		}
		node = RB.min(goTable);
		RBNode *node2;
		lr_closure* closure;
		while (node) {
			WordID idx = cast(int, node->key);
			TypeWord id = idx & IDMASK;
			closure = cast(lr_closure*, node->val);
			CLOSURE(analyzer, closure->clo_items);
			if (RB.insert(analyzer->all_closure, closure, closures->length, &node2)) {
				closure->index = closures->length;
				Arr.append(closures, closure);
			} else {
				RB.destroy(&closure->clo_items, NULL);
				qfree(closure);
				closure = cast(lr_closure*, node2->key);
			}
			if ((rules[id] & RFLAG)) {
				sky_assert(0);
			}
			rules[id] = closure->index;
			if (idx & TFLAG && id > TW_NIL)
				Log.add(logger, strTemp,
						sprintf(strTemp, "—— '%s' ——> closure %d\n", gsymbol(gen, id),
								closure->index));
			else
				Log.add(logger, strTemp,
						sprintf(strTemp, "—— %s ——> closure %d\n", gsymbol(gen, id),
								closure->index));
			node = RB.next(node);
		}
		RB.clear(goTable, NULL);
	}
	Arr.destroy(&goIdx, ARR_NOT_FREE);
	Arr.destroy(&goCloItem, ARR_NOT_FREE);
	Log.destroy(analyzer->logger);
	RB.destroy(&goTable, RB_NOT_FREE);
}
//扩充closure域
void CLOSURE(LRAnalyzer *analyzer, RBTree* clos) {
//	qlist queue = List.create(0), queueExpect = List.create(0);
	RBTree *addclos = RB.create(type_cloItem, NULL), *deps = RB.create(NULL,
	NULL);
	RBNode *node = RB.min(clos);
	while (node != NULL) {
		lr_clo_item *cloItem = (lr_clo_item *) node->key;
		lr_item *item = cloItem->item;
		if (item->at == item->gram->right->length)
			goto next;
		WordID idx = item->gram->right->data[item->at].i;
		if (idx & TFLAG)
			goto next;
		lr_gram *grammar = item->gram;
		RBTree *expect;
		if (getExpect(analyzer, grammar, item->at + 1, &expect)) {
			RB.insertAll(expect, cloItem->expect, NULL, NULL);
		}
		expect = insertExpect(analyzer, expect);
		RBTree *extraClos = countClosure(analyzer, deps, idx, NULL, expect);
		skyc_assert_(deps->length == 0);
		RB.insertAll(addclos, extraClos, NULL, closureInsertFail);
//		RB.insertAll(addclos, extraClos, NULL, cloItemInsertFail);
		RB.destroy(&extraClos, NULL);
		next: node = RB.next(node);
	}
	RB.destroy(&deps, NULL);
	RB.insertAll(clos, addclos, NULL, closureInsertFail);
//	printClosure(clos);
	RB.destroy(&addclos, NULL);
}

/**
 * 切记函数返回值不要修改
 * 参数
 * 	idx为含标志位的index
 */
static const RBTree *getFirst(LRAnalyzer *analyzer, RBTree *deps, WordID idx) {
	TypeWord id = idx & IDMASK;
	RBTree *res = analyzer->tableFirst[id];
	if (res)
		return res;
	res = RB.create(NULL, NULL);
	analyzer->tableFirst[id] = res;
	if ((idx & TFLAG) /*|| (id <= TW_END)*/) {
		if (idx != WI_NIL) {
			RB.insert(res, idx, NULL, NULL);
		} else {
			printf("TW_NIL !\n");
		}
		return res;
	} else {
		if (RB.insert(deps, idx, NULL, NULL) == false) {
			return NULL;
		}
//		sky_assert(idx == id);
		RBNode *iter = RB.min(analyzer->tableGram[id]);
		while (iter != NULL) {
			lr_gram *gram = cast(lr_gram*, iter->key);
			for (int i = 0; i < gram->right->length; i++) {
				WordID index = gram->right->data[i].int32;
				if (index & TFLAG) {
					if (index != WI_NIL) {
						RB.insert(res, index, NULL, NULL);
						break;
					}
				} else {
					if (index != id) {
						RBTree *first = getFirst(analyzer, deps, index);
						RB.insertAll(res, first, NULL, NULL);
					}
					if (NUL(analyzer, index) == false)
						break;
				}
			}
			iter = RB.next(iter);
		}
		RB.delete(deps, idx);
		return res;
	}
}
void mergeExp(LRAnalyzer *analyzer, RBTree *exps, qvec res, qvec exp) {
	for (int i = 0; i < exp->length; i++) {
		int index = exp->data[i].int32;
		if (index & EXPFLAG) {
			RBNode *node = RB.search(exps, index & IDMASK);
			qvec exp2 = cast(qexpr*, node->val);
			RB.delNode(exps, node);
			mergeExp(analyzer, exps, res, exp2);
			Arr.destroy(&exp2, ARR_NOT_FREE);
		} /*else if (index & EXP2FLAG) {
		 RBNode *node = RB.search(exps, index & IDMASK);
		 Arr.append(res, node->val);
		 RB.delNode(exps, node);
		 } */else {
			if (index != WI_NIL)
				Arr.append(res, index);
		}
	}
}

LRAnalyzer *createLRAnalyzer(qgen *gen) {
//	typeItem=createType(NULL, cast(comparefun,cmpItem), NULL, NULL, NULL, NULL, NULL, V_NIL);
	LRAnalyzer *analyzer = qmalloc(LRAnalyzer);
	typeItem = typeCompare(cmpItem);
	type_cloItem = typeCompare(cmp_cloItem);
	type_cloItem2 = typeCompare(cmp_cloItemDeep);
	type_Closure = typeCompare(cmpClosure);
	typeExpect = typeCompare(cmpExpect);
	typeExpect->free = freeExpect;
	analyzer->all_item = RB.create(typeItem, NULL);
	analyzer->all_expect = RB.create(typeExpect, NULL);
	analyzer->all_closure = RB.create(type_Closure, NULL);
	analyzer->all_cloItem = RB.create(type_cloItem2, NULL);
	analyzer->gen = gen;
	analyzer->rules = Arr.create(0);
	analyzer->closures = Arr.create(0);
	analyzer->tableFun = RB.create(NULL, NULL);
	analyzer->tableNul = qcalloc2(byte, gen->symbols->length);
	analyzer->tableNul[TW_NIL] = 1;
	analyzer->tableFirst = qcalloc2(RBTree*, gen->symbols->length);
	analyzer->tableDefExp = qcalloc2(RBTree*, gen->symbols->length);
	analyzer->tableDefClo = qcalloc2(RBTree*, gen->symbols->length);
	analyzer->tableExp = qcalloc2(RBTree*, gen->symbols->length);
	RBTree **tableItems = qcalloc2(RBTree*, gen->symbols->length);
	RBTree **tableDeps = qcalloc2(RBTree*, gen->symbols->length);
	RBTree **tableGram = qcalloc2(RBTree*, gen->symbols->length);
	analyzer->tableItems = tableItems;
	analyzer->tableDeps = tableDeps;
	analyzer->tableGram = tableGram;
	gen->tableGram = tableGram;
	lr_gram *grammar;
	RBNode *node = RB.min(gen->all_gram);
//	printf("\nsorted lr_grammar:\n");
	RBTree *t, *t2, *t3;
	while (node != NULL) {
		grammar = cast(lr_gram *, node->key);
		for (int i = 0; i <= grammar->right->length; i++) {
			lr_item* item = qmalloc(lr_item);
			item->gram = grammar;
			item->at = i;
			sky_assert(RB.insert(analyzer->all_item, item,NULL,NULL));
			if (i == 0) {
				t = tableItems[grammar->left];
				t2 = tableDeps[grammar->left];
				t3 = tableGram[grammar->left];
				if (t == NULL) {
					t = RB.create(typeItem, NULL);
					tableItems[grammar->left] = t;
					t2 = RB.create(NULL, NULL);
					tableDeps[grammar->left] = t2;
					t3 = RB.create(typeGram, NULL);
					tableGram[grammar->left] = t3;
				}
//				lr_clo_item *cloItem = qmalloc(lr_clo_item);
//				cloItem->item = item;
				RB.insert(t, item, NULL, NULL);
//				if (item->gram->right->data[0].i != WI_NIL)
				RB.insert(t2, item->gram->right->data[0].i, NULL, NULL);
				RB.insert(t3, item->gram, NULL, NULL);
			}
		}
		node = RB.next(node);
	}
	return analyzer;
}

void destroyAnalyzer(LRAnalyzer *analyzer) {
	RB.destroy(&analyzer->all_cloItem, RB_FREE_KEY | RB_FREE_FORCE);
	RB.destroy(&analyzer->all_closure, RB_FREE_KEY | RB_FREE_FORCE);
	RB.destroy(&analyzer->all_item, RB_FREE_KEY | RB_FREE_FORCE);
	RB.destroy(&analyzer->tableFun, RB_FREE_VAL | RB_FREE_FORCE);
	RB.destroy(&analyzer->all_expect, RB_FREE_KEY);
	int nsymbol = analyzer->gen->symbols->length;
	if (analyzer->tableDeps) {
		for (int i = 0; i < nsymbol; i++) {
			if (analyzer->tableDeps[i])
				RB.destroy(&analyzer->tableDeps[i], NULL);
			if (analyzer->tableFirst[i])
				RB.destroy(&analyzer->tableFirst[i], NULL);
			if (analyzer->tableGram[i])
				RB.destroy(&analyzer->tableGram[i], NULL);
			if (analyzer->tableItems[i])
				RB.destroy(&analyzer->tableItems[i], NULL);
			if (analyzer->tableDefClo[i])
				RB.destroy(&analyzer->tableDefClo[i], NULL);
			if (analyzer->tableExp[i])
				RB.destroy(&analyzer->tableExp[i], NULL);
			if (analyzer->tableDefExp[i])
				RB.destroy(&analyzer->tableDefExp[i], NULL);
		}
		qfree(analyzer->tableDeps);
		qfree(analyzer->tableFirst);
		qfree(analyzer->tableGram);
		qfree(analyzer->tableItems);
		qfree(analyzer->tableDefClo);
		qfree(analyzer->tableExp);
		qfree(analyzer->tableDefExp);

		qfree(analyzer->tableNul);
		Arr.destroy(&analyzer->closures, ARR_NOT_FREE);
	}
	Arr.destroy(&analyzer->rules, ARR_FORCE_FREE);
	destroyGen(analyzer->gen);
	qfree(analyzer);
	destroyTypeGram();
}
void gen_file(LRAnalyzer *analyzer, char *path) {
	char *prefix = "/*\n"
			"* this is generated by code\n"
			"*\n"
			"*  Author: WanQing<1109162935@qq.com>\n"
			"*/\n\n"
			"#ifndef INCLUDE_GRAMMAR__H_\n"
			"#define INCLUDE_GRAMMAR__H_\n\n"
			"typedef enum{\n"
			"  TS_NIL";
	char *suffix = "\n}TypeStat;\n\n"
			"#endif /* INCLUDE_GRAMMAR__H_ */";
	char *name = NULL;
	qstr *s;
	FILE *fp = fopen(path, "w");
	qvec funcs;
	fwrite(prefix, 1, strlen(prefix), fp);
	if (analyzer->funcs == NULL) {
		funcs = Arr.create(analyzer->tableFun->length + 1);
		funcs->length = funcs->capacity;
		analyzer->funcs = funcs;
		RBNode *node = RB.min(analyzer->tableFun);
		while (node) {
			RFunc *rf = node->val;
			funcs->data[rf->callType].p = node->key;
			node = RB.next(node);
		}
	} else {
		funcs = analyzer->funcs;
	}
	for (int i = 1; i < funcs->length; i++) {
		s = funcs->data[i].s;
		name = s->val;
		fwrite(",\n  ", 1, 4, fp);
		fwrite("TS_", 1, 3, fp);
		fwrite(name, 1, strlen(name), fp);
	}
	fwrite(suffix, 1, strlen(suffix), fp);
	fclose(fp);
}
