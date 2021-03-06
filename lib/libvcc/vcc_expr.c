/*-
 * Copyright (c) 2006 Verdens Gang AS
 * Copyright (c) 2006-2011 Varnish Software AS
 * All rights reserved.
 *
 * Author: Poul-Henning Kamp <phk@phk.freebsd.dk>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#include "config.h"

#include <math.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include "vcc_compile.h"

struct expr {
	unsigned	magic;
#define EXPR_MAGIC	0x38c794ab
	vcc_type_t	fmt;
	struct vsb	*vsb;
	uint8_t		constant;
#define EXPR_VAR	(1<<0)
#define EXPR_CONST	(1<<1)
#define EXPR_STR_CONST	(1<<2)		// Last STRING_LIST elem is "..."
	struct token	*t1, *t2;
	int		nstr;
};

/*--------------------------------------------------------------------
 * Facility for carrying expressions around and do text-processing on
 * them.
 */

static inline int
vcc_isconst(const struct expr *e)
{
	AN(e->constant);
	return (e->constant & EXPR_CONST);
}

static inline int
vcc_islit(const struct expr *e)
{
	AN(e->constant);
	return (e->constant & EXPR_STR_CONST);
}

static const char *
vcc_utype(vcc_type_t t)
{
	if (t == STRINGS || t == STRING_LIST)
		t = STRING;
	return (t->name);
}

static void vcc_expr0(struct vcc *tl, struct expr **e, vcc_type_t fmt);
static void vcc_expr_cor(struct vcc *tl, struct expr **e, vcc_type_t fmt);

static struct expr *
vcc_new_expr(vcc_type_t fmt)
{
	struct expr *e;

	ALLOC_OBJ(e, EXPR_MAGIC);
	AN(e);
	e->vsb = VSB_new_auto();
	e->fmt = fmt;
	e->constant = EXPR_VAR;
	return (e);
}

static struct expr * v_printflike_(2, 3)
vcc_mk_expr(vcc_type_t fmt, const char *str, ...)
{
	va_list ap;
	struct expr *e;

	e = vcc_new_expr(fmt);
	va_start(ap, str);
	VSB_vprintf(e->vsb, str, ap);
	va_end(ap);
	AZ(VSB_finish(e->vsb));
	return (e);
}

static void
vcc_delete_expr(struct expr *e)
{
	if (e == NULL)
		return;
	CHECK_OBJ_NOTNULL(e, EXPR_MAGIC);
	VSB_destroy(&e->vsb);
	FREE_OBJ(e);
}

/*--------------------------------------------------------------------
 * We want to get the indentation right in the emitted C code so we have
 * to represent it symbolically until we are ready to render.
 *
 * Many of the operations have very schematic output syntaxes, so we
 * use the same facility to simplify the text-processing of emitting
 * a given operation on two subexpressions.
 *
 * We use '\v' as the magic escape character.
 *	\v1  insert subexpression 1
 *	\v2  insert subexpression 2
 *	\vS  insert subexpression 1(STRINGS) as STRING
 *	\vT  insert subexpression 1(STRINGS) as STRANDS
 *	\vt  insert subexpression 1(STRINGS) as STRANDS
 *	\v+  increase indentation
 *	\v-  decrease indentation
 *	anything else is literal
 *
 * When editing, we check if any of the subexpressions contain a newline
 * and issue it as an indented block of so.
 *
 * XXX: check line lengths in edit, should pass indent in for this
 */

static struct expr *
vcc_expr_edit(struct vcc *tl, vcc_type_t fmt, const char *p, struct expr *e1,
    struct expr *e2)
{
	struct expr *e, *e3;
	int nl = 1;

	AN(e1);
	e = vcc_new_expr(fmt);
	while (*p != '\0') {
		if (*p != '\v') {
			if (*p != '\n' || !nl)
				VSB_putc(e->vsb, *p);
			nl = (*p == '\n');
			p++;
			continue;
		}
		assert(*p == '\v');
		switch (*++p) {
		case '+': VSB_cat(e->vsb, "\v+"); break;
		case '-': VSB_cat(e->vsb, "\v-"); break;
		case 'S':
		case 's':
			e3 = (*p == 'S' ? e1 : e2);
			AN(e3);
			assert(e1->fmt == STRINGS);
			if (e3->nstr > 1)
				VSB_cat(e->vsb,
				    "\nVRT_CollectString(ctx,\v+\n");
			VSB_cat(e->vsb, VSB_data(e3->vsb));
			if (e3->nstr > 1)
				VSB_cat(e->vsb,
				    ",\nvrt_magic_string_end)\v-\n");
			break;
		case 'T':
		case 't':
			e3 = (*p == 'T' ? e1 : e2);
			AN(e3);
			VSB_printf(tl->curproc->prologue,
			    "  struct strands strs_%u_a;\n"
			    "  const char * strs_%u_s[%d];\n",
			    tl->unique, tl->unique, e3->nstr);
			VSB_printf(e->vsb,
			    "\v+\nVRT_BundleStrands(%d, &strs_%u_a, strs_%u_s,"
			    "\v+\n%s,\nvrt_magic_string_end)\v-\v-",
			    e3->nstr, tl->unique, tl->unique,
			VSB_data(e3->vsb));
			tl->unique++;
			break;
		case '1':
			VSB_cat(e->vsb, VSB_data(e1->vsb));
			break;
		case '2':
			AN(e2);
			VSB_cat(e->vsb, VSB_data(e2->vsb));
			break;
		default:
			WRONG("Illegal edit in VCC expression");
		}
		p++;
	}
	AZ(VSB_finish(e->vsb));
	e->t1 = e1->t1;
	e->t2 = e1->t2;
	if (e2 != NULL)
		e->t2 = e2->t2;
	vcc_delete_expr(e1);
	vcc_delete_expr(e2);
	return (e);
}

