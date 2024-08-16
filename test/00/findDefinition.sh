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

cat >input.py <<EOF
import sys
sys.path.insert(0,'$here')
from pyminsky import minsky
minsky.load('$here/test/findDefinition.mky')

for i in range(len(minsky.model.items)):
  item=minsky.model.items[i]
  if item.visible() and  item.classType().startswith("Variable:") or item.classType().startswith("VarConstant"):
    minsky.canvas.getItemAt(item.x(), item.y())
    findResult=minsky.canvas.findVariableDefinition()
    foundItem=minsky.canvas.itemIndicator()
    if item.name()=="undef": assert not findResult,"undef" 
    if item.name() in ["param1","3"]: assert item.id()==foundItem.id(), "paramConst" 
    if item.name()=="foo":
      assert item.id()==foundItem.id()
    if item.name()=="int": assert foundItem.classType()=="IntOp", "IntOp"
    if item.name()=="bar": 
      assert findResult
      assert foundItem.classType()=="GodleyIcon", "GodleyIcon"
EOF

python3 input.py
if [ $? -ne 0 ]; then fail; fi

pass
