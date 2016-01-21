#!/usr/bin/env bash

set -eu
set -o pipefail

if ! which clang-format >/dev/null 2>&1; then
    echo clang-format is not installed
    echo Source code format not checked.
    exit 0
fi

if ! git diff >/dev/null; then
    echo The code is not part of git repo.
    echo Source code format not checked.
fi

if git diff 2>/dev/null | grep -qc .; then
    echo Git shows uncommitted changes. Fix that first.
    git status -sbuno | grep '^ M'
    echo Source code format not checked.
    exit 1
fi

clang-format -i `ls *.[ch] | grep -v "^tcpkali_expr_[yl].[ch]$"`

if git diff 2>/dev/null | grep -qc .; then
    echo clang-format has found formatting errors.
    git status -sbuno | grep '^ M'
    git checkout .
    echo "Run \`make reformat\` to format the source code."
    exit 1
fi

echo "Code formatting is OK"
