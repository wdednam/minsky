#! /bin/sh

here=`pwd`
if test $? -ne 0; then exit 2; fi
tmp=/tmp/$$
mkdir $tmp
if test $? -ne 0; then exit 2; fi
cd $tmp
if test $? -ne 0; then exit 2; fi

fail()
{
    echo "FAILED" 1>&2
    cd $here
    chmod -R u+w $tmp
    rm -rf $tmp
    exit 1
}

pass()
{
    echo "PASSED" 1>&2
    cd $here
    chmod -R u+w $tmp
    rm -rf $tmp
    exit 0
}

trap "fail" 1 2 3 15

cat >input.tcl <<EOF
source $here/test/assert.tcl
proc afterMinskyStarted {} {
  minsky.addVariable par parameter
  minsky.findObject Variable::parameter
  CSVImportDialog
  getValue :par
  minsky.value.csvDialog.url https://sourceforge.net/p/minsky/ravel/20/attachment/BIS_GDP.csv
  minsky.value.csvDialog.loadFile
  destroy .wiring.editVar
  assert {![winfo ismapped .wiring.context]}
  
  after 100 {tcl_exit}
}
EOF

$here/gui-tk/minsky input.tcl
if [ $? -ne 0 ]; then fail; fi

pass
