/*
 * qgrammar.c
 *
 *  Created on: Jun 20, 2019
 *  Author: WanQing<1109162935@qq.com>
 */
#include "qgrammar.h"
#include "bytelist.h"
#include "qvector.h"
#include "qio.h"
#include "qstring.h"
#include "qcontrol.h"
lr_gram* newGram(int id);
TypeWord getGenId(qgen *gen, TypeWord id);
void lr_rule_add(lr_gram *grammar, int id) {
	Arr.append(grammar->right, cast(void*, id));
}
void lr_gram_add(qgen *gen, lr_gram *grammar) {
	if (RB.insert(gen->all_gram, grammar, gen->grams->length, NULL)) {
		grammar->nval = gen->grams->length;
		Arr.append(gen->grams, grammar);
		Arr.shrink(grammar->right);
		printGram(NULL, gen, grammar);
		printf("\n");
	} else {
		printf("Please do not insert the same grammar!\n");
		qfree(grammar);
	}
}
int cmpGram(lr_gram *g1, lr_gram *g2) {
	int res = g1->left - g2->left;
	if (res)
		return res;
	if ((res = g1->right->length - g2->right->length) != 0)
		return res;
	for (int i = 0; i < g1->right->length; i++) {
		if ((res = g1->right->data[i].i - g2->right->data[i].i) != 0)
			return res;
	}
	return 0;
}
void parseGram(qgen *gen) {
	lr_gram *grammar;
	int num;
	next_word(gen);
	while (gtype(gen) == TAB_NEWNT) {
		TypeWord id = ginfo(gen);
		grammar = newGram(id);
		num = 0;
		char *bak = gen->ptr;
		next_word(gen);
		while (gtype(gen) != TAB_NEWNT && gtype(gen) && num < 2) {
			switch (gtype(gen)) {
			case TAB_GROUP:
				num++;
				lr_group(gen, TFLAG);
				break;
			case TAB_OR:
				lr_or(gen, TFLAG, 0);
				break;
			case TAB_OPTIONAL:
				num++;
				lr_optional(gen, TFLAG);
				break;
			case TAB_MULTI:
				num++;
				lr_multi(gen, TFLAG);
				break;
			default:
				num++;
				next_word(gen);
				break;
			}
		}
		gen->ptr = bak;
		next_word(gen);
		bool flag = false; //当num=1且为一般符号
		while (gtype(gen) != TAB_NEWNT && gtype(gen)) {
			switch (gtype(gen)) {
			case TAB_GROUP:
				if (num > 1) {
					lr_rule_add(grammar, lr_group(gen, getGenId(gen, id)));
					flag = false;
				} else {
					lr_rule_add(grammar, lr_group(gen, id));
				}
				break;
			case TAB_OR:
				if (num > 1) {
					int last = arr_pop(grammar->right, int);
					if (flag == true)
						lr_rule_add(grammar, lr_or(gen, getGenId(gen, id), last));
					else {
						lr_rule_add(grammar, lr_or(gen, last, last));
					}
				} else {
					flag = false;
					lr_or(gen, id, arr_pop(grammar->right, int));
				}
				break;
			case TAB_OPTIONAL:
				if (num > 1) {
					lr_rule_add(grammar, lr_optional(gen, getGenId(gen, id)));
					flag = false;
				} else
					lr_rule_add(grammar, lr_optional(gen, id));
				break;
			case TAB_MULTI:
				if (num > 1) {
					lr_rule_add(grammar, lr_multi(gen, getGenId(gen, id)));
					flag = false;
				} else
					lr_rule_add(grammar, lr_multi(gen, id));
				break;
			default:
				flag = true;
				lr_rule_add(grammar, ginfo(gen));
				next_word(gen);
				break;
			}
		}
		if (num > 1 || flag)
			lr_gram_add(gen, grammar);
		else
			qfree(grammar);
	}
	skyc_assert_(gtype(gen) == TAB_EOS);
}