/*--------------------------------------------------------------------
 * Expand finished expression into C-source code
 */

static void
vcc_expr_fmt(struct vsb *d, int ind, const struct expr *e1)
{
	char *p;
	int i;

	for (i = 0; i < ind; i++)
		VSB_cat(d, " ");
	p = VSB_data(e1->vsb);
	while (*p != '\0') {
		if (*p == '\n') {
			VSB_putc(d, '\n');
			if (*++p == '\0')
				break;
			for (i = 0; i < ind; i++)
				VSB_cat(d, " ");
		} else if (*p != '\v') {
			VSB_putc(d, *p++);
		} else {
			switch (*++p) {
			case '+': ind += 2; break;
			case '-': ind -= 2; break;
			default:  WRONG("Illegal format in VCC expression");
			}
			p++;
		}
	}
}

/*--------------------------------------------------------------------
 */

static void
vcc_expr_tostring(struct vcc *tl, struct expr **e, vcc_type_t fmt)
{
	const char *p;
	uint8_t	constant = EXPR_VAR;

	CHECK_OBJ_NOTNULL(*e, EXPR_MAGIC);
	assert(fmt == STRINGS || fmt == STRING_LIST || fmt == STRING);
	assert(fmt != (*e)->fmt);

	p = (*e)->fmt->tostring;
	if (p != NULL) {
		AN(*p);
		*e = vcc_expr_edit(tl, fmt, p, *e, NULL);
		(*e)->constant = constant;
		(*e)->nstr = 1;
	} else {
		if ((*e)->fmt == BLOB)
			VSB_printf(tl->sb,
			    "Wrong use of BLOB value.\n"
			    "BLOBs can only be used as arguments to VMOD"
			    " functions.\n");
		else
			VSB_printf(tl->sb,
			    "Cannot convert %s to STRING.\n",
			    vcc_utype((*e)->fmt));
		vcc_ErrWhere2(tl, (*e)->t1, tl->t);
	}
}

/*--------------------------------------------------------------------
 */

static void v_matchproto_(sym_expr_t)
vcc_Eval_Regsub(struct vcc *tl, struct expr **e, struct token *t,
    struct symbol *sym, vcc_type_t fmt)
{
	struct expr *e2;
	int all = sym->eval_priv == NULL ? 0 : 1;
	const char *p;
	char buf[128];

	(void)t;
	(void)fmt;
	SkipToken(tl, '(');
	vcc_expr0(tl, &e2, STRING);
	ERRCHK(tl);
	SkipToken(tl, ',');
	ExpectErr(tl, CSTR);
	p = vcc_regexp(tl);
	bprintf(buf, "VRT_regsub(ctx, %d,\v+\n\v1,\n%s", all, p);
	*e = vcc_expr_edit(tl, STRING, buf, e2, NULL);
	SkipToken(tl, ',');
	vcc_expr0(tl, &e2, STRING);
	ERRCHK(tl);
	*e = vcc_expr_edit(tl, STRINGS, "\v1,\n\v2)\v-", *e, e2);
	(*e)->nstr = 1;
	SkipToken(tl, ')');
}

/*--------------------------------------------------------------------
 */

static void v_matchproto_(sym_expr_t)
vcc_Eval_BoolConst(struct vcc *tl, struct expr **e, struct token *t,
    struct symbol *sym, vcc_type_t fmt)
{

	(void)t;
	(void)tl;
	(void)fmt;
	*e = vcc_mk_expr(BOOL, "(0==%d)", sym->eval_priv == NULL ? 1 : 0);
	(*e)->constant = EXPR_CONST;
}

/*--------------------------------------------------------------------
 */

void v_matchproto_(sym_expr_t)
vcc_Eval_Handle(struct vcc *tl, struct expr **e, struct token *t,
    struct symbol *sym, vcc_type_t type)
{

	(void)t;
	(void)tl;
	AN(sym->rname);

	if (sym->type != STRING && type == STRINGS) {
		*e = vcc_mk_expr(STRINGS, "\"%s\"", sym->name);
		(*e)->nstr = 1;
		(*e)->constant |= EXPR_CONST | EXPR_STR_CONST;
	} else {
		*e = vcc_mk_expr(sym->type, "%s", sym->rname);
		(*e)->constant = EXPR_VAR;
		(*e)->nstr = 1;
		if ((*e)->fmt == STRING)
			(*e)->fmt = STRINGS;
	}
}

/*--------------------------------------------------------------------
 */

void v_matchproto_(sym_expr_t)
vcc_Eval_Var(struct vcc *tl, struct expr **e, struct token *t,
    struct symbol *sym, vcc_type_t type)
{

	(void)type;
	assert(sym->kind == SYM_VAR);
	vcc_AddUses(tl, t, NULL, sym->r_methods, "Not available");
	ERRCHK(tl);
	*e = vcc_mk_expr(sym->type, "%s", sym->rname);
	(*e)->constant = EXPR_VAR;
	(*e)->nstr = 1;
	if ((*e)->fmt == STRING)
		(*e)->fmt = STRINGS;
}

/*--------------------------------------------------------------------
 */

