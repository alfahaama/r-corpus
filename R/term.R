#  Copyright 2017 Patrick O. Perry.
#
#  Licensed under the Apache License, Version 2.0 (the "License");
#  you may not use this file except in compliance with the License.
#  You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
#  Unless required by applicable law or agreed to in writing, software
#  distributed under the License is distributed on an "AS IS" BASIS,
#  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#  See the License for the specific language governing permissions and
#  limitations under the License.


term_counts <- function(x, filter = token_filter(), weights = NULL,
                        ngrams = NULL, min = NA, max = NA,
                        limit = NA, types = FALSE)
{
    x <- as_text(x)
    filter <- as_token_filter(filter)
    weights <- as_weights(weights, length(x))
    ngrams <- as_ngrams(ngrams)
    min <- as_min(min)
    max <- as_max(max)
    limit <- as_limit(limit)

    if (!(is.logical(types) && length(types) == 1 && !is.na(types))) {
        stop("'types' should be TRUE or FALSE")
    }
    types <- as.logical(types)

    ans <- .Call(C_term_counts_text, x, filter, weights, ngrams, min, max,
                 types)

    # order descending by count, then ascending by term
    o <- order(-ans$count, ans$term)

    # limit output if desired
    if (!is.na(limit) && length(o) > limit) {
        o <- o[seq_len(limit)]
    }

    ans <- ans[o, , drop = FALSE]
    row.names(ans) <- NULL
    ans
}


term_matrix <- function(x, filter = token_filter(), weights = NULL,
                        ngrams = NULL, select = NULL, group = NULL)
{
    x <- as_text(x)
    filter <- as_token_filter(filter)
    weights <- as_weights(weights, length(x))
    ngrams <- as_ngrams(ngrams)
    select <- as_character_vector("select", select)
    group <- as_group(group, length(x))

    if (is.null(group)) {
        n <- length(x)
    } else {
        n <- nlevels(group)
    }

    mat <- .Call(C_term_matrix_text, x, filter, weights, ngrams, select, group)
    Matrix::sparseMatrix(i = mat$i, j = mat$j, x = mat$count,
                         dims = c(n, length(mat$col_names)),
                         dimnames = list(mat$row_names, mat$col_names),
                         index1 = FALSE, check = FALSE)
}