int lr_or(qgen *gen, TypeWord id, WordID left) {
	int n = 1;
	lr_gram *grammar;
	int flag = !(id & TFLAG);
	if (flag) {
		if (id != left) {
			grammar = newGram(id);
			lr_rule_add(grammar, left);
			lr_gram_add(gen, grammar);
		}
	}
	start:
//	if (flag) {
//		grammar = newGram(id);
//	}
	next_word(gen);
	sky_assert(gtype(gen) != TAB_EOS);
	switch (gtype(gen)) {
	case TAB_GROUP:
		if (flag) {
			lr_group(gen, id);
		} else {
			lr_group(gen, TFLAG);
		}
		++n;
		break;
	case TAB_MULTI:
		if (flag) {
			lr_multi(gen, id);
		} else {
			lr_multi(gen, TFLAG);
		}
		++n;
		break;
	case TAB_OPTIONAL:
		if (flag) {
			lr_optional(gen, id);
		} else {
			lr_optional(gen, TFLAG);
		}
		++n;
		break;
	case TAB_OR:
	case TAB_NEWNT:
		skyc_error("| expect symbol but others!");
		break;
	default:
		if (flag) {
			grammar = newGram(id);
			lr_rule_add(grammar, ginfo(gen));
			lr_gram_add(gen, grammar);
		} else
			++n;
		next_word(gen);
		break;
	}
//	if (flag) {
//		if (grammar->right->length)
//		else
//			qfree(grammar);
//	}

	if (gtype(gen) == TAB_OR) {
		goto start;
	}
	return flag ? id : n;
}
TypeWord getGenId(qgen *gen, TypeWord id) {
	RBNode *node;
	int n = 0;
	if (RB.insert(gen->numGen, id, n, &node) == false) {
		n = ++node->val;
	}
	sprintf(gen->buf, NT_FORMAT, gsymbol(gen, id), n);
	return add_nt(gen, STR.get(gen->buf));
}
//name等于零，跳过不生成，并返回长度
int lr_group(qgen *gen, TypeWord id) {
	int n = 0, num;
	lr_gram *grammar;
	bool flag = !(id & TFLAG);
	if (flag) {
		grammar = newGram(id);
		char *ptr = gen->ptr;
		num = lr_group(gen, TFLAG);
		gen->ptr = ptr;
	}
	next_word(gen);
	bool isAdd = false;
	while ( gtype(gen) != TAB_GREND) {
		sky_assert(gtype(gen) != TAB_EOS);
		switch (gtype(gen)) {
		case TAB_GROUP:
			if (flag) {
				if (num > 1) {
					isAdd = false;
					lr_rule_add(grammar, lr_group(gen, getGenId(gen, id)));
				} else
					lr_rule_add(grammar, lr_group(gen, id));
			} else {
				lr_group(gen, TFLAG);
			}
			++n;
			break;
		case TAB_OR:
			if (flag) {
				int lastid = arr_pop(grammar->right, int);
				if (num > 1) {
					if (isAdd)
						lr_rule_add(grammar, lr_or(gen, getGenId(gen, id), lastid));
					else
						lr_rule_add(grammar, lr_or(gen, lastid, lastid));
				} else {
					isAdd = false;
					lr_or(gen, id, lastid);
				}
			} else {
				lr_or(gen, TFLAG, 0);
			}
			break;
		case TAB_OPTIONAL:
			if (flag) {
				if (num > 1) {
					isAdd = false;
					lr_rule_add(grammar, lr_optional(gen, getGenId(gen, id)));
				} else
					lr_rule_add(grammar, lr_optional(gen, id));
			} else {
				lr_optional(gen, TFLAG);
			}
			++n;
			break;
		case TAB_MULTI:
			if (flag) {
				if (num > 1) {
					isAdd = false;
					lr_rule_add(grammar, lr_multi(gen, getGenId(gen, id)));
				} else
					lr_rule_add(grammar, lr_multi(gen, id));
			} else {
				lr_multi(gen, TFLAG);
			}
			++n;
			break;
		case TAB_NEWNT:
			sky_error("expect ) but new notTerminal!", 0);
			break;
		default:
			if (flag) {
				lr_rule_add(grammar, ginfo(gen));
				isAdd = true;
			} else
				++n;
			next_word(gen);
			break;
		}
	}
	if (flag) {
		if (num > 1 || isAdd)
			lr_gram_add(gen, grammar);
		else
			qfree(grammar);
	}
	next_word(gen);
	return flag ? id : n;
}
lr_gram* newGram(int id) {
	lr_gram *gram = qcalloc(lr_gram);
	gram->left = id;
	gram->right = Arr.create(0);
	return gram;
}
/*int lr_common(qgen *gen, TypeWord id,tktype tabend) {
 int num;
 next_word(gen);
 int flag = !(id & TFLAG);
 while ( gtype(gen) != tabend) {
 sky_assert(gtype(gen) != TAB_EOS);
 switch (gtype(gen)) {
 case TAB_GROUP:
 if (flag) {
 if(num>1)
 lr_rule_add(grammar, lr_group(gen, getGenId(gen, id)));
 else
 lr_group(gen, id);
 } else {
 lr_group(gen, TFLAG);
 }
 ++n;
 break;
 case TAB_OR:
 if (flag) {
 if (num > 1) {
 lr_rule_add(grammar,
 lr_or(gen, getGenId(gen, id), arr_pop(grammar->right, int)));
 } else {
 lr_or(gen, id, arr_pop(grammar->right, int));
 }
 } else {
 lr_or(gen, TFLAG, 0);
 }
 break;
 case TAB_MULTI:
 if (flag) {
 lr_rule_add(grammar, lr_multi(gen, getGenId(gen, id)));
 } else {
 lr_multi(gen, TFLAG);
 }
 ++n;
 break;
 case TAB_OPTIONAL:
 if (flag) {
 lr_rule_add(grammar, lr_optional(gen, getGenId(gen, id)));
 } else {
 lr_optional(gen, TFLAG);
 }
 ++n;
 break;
 case TAB_NEWNT:
 sky_error("expect ) but new notTerminal!", 0);
 break;
 }

 }
 }*/