static struct expr *
vcc_priv_arg(struct vcc *tl, const char *p, const char *name, const char *vmod)
{
	struct expr *e2;
	char buf[32];
	struct inifin *ifp;

	(void)name;
	if (!strcmp(p, "PRIV_VCL")) {
		e2 = vcc_mk_expr(VOID, "&vmod_priv_%s", vmod);
	} else if (!strcmp(p, "PRIV_CALL")) {
		bprintf(buf, "vmod_priv_%u", tl->unique++);
		ifp = New_IniFin(tl);
		Fh(tl, 0, "static struct vmod_priv %s;\n", buf);
		VSB_printf(ifp->fin, "\tVRT_priv_fini(&%s);", buf);
		e2 = vcc_mk_expr(VOID, "&%s", buf);
	} else if (!strcmp(p, "PRIV_TASK")) {
		e2 = vcc_mk_expr(VOID,
		    "VRT_priv_task(ctx, &VGC_vmod_%s)", vmod);
	} else if (!strcmp(p, "PRIV_TOP")) {
		e2 = vcc_mk_expr(VOID,
		    "VRT_priv_top(ctx, &VGC_vmod_%s)", vmod);
	} else {
		WRONG("Wrong PRIV_ type");
	}
	return (e2);
}

struct func_arg {
	vcc_type_t		type;
	const char		*enum_bits;
	const char		*cname;
	const char		*name;
	const char		*val;
	struct expr		*result;
	VTAILQ_ENTRY(func_arg)	list;
};

static void
vcc_do_enum(struct vcc *tl, struct func_arg *fa, int len, const char *ptr)
{
	const char *r;

	(void)tl;
	r = strchr(fa->cname, '.');
	AN(r);
	fa->result = vcc_mk_expr(VOID, "*%.*s.enum_%.*s",
	    (int)(r - fa->cname), fa->cname, len, ptr);
}

static void
vcc_do_arg(struct vcc *tl, struct func_arg *fa)
{
	const char *p, *r;
	struct expr *e2;

	if (fa->type == ENUM) {
		ExpectErr(tl, ID);
		ERRCHK(tl);
		r = p = fa->enum_bits;
		do {
			if (vcc_IdIs(tl->t, p))
				break;
			p += strlen(p) + 1;
		} while (*p != '\1');
		if (*p == '\1') {
			VSB_printf(tl->sb, "Wrong enum value.");
			VSB_printf(tl->sb, "  Expected one of:\n");
			do {
				VSB_printf(tl->sb, "\t%s\n", r);
				r += strlen(r) + 1;
			} while (*r != '\0' && *r != '\1');
			vcc_ErrWhere(tl, tl->t);
			return;
		}
		vcc_do_enum(tl, fa, PF(tl->t));
		SkipToken(tl, ID);
	} else {
		vcc_expr0(tl, &e2, fa->type);
		ERRCHK(tl);
		assert(e2->fmt == fa->type);
		fa->result = e2;
	}
}

static void
vcc_func(struct vcc *tl, struct expr **e, const char *spec,
    const char *extra, const struct symbol *sym)
{
	vcc_type_t rfmt;
	const char *args;
	const char *cfunc;
	const char *p;
	struct expr *e1;
	struct func_arg *fa, *fa2;
	VTAILQ_HEAD(,func_arg) head;
	struct token *t1;

	rfmt = VCC_Type(spec);
	spec += strlen(spec) + 1;
	cfunc = spec;
	spec += strlen(spec) + 1;
	args = spec;
	SkipToken(tl, '(');
	p = args;
	if (extra == NULL)
		extra = "";
	AN(rfmt);
	VTAILQ_INIT(&head);
	while (*p != '\0') {
		fa = calloc(1, sizeof *fa);
		AN(fa);
		fa->cname = cfunc;
		VTAILQ_INSERT_TAIL(&head, fa, list);
		if (!memcmp(p, "PRIV_", 5)) {
			fa->result = vcc_priv_arg(tl, p, sym->name, sym->vmod);
			fa->name = "";
			p += strlen(p) + 1;
			continue;
		}
		fa->type = VCC_Type(p);
		AN(fa->type);
		p += strlen(p) + 1;
		if (*p == '\1') {
			fa->enum_bits = ++p;
			while (*p != '\1')
				p += strlen(p) + 1;
			p++;
			assert(*p == '\0');
			p++;
		}
		if (*p == '\2') {
			fa->name = p + 1;
			p += strlen(p) + 1;
		}
		if (*p == '\3') {
			fa->val = p + 1;
			p += strlen(p) + 1;
		}
		assert(*p == 0 || *p > ' ');
	}

	VTAILQ_FOREACH(fa, &head, list) {
		if (tl->t->tok == ')')
			break;
		if (fa->result != NULL)
			continue;
		if (tl->t->tok == ID) {
			t1 = VTAILQ_NEXT(tl->t, list);
			if (t1->tok == '=')
				break;
		}
		vcc_do_arg(tl, fa);
		ERRCHK(tl);
		if (tl->t->tok == ')')
			break;
		SkipToken(tl, ',');
	}
	while (tl->t->tok == ID) {
		VTAILQ_FOREACH(fa, &head, list) {
			if (fa->name == NULL)
				continue;
			if (vcc_IdIs(tl->t, fa->name))
				break;
		}
		if (fa == NULL) {
			VSB_printf(tl->sb, "Unknown argument '%.*s'\n",
			    PF(tl->t));
			vcc_ErrWhere(tl, tl->t);
			return;
		}
		if (fa->result != NULL) {
			VSB_printf(tl->sb, "Argument '%s' already used\n",
			    fa->name);
			vcc_ErrWhere(tl, tl->t);
			return;
		}
		vcc_NextToken(tl);
		SkipToken(tl, '=');
		vcc_do_arg(tl, fa);
		ERRCHK(tl);
		if (tl->t->tok == ')')
			break;
		SkipToken(tl, ',');
	}

	e1 = vcc_mk_expr(rfmt, "%s(ctx%s\v+", cfunc, extra);
	VTAILQ_FOREACH_SAFE(fa, &head, list, fa2) {
		if (fa->result == NULL && fa->type == ENUM && fa->val != NULL)
			vcc_do_enum(tl, fa, strlen(fa->val), fa->val);
		if (fa->result == NULL && fa->val != NULL)
			fa->result = vcc_mk_expr(fa->type, "%s", fa->val);
		if (fa->result != NULL)
			e1 = vcc_expr_edit(tl, e1->fmt, "\v1,\n\v2",
			    e1, fa->result);
		else {
			VSB_printf(tl->sb, "Argument '%s' missing\n",
			    fa->name);
			vcc_ErrWhere(tl, tl->t);
		}
		free(fa);
	}
	*e = vcc_expr_edit(tl, e1->fmt, "\v1\n)\v-", e1, NULL);
	SkipToken(tl, ')');
}

