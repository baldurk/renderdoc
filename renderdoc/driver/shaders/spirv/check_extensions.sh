#!/bin/bash

# not sorted because the registry lists them in extension number order
grep -o '\[SPV_.*\]' spirv_registry.md | tr -d '[] \t' > all_exts.txt
unix2dos -q all_exts.txt

# sorted by enum value
sed -n '/kind.*"Capability",$/,/"category"/p' spirv.core.grammar.json |
	grep 'enumerant\>\|value' | paste -sd ' \n' | awk '{print $6" "$3}' |
	tr -d '",' | sort -n > all_capabilities.txt
unix2dos -q all_capabilities.txt

export IFS="
"
for I in $(cat all_exts.txt | dos2unix); do
	if ! grep -q '`'"$I"'`' extension_support.md; then
		echo "Extension $I isn't in extension_support.md";
	fi
done

for I in $(cat all_capabilities.txt | awk '{print $2}'); do
	if ! grep -q '`'"$I"'`' extension_support.md; then
		echo "Capability $I isn't in extension_support.md";
	fi
done

for I in $(rg SPV_ extension_support.md | egrep -o 'SPV_[A-Z0-9a-z_]*'); do
	if ! grep -q $I spirv_registry.md; then
		echo "Extension $I is in extension_support.md but not the registry";
	fi;
done

for I in $(egrep '^  ' extension_support.md | tr -d '* `'); do
	if ! egrep -q "\"enumerant\".*\"$I\"" spirv.core.grammar.json; then
		if ! egrep -q "\"aliases\".*\"$I\"" spirv.core.grammar.json; then
			echo "Capability $I is in extension_support.md but not the grammar";
		fi;
	fi;
done