int lr_optional(qgen *gen, TypeWord id) {
	int n = 0, num;
	lr_gram *grammar;
	int flag = !(id & TFLAG);
	if (flag) {
		char *ptr = gen->ptr;
		num = lr_optional(gen, TFLAG);
		gen->ptr = ptr;
		grammar = newGram(id);
		lr_rule_add(grammar, WI_NIL);
		lr_gram_add(gen, grammar);
		grammar = newGram(id);
	}
	next_word(gen);
	bool isAdd = false;
	while (gtype(gen) != TAB_OPTEND) {
		sky_assert(gtype(gen) != TAB_EOS);
		switch (gtype(gen)) {
		case TAB_GROUP:
			if (flag) {
				if (num > 1) {
					isAdd = false;
					lr_rule_add(grammar, lr_group(gen, getGenId(gen, id)));
				} else
					lr_rule_add(grammar, lr_group(gen, id));
			} else {
				lr_group(gen, TFLAG);
			}
			++n;
			break;
		case TAB_OR:
			if (flag) {
				skyc_assert_(grammar->right->length);
				int lastid = arr_pop(grammar->right, int);
				if (num > 1) {
					if (isAdd)
						lr_rule_add(grammar, lr_or(gen, getGenId(gen, id), lastid));
					else
						lr_rule_add(grammar, lr_or(gen, lastid, lastid));
				} else {
					lr_or(gen, id, lastid);
					isAdd = false;
				}
			} else {
				lr_or(gen, TFLAG, 0);
			}
			break;
		case TAB_MULTI:
			if (flag) {
				if (num > 1) {
					isAdd = false;
					lr_rule_add(grammar, lr_multi(gen, getGenId(gen, id)));
				} else
					lr_rule_add(grammar, lr_multi(gen, id));
			} else {
				lr_multi(gen, TFLAG);
			}
			++n;
			break;
		case TAB_OPTIONAL:
			if (flag) {
				if (num == 1)
					skyc_error("[]不能在同一个式子中连续使用！");
				lr_rule_add(grammar, lr_optional(gen, getGenId(gen, id)));
			} else {
				lr_optional(gen, TFLAG);
			}
			++n;
			break;
		case TAB_NEWNT:
			skyc_error("parser error!");
			break;
		default:
			if (flag) {
				lr_rule_add(grammar, ginfo(gen));
				isAdd = true;
			} else
				++n;
			next_word(gen);
			break;
		}
	}
	if (flag) {
		if (isAdd || num > 1) {
			skyc_assert_(grammar->right->length);
			lr_gram_add(gen, grammar);
		} else {
			qfree(grammar);
		}
	}
	next_word(gen);
	return flag ? id : n;
}