/*--------------------------------------------------------------------
 */

void
vcc_Eval_Func(struct vcc *tl, const char *spec,
    const char *extra, const struct symbol *sym)
{
	struct expr *e = NULL;

	vcc_func(tl, &e, spec, extra, sym);
	ERRCHK(tl);
	vcc_expr_fmt(tl->fb, tl->indent, e);
	VSB_cat(tl->fb, ";\n");
	vcc_delete_expr(e);
}

/*--------------------------------------------------------------------
 */

void v_matchproto_(sym_expr_t)
vcc_Eval_SymFunc(struct vcc *tl, struct expr **e, struct token *t,
    struct symbol *sym, vcc_type_t fmt)
{

	(void)t;
	(void)fmt;
	assert(sym->kind == SYM_FUNC);
	AN(sym->eval_priv);

	// assert(sym->fmt == VCC_Type(sym->eval_priv));
	vcc_func(tl, e, sym->eval_priv, sym->extra, sym);
	ERRCHK(tl);
	if ((*e)->fmt == STRING) {
		(*e)->fmt = STRINGS;
		(*e)->nstr = 1;
	}
}

/*--------------------------------------------------------------------
 * SYNTAX:
 *    Expr4:
 *	'(' ExprCor ')'
 *	symbol
 *	CNUM
 *	CSTR
 */

static void
vcc_expr4(struct vcc *tl, struct expr **e, vcc_type_t fmt)
{
	struct expr *e1, *e2;
	const char *ip, *sign;
	struct token *t;
	struct symbol *sym;
	double d;
	int i;

	sign = "";
	*e = NULL;
	if (tl->t->tok == '(') {
		SkipToken(tl, '(');
		vcc_expr_cor(tl, &e2, fmt);
		ERRCHK(tl);
		SkipToken(tl, ')');
		if (e2->fmt == STRINGS)
			*e = e2;
		else
			*e = vcc_expr_edit(tl, e2->fmt, "(\v1)", e2, NULL);
		return;
	}
	switch (tl->t->tok) {
	case ID:
		if (vcc_IdIs(tl->t, "default") && fmt == PROBE) {
			vcc_NextToken(tl);
			*e = vcc_mk_expr(PROBE, "%s", vcc_default_probe(tl));
			return;
		}
		if (vcc_IdIs(tl->t, "default") && fmt == BACKEND) {
			vcc_NextToken(tl);
			*e = vcc_mk_expr(BACKEND,
			    "*(VCL_conf.default_director)");
			return;
		}
		t = tl->t;
		sym = VCC_SymbolGet(tl, SYM_NONE, "Symbol not found",
		    XREF_REF);
		ERRCHK(tl);
		AN(sym);
		if (sym->kind == SYM_FUNC && sym->type == VOID) {
			VSB_printf(tl->sb, "Function returns VOID:\n");
			vcc_ErrWhere(tl, tl->t);
			return;
		}
		if (sym->eval != NULL) {
			AN(sym->eval);
			AZ(*e);
			sym->eval(tl, e, t, sym, fmt);
			ERRCHK(tl);
			/* Unless asked for a HEADER, fold to string here */
			if (*e && fmt != HEADER && (*e)->fmt == HEADER) {
				vcc_expr_tostring(tl, e, STRINGS);
				ERRCHK(tl);
			}
			return;
		}
		VSB_printf(tl->sb,
		    "Symbol type (%s) can not be used in expression.\n",
		    sym->kind->name);
		vcc_ErrWhere(tl, tl->t);
		if (sym->def_b != NULL) {
			VSB_printf(tl->sb, "That symbol was defined here:\n");
			vcc_ErrWhere(tl, sym->def_b);
		}
		return;
	case CSTR:
		assert(fmt != VOID);
		if (fmt == IP) {
			if (tl->t->dec[0] == '/') {
				VSB_printf(tl->sb,
					"Cannot convert to an IP address: ");
				vcc_ErrToken(tl, tl->t);
				vcc_ErrWhere(tl, tl->t);
				return;
			}
			Resolve_Sockaddr(tl, tl->t->dec, "80",
			    &ip, NULL, &ip, NULL, NULL, 1,
			    tl->t, "IP constant");
			ERRCHK(tl);
			e1 = vcc_mk_expr(IP, "%s", ip);
			ERRCHK(tl);
		} else {
			e1 = vcc_new_expr(STRINGS);
			EncToken(e1->vsb, tl->t);
			AZ(VSB_finish(e1->vsb));
			e1->constant |= EXPR_STR_CONST;
			e1->nstr = 1;
		}
		e1->t1 = tl->t;
		e1->constant |= EXPR_CONST;
		vcc_NextToken(tl);
		*e = e1;
		return;
	case '-':
		if (fmt != INT && fmt != REAL && fmt != DURATION)
			break;
		vcc_NextToken(tl);
		ExpectErr(tl, CNUM);
		sign = "-";
		/* FALLTHROUGH */
	case CNUM:
		/*
		 * XXX: %g may not have enough decimals by default
		 * XXX: but %a is ugly, isn't it ?
		 */
		assert(fmt != VOID);
		if (fmt == BYTES) {
			vcc_ByteVal(tl, &d);
			ERRCHK(tl);
			e1 = vcc_mk_expr(BYTES, "%.1f", d);
		} else {
			vcc_NumVal(tl, &d, &i);
			ERRCHK(tl);
			if (tl->t->tok == ID) {
				e1 = vcc_mk_expr(DURATION, "%s%g",
				    sign, d * vcc_TimeUnit(tl));
				ERRCHK(tl);
			} else if (i || fmt == REAL)
				e1 = vcc_mk_expr(REAL, "%s%f", sign, d);
			else
				e1 = vcc_mk_expr(INT, "%s%ld",
				    sign, (unsigned long)d);
		}
		e1->constant = EXPR_CONST;
		*e = e1;
		return;
	default:
		break;
	}
	VSB_printf(tl->sb, "Unknown token ");
	vcc_ErrToken(tl, tl->t);
	VSB_printf(tl->sb, " when looking for %s\n\n", vcc_utype(fmt));
	vcc_ErrWhere(tl, tl->t);
}

