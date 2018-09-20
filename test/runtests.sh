echo "Running unit tests:"

for i in test/*_test
do
    if test -f $i
    then
        if $VALGRIND ./$i 2>> log/test.log
        then
            echo $i PASS
        else
            echo "ERROR in test $i: see log/test.log"
            echo "------"
            tail test/test.log
            exit 1
        fi
    fi
done

echo ""