int lr_multi(qgen *gen, TypeWord id) {
	int n = 0, num;
	lr_gram *grammar, *grammar2;
	int flag = !(id & TFLAG);
	if (flag) {
		char *ptr = gen->ptr;
		num = lr_multi(gen, TFLAG);
		gen->ptr = ptr;
		grammar = newGram(id);
		lr_rule_add(grammar, WI_NIL);
		lr_gram_add(gen, grammar);
		grammar = newGram(id);
		grammar2 = newGram(id);
		lr_rule_add(grammar2, id);
	}
	next_word(gen);
	bool genFlag = false;
	while (gtype(gen) != TAB_MULTIEND) {
		sky_assert(gtype(gen) != TAB_EOS);
		switch (gtype(gen)) {
		case TAB_GROUP:
			if (flag) {
				if (num > 1) {
					genFlag = false;
					lr_rule_add(grammar, lr_group(gen, getGenId(gen, id)));
				} else
					lr_rule_add(grammar, lr_group(gen, id));
			} else {
				lr_group(gen, TFLAG);
			}
			++n;
			break;
		case TAB_OR:
			if (flag) {
				int lastid = arr_pop(grammar->right, int);
				if (num > 1) {
					if (genFlag)
						lr_rule_add(grammar, lr_or(gen, getGenId(gen, id), lastid));
					else
						lr_rule_add(grammar, lr_or(gen, lastid, lastid));

				} else
					lr_rule_add(grammar, lr_or(gen, id, lastid));
			} else {
				lr_or(gen, TFLAG, 0);
			}
			break;
		case TAB_OPTIONAL:
			if (flag) {
				if (num == 1)
					skyc_error("{}和[]不能同时在一个式子中使用！");
				else {
					genFlag = false;
					lr_rule_add(grammar, lr_optional(gen, getGenId(gen, id)));
				}
			} else {
				lr_optional(gen, TFLAG);
			}
			++n;
			break;
		case TAB_MULTI:
			if (flag) {
				if (num == 1)
					skyc_error("{}不能同时在一个式子中连续使用，这没有意义！");
				else {
					genFlag = false;
					lr_rule_add(grammar, lr_multi(gen, getGenId(gen, id)));
				}
			} else {
				lr_multi(gen, TFLAG);
			}
			++n;
		case TAB_NEWNT:
			sky_error("parser error!", 0);
			break;
		default:
			if (flag) {
				genFlag = true;
				lr_rule_add(grammar, ginfo(gen));
			} else
				++n;
			next_word(gen);
			break;
		}
	}
	if (flag) {
//		lr_rule_add(grammar, grammar->left);
		lr_gram_add(gen, grammar);
		Arr.addFromVec(grammar2->right, grammar->right);
		lr_gram_add(gen, grammar2);
	}
	next_word(gen);
	return flag ? id : n;
}
/**
 * 如果想保存nval的信息，必须在之前调用genGramFile函数
 */