/*--------------------------------------------------------------------
 * SYNTAX:
 *    ExprMul:
 *      Expr4 { {'*'|'/'} Expr4 } *
 */

static void
vcc_expr_mul(struct vcc *tl, struct expr **e, vcc_type_t fmt)
{
	struct expr *e2;
	vcc_type_t f2;
	struct token *tk;

	*e = NULL;
	vcc_expr4(tl, e, fmt);
	ERRCHK(tl);
	AN(*e);

	while (tl->t->tok == '*' || tl->t->tok == '/') {
		f2 = (*e)->fmt->multype;
		if (f2 == NULL) {
			VSB_printf(tl->sb,
			    "Operator %.*s not possible on type %s.\n",
			    PF(tl->t), vcc_utype((*e)->fmt));
			vcc_ErrWhere(tl, tl->t);
			return;
		}
		tk = tl->t;
		vcc_NextToken(tl);
		vcc_expr4(tl, &e2, f2);
		ERRCHK(tl);
		if (e2->fmt != INT && e2->fmt != f2) {
			VSB_printf(tl->sb, "%s %.*s %s not possible.\n",
			    vcc_utype((*e)->fmt), PF(tk), vcc_utype(e2->fmt));
			vcc_ErrWhere(tl, tk);
			return;
		}
		if (tk->tok == '*')
			*e = vcc_expr_edit(tl, (*e)->fmt, "(\v1*\v2)", *e, e2);
		else
			*e = vcc_expr_edit(tl, (*e)->fmt, "(\v1/\v2)", *e, e2);
	}
}

/*--------------------------------------------------------------------
 * SYNTAX:
 *    ExprAdd:
 *      ExprMul { {'+'|'-'} ExprMul } *
 */

static const struct adds {
	unsigned	op;
	vcc_type_t	a;
	vcc_type_t	b;
	vcc_type_t	fmt;
} vcc_adds[] = {
	{ '+', BYTES,		BYTES,		BYTES },
	{ '-', BYTES,		BYTES,		BYTES },
	{ '+', DURATION,	DURATION,	DURATION },
	{ '-', DURATION,	DURATION,	DURATION },
	{ '+', INT,		INT,		INT },
	{ '-', INT,		INT,		INT },
	{ '+', INT,		REAL,		REAL },
	{ '-', INT,		REAL,		REAL },
	{ '+', REAL,		INT,		REAL },
	{ '-', REAL,		INT,		REAL },
	{ '+', REAL,		REAL,		REAL },
	{ '-', REAL,		REAL,		REAL },
	{ '-', TIME,		TIME,		DURATION },
	{ '+', TIME,		DURATION,	TIME },
	{ '-', TIME,		DURATION,	TIME },

	{ EOI, VOID,		VOID,		VOID }
};

