web: cd bin && ./main
webmemcheck: cd bin && sudo valgrind -v --track-origins=yes --leak-check=full --show-leak-kinds=all ./main
tests: sh runtests.sh
testmemcheck: sh runtests-valgrind.sh
