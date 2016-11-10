CLANG_FORMAT="/c/Progra~2/LLVM/bin/clang-format.exe"
$CLANG_FORMAT --version

files_to_lint="$(find ./ -name '*.cpp' -or -name '*.h')"

for f in $files_to_lint; do
    d=$(diff -u "$f" <($CLANG_FORMAT "$f") || true)
    if ! [ -z "$d" ]; then
        echo "!!! $f not compliant to coding style, here is the fix:"
        echo "$d"
        fail=1
    fi
done

