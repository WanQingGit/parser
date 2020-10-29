/*
 * qtokenizer.c
 *
 *  Created on: May 29, 2019
 *  Author: WanQing<1109162935@qq.com>
 */
#include <qstring.h>
#include "qtokenizer.h"
#include "qcontrol.h"
#include "qstrutils.h"
#include "qvector.h"
#include "qtoken_gen.h"

#define mixchar 	'.':case '<':case '>':case '=':case '^':case '!':\
									case '&':case '|':case '+':case '-':case '*':case '/':\
									case '@':case ':':case '%':case '?'
#define resetbuf(lex) (lex->nbuf=0)
#define nextp(lex) ((lex->ptr>=lex->tail)?(lex->fill(lex),*(lex->ptr)):(*(++lex->ptr)))
#define lchar(lex) (*lex->ptr)
#define incline(lex) ++lex->curline
#define nextpc(lex) ({\
		if(is_newline(nextp(lex))){\
			incline(lex);\
			if (lchar(lex)=='\r'&&*(lex->ptr + 1) == '\n')\
				++lex->ptr;}\
				lchar(lex);	})
#define save_next(lex) ({lex->buffer[lex->nbuf++]=lchar(lex);nextp(lex);})
static void fillFromFile(qlexer* lex) {
	int read = fread(lex->read, sizeof(char), LEX_BUFSIZE - 1,
			cast(FILE*, lex->file)); //_IO_BUFSIZ
	lex->read[read] = 0;
	lex->tail = lex->read + read - 1;
	lex->ptr = lex->read;
//	lex->ptr = lex->read - 1;
}
/**
 * 添加关键字Token
 */
void addStrToken(qlexer *lex, char *s, int len) {
	qstr *str = STR.create(s, len);
	skyc_assert_(str->info&TFLAG);
	addToken(lex, str->info & IDMASK, str);
}
qlexer* create_lexer_file(char *path) {
	qlexer* lex = qcalloc(qlexer);
	lex->tkcache = Bytes.create(sizeof(qtk));
	lex->file = fopen(path, "r");
	lex->fill = fillFromFile;
	lex->close = fclose;
	nextpc(lex);
	return lex;
}
void set_lexer_file(qlexer *lex, char *path) {
	if (lex->file)
		lex->close(lex->file);
	lex->fill = fillFromFile;
	lex->close = fclose;
	lex->file = fopen(path, "r");
	lex->close = fclose;
	lex->fill(lex);
	lex->tkcache->length = 0;
	return lex;
}
void lex_destory(qlexer *lex) {
	Bytes.destroy(&lex->tkcache);
	lex->close(lex->file);
	qfree(lex);
}
#define addToken_(lex,type,val) addToken(lex,type,val);\
	if(lex->tokenHook) lex->tokenHook(lex->ctx,Bytes_last(lex->tkcache,qtk))