void serialGram(qbytes* bytes, lr_gram *gram) {
	checksize(bytes, 8);
	byte *ptr = Bytes_tail(bytes, byte);
	writeInt32(ptr, gram->left);
	writeInt32(ptr, gram->nval);
	bytes->length += 8;
	typeList->serialize(bytes, gram->right);
}
int deserialGram(byte* l, intptr_t *i) {
	byte *ol = l;
	lr_gram *gram = qmalloc(lr_gram);
	gram->left = readInt32(l);
	gram->nval = readInt32(l);
	l += typeList->deserial(l, i);
	gram->right = *i;
	*i = gram;
	return l - ol;
}
void freeGram(lr_gram *gram) {
	Arr.destroy(&gram->right, ARR_NOT_FREE);
	qfree(gram);
}
Type typeGram = NULL;
void createTypeGram() {
	if (typeGram == NULL)
	typeGram = createType("<grammar>", cmpGram, NULL, serialGram, deserialGram,
			NULL, freeGram, V_NIL);
}

void initParser(qgen *gen) {
	createTypeGram();
	if (gen) {
		gen->all_gram = RB.create(typeGram, NULL);
		gen->grams = Arr.create(0);
		gen->grams->type = typeGram;
	}
}
void genGramFile(qgen *gen, char *path) {
	char *prefix = "/*\n"
			" *\n"
			" *  this is generated by code.\n"
			" *  If you want to modify, please modify the source code.\n"
			" *  \n"
			" *  Author: WanQing<1109162935@qq.com>\n"
			" */\n\n"
			"#ifndef INCLUDE_TYPEGRAM_H_\n"
			"#define INCLUDE_TYPEGRAM_H_\n\n"
			"typedef enum {";
	char *suffix = "\n} TypeGram;\n\n"
			"#endif /* INCLUDE_TYPEGRAM_H_ */\n\n";
	qstrbuf buf = { 0 };
//	int sum = 0;
	char temp[10];
	FILE *fp = fopen(path, "w");
	fwrite(prefix, 1, strlen(prefix), fp);
	int nsymbol = gen->symbols->length;
	RBTree **tableGram = gen->tableGram;
	qvec grams = gen->grams;
	int len = grams->length;
	byte *tableWord = qcalloc2(byte, nsymbol);
	STR.add(&buf, "\n  TG_", 6);
	for (int i = 0; i < len; i++) {
		lr_gram *gram = arr_data(grams, lr_gram*, i);
		skyc_assert_(gram->nval == i);
		TypeWord id = gram->left & IDMASK;
		RBTree *t = tableGram[id];
		qstr *name = gsymStr(gen, gram->left);
		buf.n = 6;
		STR.sub(&buf, name->val, ".", "_D");
		if (t->length > 1) {
			int size = sprintf(temp, "_%d", tableWord[id]++);
			STR.add(&buf, temp, size);
		}
		STR.add(&buf, ",		// ", 6);
		printGram(&buf, gen, gram);
		fwrite(buf.val, 1, buf.n, fp);
	}
	fwrite(suffix, 1, strlen(suffix), fp);
	fwrite("/*\n", 1, 3, fp);
	buf.n = 0;
	STR.add(&buf, " * ", 3);
	for (int i = 0; i < nsymbol; i++) {
		RBTree *t = tableGram[i];
		if (t) {
			RBNode *iter = RB.min(t);
			while (iter) {
				lr_gram *gram = cast(lr_gram*, iter->key);
				buf.n = 3;
				printGram(&buf, gen, gram);
				STR.add(&buf, "\n", 1);
				fwrite(buf.val, 1, buf.n, fp);
				buf.n = 0;
				iter = RB.next(iter);
			}
		}
	}
	fwrite(" */", 1, 3, fp);
	qfree(tableWord);
	qfree(buf.val);
	fclose(fp);
}
