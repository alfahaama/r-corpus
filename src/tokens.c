/*
 * Copyright 2017 Patrick O. Perry.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <stdlib.h>
#include <string.h>
#include "corpus/src/table.h"
#include "corpus/src/text.h"
#include "corpus/src/textset.h"
#include "corpus/src/tree.h"
#include "corpus/src/typemap.h"
#include "corpus/src/symtab.h"
#include "corpus/src/wordscan.h"
#include "corpus/src/filter.h"
#include "rcorpus.h"

// the R 'error' is a #define (to Rf_error) that clashes with the 'error'
// member of struct corpus_filter
#ifdef error
#  undef error
#endif


struct tokens {
	struct corpus_filter *filter;

	int *buf;
	int nbuf, nbuf_max;

	SEXP *types;
	int ntype, ntype_max;

	int nprot;
};


static void tokens_init(struct tokens *ctx, struct corpus_filter *filter);
static void tokens_destroy(struct tokens *ctx);
static void tokens_add(struct tokens *ctx, int type_id);
static SEXP tokens_scan(struct tokens *ctx, const struct corpus_text *text);


void tokens_init(struct tokens *ctx, struct corpus_filter *filter)
{
	int i, n;

	ctx->filter = filter;

	ctx->nbuf_max = 256;
	ctx->buf = (void *)R_alloc(ctx->nbuf_max, sizeof(*ctx->buf));

	ctx->ntype_max = 256;
	ctx->ntype = 0;
	ctx->types = (void *)R_alloc(ctx->ntype_max, sizeof(*ctx->types));

	// add the terms in the filter
	n = ctx->filter->nterm;
	for (i = 0; i < n; i++) {
		tokens_add(ctx, filter->type_ids[i]);
	}

	ctx->nprot = n;
}


void tokens_destroy(struct tokens *ctx)
{
	UNPROTECT(ctx->nprot);
}


void tokens_add(struct tokens *ctx, int type_id)
{
	const struct corpus_text *type;

	if (ctx->ntype == ctx->ntype_max) {
		ctx->ntype_max = 2 * ctx->ntype_max;
		ctx->types = (void *)S_realloc((void *)ctx->types,
					       ctx->ntype_max,
					       ctx->ntype,
					       sizeof(*ctx->types));
	}

	type = &ctx->filter->symtab.types[type_id].text;
	ctx->types[ctx->ntype] = mkCharLenCE((char *)type->ptr,
					     CORPUS_TEXT_SIZE(type),
					     CE_UTF8);
	PROTECT(ctx->types[ctx->ntype]);
	ctx->ntype++;
}


SEXP tokens_scan(struct tokens *ctx, const struct corpus_text *text)
{
	SEXP ans;
	int nadd, type_id, term_id, nterm;
	int err, i, ntok;

	if (!text->ptr) {
		return ScalarString(NA_STRING);
	}

	if ((err = corpus_filter_start(ctx->filter, text))) {
		Rf_error("error while tokenizing text");
	}

	nterm = ctx->filter->nterm;
	nadd = 0;
	ntok = 0;

	while (corpus_filter_advance(ctx->filter, &term_id)) {
		// add the new terms
		while (nterm < ctx->filter->nterm) {
			type_id = ctx->filter->type_ids[nterm];
			tokens_add(ctx, type_id);
			nterm++;
			nadd++;
		}

		if (ntok == ctx->nbuf_max) {
			ctx->nbuf_max = 2 * ctx->nbuf_max;
			ctx->buf = (void *)S_realloc((void *)ctx->buf,
						     ctx->nbuf_max,
						     ntok, sizeof(*ctx->buf));
		}
		ctx->buf[ntok] = term_id;
		ntok++;
	}

	if (ctx->filter->error) {
		Rf_error("error while tokenizing text");
	}

	PROTECT(ans = allocVector(STRSXP, ntok));
	for (i = 0; i < ntok; i++) {
		term_id =  ctx->buf[i];
		if (term_id >= 0) {
			SET_STRING_ELT(ans, i, ctx->types[term_id]);
		} else {
			SET_STRING_ELT(ans, i, NA_STRING);
		}
	}

	// no need to protect the new terms any more; ans protects them
	UNPROTECT(nadd + 1);

	return ans;
}


SEXP tokens_text(SEXP sx, SEXP sprops)
{
	SEXP ans, names, stext, sfilter;
	const struct corpus_text *text;
	struct corpus_filter *filter;
	struct tokens ctx;
	R_xlen_t i, n;

	PROTECT(stext = coerce_text(sx));
	PROTECT(sfilter = alloc_filter(sprops));

	text = as_text(stext, &n);
	filter = as_filter(sfilter);

	PROTECT(ans = allocVector(VECSXP, n));
	names = names_text(stext);
	setAttrib(ans, R_NamesSymbol, names);

	tokens_init(&ctx, filter);

	for (i = 0; i < n; i++) {
		SET_VECTOR_ELT(ans, i, tokens_scan(&ctx, &text[i]));
	}

	tokens_destroy(&ctx);

	UNPROTECT(3);
	return ans;
}