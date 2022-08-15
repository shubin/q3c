mkdir -p ./include
mkdir -p ./bin

for f in *.vert.glsl; do
	n=$(basename -s .glsl $f)
	n=${n/./_}
	echo $n
	n=$n
	glslangValidator -V $f -o shader_${n}_spirv
	xxd -i shader_${n}_spirv > include/shader_${n}.c
	rm shader_${n}_spirv
done

for f in *.frag.glsl; do
	n=$(basename -s .glsl $f)
	n=${n/./_}
	echo $n
	n=$n
	glslangValidator -V $f -o shader_${n}_spirv
	xxd -i shader_${n}_spirv > include/shader_$n.c
	rm shader_${n}_spirv
done