static void
vcc_expr_add(struct vcc *tl, struct expr **e, vcc_type_t fmt)
{
	const struct adds *ap;
	struct expr  *e2;
	struct token *tk;
	int lit, n;

	*e = NULL;
	vcc_expr_mul(tl, e, fmt);
	ERRCHK(tl);

	while (tl->t->tok == '+' || tl->t->tok == '-') {
		tk = tl->t;
		for (ap = vcc_adds; ap->op != EOI; ap++)
			if (tk->tok == ap->op && (*e)->fmt == ap->a)
				break;
		vcc_NextToken(tl);
		if (ap->op == EOI && fmt == STRINGS)
			vcc_expr_mul(tl, &e2, STRINGS);
		else
			vcc_expr_mul(tl, &e2, (*e)->fmt);
		ERRCHK(tl);

		for (ap = vcc_adds; ap->op != EOI; ap++)
			if (tk->tok == ap->op && (*e)->fmt == ap->a &&
			    e2->fmt == ap->b)
				break;

		if (ap->op == '+') {
			*e = vcc_expr_edit(tl, ap->fmt, "(\v1 + \v2)", *e, e2);
		} else if (ap->op == '-') {
			*e = vcc_expr_edit(tl, ap->fmt, "(\v1 - \v2)", *e, e2);
		} else if (tk->tok == '+' &&
		    ((*e)->fmt == STRINGS || fmt == STRINGS)) {
			if ((*e)->fmt != STRINGS)
				vcc_expr_tostring(tl, e, STRINGS);
			if (e2->fmt != STRINGS)
				vcc_expr_tostring(tl, &e2, STRINGS);
			if (vcc_islit(*e) && vcc_isconst(e2)) {
				lit = vcc_islit(e2);
				*e = vcc_expr_edit(tl, STRINGS,
				    "\v1\n\v2", *e, e2);
				(*e)->constant = EXPR_CONST;
				if (lit)
					(*e)->constant |= EXPR_STR_CONST;
			} else {
				n = (*e)->nstr + e2->nstr;
				*e = vcc_expr_edit(tl, STRINGS,
				    "\v1,\n\v2", *e, e2);
				(*e)->constant = EXPR_VAR;
				(*e)->nstr = n;
			}
		} else {
			VSB_printf(tl->sb, "%s %.*s %s not possible.\n",
			    vcc_utype((*e)->fmt), PF(tk), vcc_utype(e2->fmt));
			vcc_ErrWhere2(tl, tk, tl->t);
			return;
		}
	}
}

/*--------------------------------------------------------------------
 * SYNTAX:
 *    ExprCmp:
 *	ExprAdd
 *      ExprAdd Relation ExprAdd
 *	ExprAdd(STRING) '~' CString
 *	ExprAdd(STRING) '!~' CString
 *	ExprAdd(IP) '==' ExprAdd(IP)
 *	ExprAdd(IP) '!=' ExprAdd(IP)
 *	ExprAdd(IP) '~' ACL
 *	ExprAdd(IP) '!~' ACL
 */

struct cmps;

typedef void cmp_f(struct vcc *, struct expr **, const struct cmps *);

struct cmps {
	vcc_type_t		fmt;
	unsigned		token;
	cmp_f			*func;
	const char		*emit;
};

static void v_matchproto_(cmp_f)
cmp_simple(struct vcc *tl, struct expr **e, const struct cmps *cp)
{
	struct expr *e2;
	struct token *tk;

	tk = tl->t;
	vcc_NextToken(tl);
	vcc_expr_add(tl, &e2, (*e)->fmt);
	ERRCHK(tl);

	if (e2->fmt != (*e)->fmt) {
		VSB_printf(tl->sb,
		    "Comparison of different types: %s '%.*s' %s\n",
		    vcc_utype((*e)->fmt), PF(tk), vcc_utype(e2->fmt));
		vcc_ErrWhere(tl, tk);
	} else
		*e = vcc_expr_edit(tl, BOOL, cp->emit, *e, e2);
}

static void v_matchproto_(cmp_f)
cmp_regexp(struct vcc *tl, struct expr **e, const struct cmps *cp)
{
	char buf[128];
	const char *re;

	*e = vcc_expr_edit(tl, STRING, "\vS", *e, NULL);
	vcc_NextToken(tl);
	ExpectErr(tl, CSTR);
	re = vcc_regexp(tl);
	ERRCHK(tl);
	bprintf(buf, "%sVRT_re_match(ctx, \v1, %s)", cp->emit, re);
	*e = vcc_expr_edit(tl, BOOL, buf, *e, NULL);
}

static void v_matchproto_(cmp_f)
cmp_acl(struct vcc *tl, struct expr **e, const struct cmps *cp)
{
	struct symbol *sym;
	char buf[256];

	vcc_NextToken(tl);
	vcc_ExpectVid(tl, "ACL");
	sym = VCC_SymbolGet(tl, SYM_ACL, SYMTAB_CREATE, XREF_REF);
	ERRCHK(tl);
	AN(sym);
	VCC_GlobalSymbol(sym, ACL, ACL_SYMBOL_PREFIX);
	bprintf(buf, "%sVRT_acl_match(ctx, %s, \v1)", cp->emit, sym->rname);
	*e = vcc_expr_edit(tl, BOOL, buf, *e, NULL);
}

static void v_matchproto_(cmp_f)
cmp_string(struct vcc *tl, struct expr **e, const struct cmps *cp)
{
	struct expr *e2;
	struct token *tk;
	char buf[128];

	tk = tl->t;
	vcc_NextToken(tl);
	vcc_expr_add(tl, &e2, STRINGS);
	ERRCHK(tl);
	if (e2->fmt != STRINGS) {
		VSB_printf(tl->sb,
		    "Comparison of different types: %s '%.*s' %s\n",
		    vcc_utype((*e)->fmt), PF(tk), vcc_utype(e2->fmt));
		vcc_ErrWhere(tl, tk);
	} else if ((*e)->nstr == 1 && e2->nstr == 1) {
		bprintf(buf, "(%s VRT_strcmp(\v1, \v2))", cp->emit);
		*e = vcc_expr_edit(tl, BOOL, buf, *e, e2);
	} else {
		bprintf(buf, "(%s VRT_CompareStrands(\vT, \vt))", cp->emit);
		*e = vcc_expr_edit(tl, BOOL, buf, *e, e2);
	}
}

#define IDENT_REL(typ)							\
	{typ,		T_EQ,		cmp_simple, "(\v1 == \v2)" },	\
	{typ,		T_NEQ,		cmp_simple, "(\v1 != \v2)" }