void addToken(qlexer *lex, int type, void *val) {
	checksize(lex->tkcache, 1);
	qtk *tk = Bytes_tail(lex->tkcache, qtk);
	tk->type = type;
	tk->v.p = val;
	lex->tkcache->length += 1;
}
enum num_type {
	N_NEG, N_DOU, N_X
};
static void lex_num(qlexer* lex) {
	byte type = 0;
	FLT vd = 0;
	INT vi = 0;
	char dot = -1;
	char fbit;
//	if (lex->tkcache->length) {
//		if (cast(qtk*,Bytes_last(lex->tkcache))->type == TW_SUB) {
//			type |= MASK(N_NEG);
//			//		nextp(lex);
//		}
//	}
//	lex->token = TAB_INT;
//	if (lex->token == '-') {
//		type |= MASK(N_NEG);
//		nextp(lex);
//	}
	if (lchar(lex) == '0') {
		if (nextp(lex) == 'x' || lchar(lex) == 'X') {
			type |= MASK(N_X);
			nextp(lex);
		}
	}
	do {
		if (dot >= 0)
			dot++;
		if (is_dec(lchar(lex)) && (!BIT(type, N_X, 1))) {
			fbit = lchar(lex) - '0';
			dot >= 0 ? (vd = 10 * vd + fbit) : (vi = 10 * vi + fbit);
		} else if (is_hex(lchar(lex)) && (type & MASK(N_X))) {
			if (lchar(lex) >= 'a') {
				fbit = 10 + lchar(lex) - 'a';
			} else if (lchar(lex) >= 'A') {
				fbit = 10 + lchar(lex) - 'A';
			} else {
				fbit = lchar(lex) - '0';
			}
			dot >= 0 ? (vd = vd * 16 + fbit) : (vi = 16 * vi + fbit);
		} else if (lchar(lex) == '.') {
			if (!BIT(type, N_DOU, 1)) {
				type |= MASK(N_DOU);
//				lex->token = TK_FLT;
				dot = 0;
				vd = vi;
			} else
//				printf( "Couldn't lexrser file: %s\n", lex->src);
				sky_error("parse num error!", 1);
		} else {
			sky_error("parse num error!", 1);
		}
		nextp(lex);
	} while (is_hex(lchar(lex)) || lchar(lex) == '.');
	if (BIT(type, N_NEG, 1)) {
		vi = -vi;
		vd = -vd;
	}
	qobj* num = qmalloc(qobj);
	if (dot >= 0) {
		vd /= (BIT(type,N_X,1) ? Q_POW(16, dot) : Q_POW(10, dot));
		num->val.flt = vd;
		num->type = typeFloat;
	} else {
		num->val.i = vi;
		num->type = typeInt;
	}
	lex->u.o = num;
}
void lex_string(qlexer* lex, char end) {
	static qstrbuf buf;
	buf.n = 0;
	nextp(lex);
	char c;
	while (lchar(lex) != end) {
		switch (lchar(lex)) {
		case '\\':
			nextp(lex);
			switch (lchar(lex)) {
			case 'a':
				c = '\a';
				goto read_save;
			case 'b':
				c = '\b';
				goto read_save;
			case 'f':
				c = '\f';
				goto read_save;
			case 'n':
				c = '\n';
				goto read_save;
			case 'r':
				c = '\r';
				goto read_save;
			case 't':
				c = '\t';
				goto read_save;
			case 'v':
				c = '\v';
				goto read_save;
			case 'u':
				break;
//				utf8esc(lex);
//				goto no_save;
			case ' ':
			case '\f':
			case '\t':
			case '\v':
//				while(c=nextp(lex)){
//					switch()
//				}
				break;
			case '\n':
			case '\r':
				incline(lex);
				c = '\n';
				goto read_save;
			case '\"':
			case '\'':
				c = lchar(lex);
				goto read_save;
			default:
				sky_error("string error!", 0);
			}
		case '\r':
		case '\n':
			sky_error("string error!", 0);
			break;

		default:
			STR.add(&buf, lchar(lex), 0);
			nextp(lex);
			break;
			read_save: STR.add(&buf, c, 0);
			nextp(lex);
			break;
		}
	}
	STR.add(&buf, '\0', 0);
	qstr* str = STR.get(buf.val);
	lex->u.s = str;
	nextp(lex);
}
int gen_tokens(qlexer *lex, int num) {
	bool flag;
	qbytes *tkcache = lex->tkcache;
	int oldn = tkcache->length;
	do {
		switch (lchar(lex)) {
		case 0:
			addToken(lex, TW_END, 0);
			return tkcache->length - oldn;
		case '#':
			if (nextp(lex) == '#' && nextp(lex) == '#') {
				while (nextpc(lex) != '#' || nextpc(lex) != '#' || nextpc(lex) != '#') {
				}
				nextp(lex);
				printf("###\n");
			} else {
				while (!is_newline(lchar(lex)) && lchar(lex)) {
					nextpc(lex);
				}
//				lex->curline++;
			}
			break;
		case '"':
		case '\'':
			lex_string(lex, lchar(lex));
			addToken_(lex, TW_STRING, lex->u.s)
			;
			break;
//			return lex->token = TK_STRING;
		case '0':
		case '1':
		case '2':
		case '3':
		case '4':
		case '5':
		case '6':
		case '7':
		case '8':
		case '9':
			lex_num(lex);
			addToken_(lex, TW_NUM, lex->u.o)
			;
			break;
//			return lex->token;
		case '\r':
		case '\n':
		case ';':
		case ' ':
		case '\f':
		case '\t':
		case '\v':
			flag = true;
//			lex->token = 0;
			do {
				switch (lchar(lex)) {
				case '\r':
					if (*(lex->ptr + 1) == '\n')
						++lex->ptr;
				case '\n':
					incline(lex);
				case ';':
					if (lex->tkcache->length == 0
							|| Bytes_last(tkcache,qtk)->type != TW_NLINE)
						addToken(lex, TW_NLINE, NULL);
				case ' ':
				case '\f':
				case '\t':
				case '\v':
					break;
				default:
					flag = false;
				}
			} while (flag && nextp(lex));
			break;
		case mixchar:
			resetbuf(lex);
			save_next(lex);
			flag = true;
//			lex->token = 0;
			do {
				switch (lchar(lex)) {
				case mixchar:
					save_next(lex);
					break;
				default:
					addStrToken(lex, lex->buffer, lex->nbuf);
					flag = false;
				}
			} while (flag);
			break;
		case ',':
		case '[':
		case ']':
		case '(':
		case ')':
		case '{':
		case '}':
			lex->buffer[0] = lchar(lex);
			addStrToken(lex, lex->buffer, 1);
			nextp(lex);
			break;
		default:
			if (is_alp(lchar(lex))) {
				resetbuf(lex);
				while (is_alp(lchar(lex)) || is_dec(lchar(lex))) {
					save_next(lex);
				}
				qstr* str = STR.create(lex->buffer, lex->nbuf);
				if (str->info & TFLAG) {
					addToken(lex, str->info & IDMASK, str);
				} else {
					addToken_(lex, TW_NAME, str)
					;
				}
			} else {
				sky_error("unknown symbol!", 0);
			}
			break;
		} //end switch
	} while (tkcache->length - oldn < num && lchar(lex));
	if (lchar(lex) == '\0') {
		addToken(lex, TW_END, 0);
	}
	return tkcache->length - oldn;
}

