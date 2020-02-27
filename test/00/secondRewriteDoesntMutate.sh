export LD_LIBRARY_PATH=$HOME/usr/lib:/usr/local/lib:$LD_LIBRARY_PATH
EXIT=0

here=`pwd`
mkdir /tmp/$$
cd /tmp/$$

for i in $here/examples/*.mky; do
    if [ $i = "$here/examples/EndogenousMoney.mky" ]; then continue; fi 
    "$here/gui-tk/minsky" "$here/test/rewriteMky.tcl" "$i"  tmp;
    "$here/gui-tk/minsky" "$here/test/rewriteMky.tcl" tmp  tmp1;
    "$here/gui-tk/minsky" "$here/test/rewriteMky.tcl" tmp1  tmp2;
    if test $? -ne 0; then EXIT=1; break; fi
    # check second rewrite doesn't mutate
    
    $here/test/cmpFp tmp1 tmp2
    if [ $? -ne 0 ]; then
        echo "$i mutates"
        EXIT=1
    fi
    # sanity check that the first exported schema is OK
    $here/test/checkSchemasAreSame "$i" tmp
    if [ $? -ne 0 ]; then
        echo "$i fails schema rewrite"
        EXIT=1
    fi
done

rm -rf /tmp/$$
exit $EXIT