#define NUM_REL(typ)							\
	IDENT_REL(typ),							\
	{typ,		T_LEQ,		cmp_simple, "(\v1 <= \v2)" },	\
	{typ,		T_GEQ,		cmp_simple, "(\v1 >= \v2)" },	\
	{typ,		'<',		cmp_simple, "(\v1 < \v2)" },	\
	{typ,		'>',		cmp_simple, "(\v1 > \v2)" }

static const struct cmps vcc_cmps[] = {
	NUM_REL(INT),
	NUM_REL(DURATION),
	NUM_REL(BYTES),
	NUM_REL(REAL),
	NUM_REL(TIME),
	IDENT_REL(BACKEND),
	IDENT_REL(ACL),
	IDENT_REL(PROBE),
	IDENT_REL(STEVEDORE),
	IDENT_REL(SUB),
	IDENT_REL(INSTANCE),

	{IP,		T_EQ,		cmp_simple, "!VRT_ipcmp(\v1, \v2)" },
	{IP,		T_NEQ,		cmp_simple, "VRT_ipcmp(\v1, \v2)" },

	{IP,		'~',		cmp_acl, "" },
	{IP,		T_NOMATCH,	cmp_acl, "!" },

	{STRINGS,	T_EQ,		cmp_string, "0 =="},
	{STRINGS,	T_NEQ,		cmp_string, "0 !="},
	{STRINGS,	'<',		cmp_string, "0 > "},
	{STRINGS,	'>',		cmp_string, "0 < "},
	{STRINGS,	T_LEQ,		cmp_string, "0 >="},
	{STRINGS,	T_GEQ,		cmp_string, "0 <="},

	{STRINGS,	'~',		cmp_regexp, "" },
	{STRINGS,	T_NOMATCH,	cmp_regexp, "!" },

	{VOID,		0,		NULL, NULL}
};

#undef IDENT_REL
#undef NUM_REL

static void
vcc_expr_cmp(struct vcc *tl, struct expr **e, vcc_type_t fmt)
{
	const struct cmps *cp;
	struct token *tk;

	*e = NULL;
	vcc_expr_add(tl, e, fmt);
	ERRCHK(tl);
	tk = tl->t;

	if ((*e)->fmt == BOOL)
		return;

	for (cp = vcc_cmps; cp->fmt != VOID; cp++) {
		if ((*e)->fmt == cp->fmt && tl->t->tok == cp->token) {
			AN(cp->func);
			cp->func(tl, e, cp);
			return;
		}
	}

	switch (tk->tok) {
	case T_EQ:
	case T_NEQ:
	case '<':
	case T_LEQ:
	case '>':
	case T_GEQ:
	case '~':
	case T_NOMATCH:
		VSB_printf(tl->sb, "Operator %.*s not possible on %s\n",
		    PF(tl->t), vcc_utype((*e)->fmt));
		vcc_ErrWhere(tl, tl->t);
		return;
	default:
		break;
	}
	if (fmt != BOOL)
		return;
	if ((*e)->fmt == BACKEND || (*e)->fmt == INT)
		*e = vcc_expr_edit(tl, BOOL, "(\v1 != 0)", *e, NULL);
	else if ((*e)->fmt == DURATION)
		*e = vcc_expr_edit(tl, BOOL, "(\v1 > 0)", *e, NULL);
	else if ((*e)->fmt == STRINGS)
		*e = vcc_expr_edit(tl, BOOL, "(\vS != 0)", *e, NULL);
	else
		INCOMPL();
}

/*--------------------------------------------------------------------
 * SYNTAX:
 *    ExprNot:
 *      '!' ExprCmp
 */

static void
vcc_expr_not(struct vcc *tl, struct expr **e, vcc_type_t fmt)
{
	struct expr *e2;
	struct token *tk;

	*e = NULL;
	if (fmt != BOOL || tl->t->tok != '!') {
		vcc_expr_cmp(tl, e, fmt);
		return;
	}

	vcc_NextToken(tl);
	tk = tl->t;
	vcc_expr_cmp(tl, &e2, fmt);
	ERRCHK(tl);
	if (e2->fmt != BOOL) {
		VSB_printf(tl->sb, "'!' must be followed by BOOL, found ");
		VSB_printf(tl->sb, "%s.\n", vcc_utype(e2->fmt));
		vcc_ErrWhere2(tl, tk, tl->t);
	} else {
		*e = vcc_expr_edit(tl, BOOL, "!(\v1)", e2, NULL);
	}
}

/*--------------------------------------------------------------------
 * SYNTAX:
 *    ExprCand:
 *      ExprNot { '&&' ExprNot } *
 */

static void
vcc_expr_cand(struct vcc *tl, struct expr **e, vcc_type_t fmt)
{
	struct expr *e2;
	struct token *tk;

	*e = NULL;
	vcc_expr_not(tl, e, fmt);
	ERRCHK(tl);
	if ((*e)->fmt != BOOL || tl->t->tok != T_CAND)
		return;
	*e = vcc_expr_edit(tl, BOOL, "(\v+\n\v1", *e, NULL);
	while (tl->t->tok == T_CAND) {
		vcc_NextToken(tl);
		tk = tl->t;
		vcc_expr_not(tl, &e2, fmt);
		ERRCHK(tl);
		if (e2->fmt != BOOL) {
			VSB_printf(tl->sb,
			    "'&&' must be followed by BOOL,"
			    " found %s.\n", vcc_utype(e2->fmt));
			vcc_ErrWhere2(tl, tk, tl->t);
			return;
		}
		*e = vcc_expr_edit(tl, BOOL, "\v1\v-\n&&\v+\n\v2", *e, e2);
	}
	*e = vcc_expr_edit(tl, BOOL, "\v1\v-\n)", *e, NULL);
}

