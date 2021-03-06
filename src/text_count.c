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

#include <stddef.h>
#include <stdlib.h>
#include "corpus/src/table.h"
#include "corpus/src/text.h"
#include "corpus/src/textset.h"
#include "corpus/src/tree.h"
#include "corpus/src/typemap.h"
#include "corpus/src/symtab.h"
#include "corpus/src/sentscan.h"
#include "corpus/src/wordscan.h"
#include "corpus/src/filter.h"
#include "corpus/src/sentfilter.h"
#include "rcorpus.h"

// the R 'error' is a #define (to Rf_error) that clashes with the 'error'
// member of struct corpus_sentfilter
#ifdef error
#  undef error
#endif


#define BAIL(msg) \
	do { \
		corpus_sentfilter_destroy(&filter); \
		Rf_error(msg); \
	} while (0)


SEXP text_count_sentences(SEXP sx, SEXP scrlf_break, SEXP ssuppress)
{
	SEXP ans;
	struct corpus_sentfilter filter;
	const struct corpus_text *text, *suppress;
	R_xlen_t i, n, isupp, nsupp, nunit;
	int flags, nprot, err;

	nprot = 0;

	// x
	PROTECT(sx = coerce_text(sx)); nprot++;
	text = as_text(sx, &n);

	// crlf_break
	PROTECT(scrlf_break = coerceVector(scrlf_break, LGLSXP)); nprot++;
	if (LOGICAL(scrlf_break)[0] == TRUE) {
		flags = CORPUS_SENTSCAN_STRICT;
	} else {
		flags = CORPUS_SENTSCAN_SPCRLF;
	}

	if ((err = corpus_sentfilter_init(&filter, flags))) {
		Rf_error("memory allocation failure");
	}

	// suppress
	if (ssuppress != R_NilValue) {
		PROTECT(ssuppress = coerce_text(ssuppress)); nprot++;
		suppress = as_text(ssuppress, &nsupp);

		for (isupp = 0; isupp < nsupp; isupp++) {
			if (!suppress[isupp].ptr) {
				continue;
			}

			err = corpus_sentfilter_suppress(&filter,
							 &suppress[isupp]);
			if (err) {
				BAIL("failed adding break suppression"
				     " to sentence filter");
			}
		}
	}

	PROTECT(ans = allocVector(REALSXP, n)); nprot++;
	setAttrib(ans, R_NamesSymbol, names_text(sx));

	for (i = 0; i < n; i++) {
		if (!text[i].ptr) { // missing value
			REAL(ans)[i] = NA_REAL;
			continue;
		}

		if (CORPUS_TEXT_SIZE(&text[i]) == 0) { // empty text
			REAL(ans)[i] = 0;
			continue;
		}

		if ((err = corpus_sentfilter_start(&filter, &text[i]))) {
			BAIL("memory allocation failure");
		}

		nunit = 0;
		while (corpus_sentfilter_advance(&filter)) {
			nunit++;
		}
		if (filter.error) {
			BAIL("memory allocation failure");
		}

		REAL(ans)[i] = (double)nunit;
	}

	corpus_sentfilter_destroy(&filter);
	UNPROTECT(nprot);
	return ans;
}


#undef BAIL
#define BAIL(msg) \
	do { \
		Rf_error(msg); \
	} while (0)


SEXP text_count_tokens(SEXP sx, SEXP sfilter)
{
	SEXP ans;
	struct corpus_filter *filter;
	const struct corpus_text *text;
	R_xlen_t i, n, nunit;
	int nprot, err;

	nprot = 0;

	// x
	PROTECT(sx = coerce_text(sx)); nprot++;
	text = as_text(sx, &n);

	// filter
	PROTECT(sfilter = alloc_filter(sfilter)); nprot++;
	filter = as_filter(sfilter);

	PROTECT(ans = allocVector(REALSXP, n)); nprot++;
	setAttrib(ans, R_NamesSymbol, names_text(sx));

	for (i = 0; i < n; i++) {
		if (!text[i].ptr) { // missing value
			REAL(ans)[i] = NA_REAL;
			continue;
		}

		if (CORPUS_TEXT_SIZE(&text[i]) == 0) { // empty text
			REAL(ans)[i] = 0;
			continue;
		}

		if ((err = corpus_filter_start(filter, &text[i],
					       CORPUS_FILTER_SCAN_TOKENS))) {
			BAIL("memory allocation failure");
		}

		nunit = 0;

		while (corpus_filter_advance(filter)) {
			if (filter->type_id < 0) {
				// skip ignored and dropped tokens
				continue;
			}
			nunit++;
		}

		if (filter->error) {
			BAIL("memory allocation failure");
		}

		REAL(ans)[i] = (double)nunit;
	}

	UNPROTECT(nprot);
	return ans;
}
