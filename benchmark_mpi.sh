ITERATIONS=$1
SEPARATOR=$2
FILENAME=$3
NBCORES=$4
MAX=$((ITERATIONS+0))

RESULT_CSV=""
cd build

for i in $(seq 1 $MAX)
do
    VALUE="$(mpirun.mpich -np 1 ./mpimnt $FILENAME $NBCORES 000 | tail -1 | cut -d ']' -f2 | sed 's/^[ ]*//' | cut -d ' ' -f1)"
    RESULT_CSV="$RESULT_CSV$SEPARATOR$VALUE"
done

echo "${RESULT_CSV:1}"