/*--------------------------------------------------------------------
 * SYNTAX:
 *    ExprCOR:
 *      ExprCand { '||' ExprCand } *
 */

static void
vcc_expr_cor(struct vcc *tl, struct expr **e, vcc_type_t fmt)
{
	struct expr *e2;
	struct token *tk;

	*e = NULL;
	vcc_expr_cand(tl, e, fmt);
	ERRCHK(tl);
	if ((*e)->fmt != BOOL || tl->t->tok != T_COR)
		return;
	*e = vcc_expr_edit(tl, BOOL, "(\v+\n\v1", *e, NULL);
	while (tl->t->tok == T_COR) {
		vcc_NextToken(tl);
		tk = tl->t;
		vcc_expr_cand(tl, &e2, fmt);
		ERRCHK(tl);
		if (e2->fmt != BOOL) {
			VSB_printf(tl->sb,
			    "'||' must be followed by BOOL,"
			    " found %s.\n", vcc_utype(e2->fmt));
			vcc_ErrWhere2(tl, tk, tl->t);
			return;
		}
		*e = vcc_expr_edit(tl, BOOL, "\v1\v-\n||\v+\n\v2", *e, e2);
	}
	*e = vcc_expr_edit(tl, BOOL, "\v1\v-\n)", *e, NULL);
}

/*--------------------------------------------------------------------
 * This function is the entry-point for getting an expression with
 * a particular type, ready for inclusion in the VGC.
 */

static void
vcc_expr0(struct vcc *tl, struct expr **e, vcc_type_t fmt)
{
	struct token *t1;

	assert(fmt != VOID);
	assert(fmt != STRINGS);
	*e = NULL;
	t1 = tl->t;
	if (fmt == STRING_LIST || fmt == STRING)
		vcc_expr_cor(tl, e, STRINGS);
	else
		vcc_expr_cor(tl, e, fmt);
	ERRCHK(tl);
	assert((*e)->fmt != STRING_LIST && (*e)->fmt != STRING);

	if ((*e)->fmt == STRINGS && fmt == STRING_LIST)
		(*e)->fmt = STRING_LIST;
	else if ((*e)->fmt == STRINGS && fmt == STRING)
		*e = vcc_expr_edit(tl, STRING, "\vS", *e, NULL);
	else if ((*e)->fmt == STRINGS && fmt == STRANDS) {
		*e = vcc_expr_edit(tl, STRANDS, "\vT", (*e), NULL);
	} else if ((*e)->fmt != STRINGS &&
	    (fmt == STRING || fmt == STRING_LIST))
		vcc_expr_tostring(tl, e, fmt);

	if ((*e)->fmt == STRING_LIST)
		*e = vcc_expr_edit(tl, STRING_LIST,
		    "\v+\n\v1,\nvrt_magic_string_end\v-", *e, NULL);

	if (fmt != (*e)->fmt)  {
		VSB_printf(tl->sb, "Expression has type %s, expected %s\n",
		    vcc_utype((*e)->fmt), vcc_utype(fmt));
		vcc_ErrWhere2(tl, t1, tl->t);
		return;
	}
}

/*--------------------------------------------------------------------
 * This function parses and emits the C-code to evaluate an expression
 *
 * We know up front what kind of type we want the expression to be,
 * and this function is the backstop if that doesn't succeed.
 */

void
vcc_Expr(struct vcc *tl, vcc_type_t fmt)
{
	struct expr *e = NULL;

	assert(fmt != VOID);
	assert(fmt != STRINGS);
	vcc_expr0(tl, &e, fmt);
	ERRCHK(tl);
	vcc_expr_fmt(tl->fb, tl->indent, e);
	VSB_printf(tl->fb, "\n");
	vcc_delete_expr(e);
}

/*--------------------------------------------------------------------
 */

void v_matchproto_(sym_act_f)
vcc_Act_Call(struct vcc *tl, struct token *t, struct symbol *sym)
{

	struct expr *e;

	e = NULL;
	vcc_Eval_SymFunc(tl, &e, t, sym, VOID);
	if (!tl->err) {
		vcc_expr_fmt(tl->fb, tl->indent, e);
		SkipToken(tl, ';');
		VSB_cat(tl->fb, ";\n");
	} else if (t != tl->t) {
		vcc_ErrWhere2(tl, t, tl->t);
	}
	vcc_delete_expr(e);
}

/*--------------------------------------------------------------------
 */

void
vcc_Expr_Init(struct vcc *tl)
{
	struct symbol *sym;

	sym = VCC_MkSym(tl, "regsub", SYM_FUNC);
	AN(sym);
	sym->type = STRING;
	sym->eval = vcc_Eval_Regsub;
	sym->eval_priv = NULL;

	sym = VCC_MkSym(tl, "regsuball", SYM_FUNC);
	AN(sym);
	sym->type = STRING;
	sym->eval = vcc_Eval_Regsub;
	sym->eval_priv = sym;

	sym = VCC_MkSym(tl, "true", SYM_FUNC);
	AN(sym);
	sym->type = BOOL;
	sym->eval = vcc_Eval_BoolConst;
	sym->eval_priv = sym;

	sym = VCC_MkSym(tl, "false", SYM_FUNC);
	AN(sym);
	sym->type = BOOL;
	sym->eval = vcc_Eval_BoolConst;
	sym->eval_priv = NULL;
}
