import {describe, expect, test} from '@jest/globals';
import {
  CppClass,
   minsky,
   GodleyIcon,
   PlotWidget,
  VariableBase,
 } from './index';
import * as fs from 'fs';
import * as JSON5 from 'json5';

const restService = require('bindings')('../../node-addons/minskyRESTService.node');

// replace backend with a synchronous call
CppClass.backend=(command: string, ...args: any[])=>{
  var arg='';
  if (args.length>1) {
    arg=JSON5.stringify(args, {quote: '"'});
  } else if (args.length===1) {
    arg=JSON5.stringify(args[0], {quote: '"'});
  }
  console.log(command);
  return JSON5.parse(restService.call(`${command}/$sync`, arg));
};

describe('dummy',()=>{
  test('dummy',()=>{
    expect(true).toBe(true);
  });
});

var tmpDir;
beforeAll(()=>{
  tmpDir=fs.mkdtempSync("minsky-jest");
});
afterAll(()=>{
  fs.rmSync(tmpDir,{recursive: true});
});
           
beforeEach(()=>{
  minsky.clearAllMaps();
});

describe('Minsky tests', ()=>{
  test('save empty & load',()=>{
    minsky.save(tmpDir+"/foo.mky");
    minsky.load(tmpDir+"/foo.mky");
  });
 
  test('name item', async ()=>{
     minsky.canvas.addOperation("time");
     minsky.canvas.getItemAt(0,0);
     minsky.nameCurrentItem("foo");
     console.log(JSON5.stringify(await minsky.namedItems.elem("foo")));
     expect(await minsky.namedItems.elem("foo").second.classType()).toBe("Operation:time");
     expect(await minsky.namedItems.size()).toBe(1);
     minsky.namedItems.insert("fooBar",await minsky.canvas.item.$properties());
     expect(await minsky.namedItems.size()).toBe(2);
     // TODO erase doesn't quite work
     minsky.namedItems.erase("foo");
     expect(await minsky.namedItems.size()).toBe(1);
   });
 
  test('addIntegral', async ()=>{
     minsky.clearAllMaps();
     expect(await minsky.model.items.size()).toBe(0);
     minsky.canvas.addVariable("foo","flow");
     expect(await minsky.model.items.size()).toBe(1);
     minsky.canvas.getItemAt(0,0);
     expect(await minsky.canvas.item.classType()).toBe("Variable:flow");
     expect(await (new VariableBase(minsky.canvas.item)).name()).toBe("foo");
     minsky.addIntegral();
     expect(await minsky.model.items.size()).toBe(2);
     expect(await minsky.model.items.elem(1).classType()).toBe("IntOp");
   });
 
   test('operations', async ()=>{
     expect(await minsky.availableOperations()).toEqual(expect.arrayContaining(["time"]));
     expect(await minsky.classifyOp("time")).toBe("general");
   });
 
   test('edited', async ()=>{
     minsky.clearAllMaps();
     expect(await minsky.edited()).toBe(false);
     minsky.canvas.addOperation("time");
     expect(await minsky.edited()).toBe(true);
     minsky.clearAllMaps();
     expect(await minsky.edited()).toBe(false);
   });
 
  test('copy/cut/paste',async ()=>{
    minsky.load("../examples/GoodwinLinear02.mky");
    minsky.canvas.selection.clear();
    minsky.copy();
    expect(await minsky.clipboardEmpty()).toBe(true);
    minsky.canvas.mouseDown(0,0);
    minsky.canvas.mouseUp(200,200);
    minsky.copy();
    expect(await minsky.canvas.selection.empty()).toBe(false);
    expect(await minsky.clipboardEmpty()).toBe(false);
    let numItems=await minsky.model.items.size();
    let numSelected=await minsky.canvas.selection.items.size();
    expect(numItems).toBeGreaterThan(0);
    expect(numSelected).toBeGreaterThan(0);
    minsky.cut();
    expect(await minsky.model.items.size()+numSelected).toBe(numItems);
    minsky.paste();
    expect(await minsky.model.items.size()).toBe(numItems);
   });
   
   test('undo',async ()=>{
     minsky.clearAllMaps();
     expect(await minsky.undo(0)).toBe(1);
     minsky.canvas.addOperation("time");
     expect(await minsky.undo(0)).toBe(2);
     expect(await minsky.model.items.size()).toBe(1);
     minsky.canvas.addOperation("time");
     expect(await minsky.undo(0)).toBe(3);
     expect(await minsky.model.items.size()).toBe(2);
     expect(await minsky.undo(1)).toBe(2);
     expect(await minsky.model.items.size()).toBe(1);
     expect(await minsky.undo(-1)).toBe(3);
     expect(await minsky.model.items.size()).toBe(2);
   });
   test('doPushHistory',async ()=>{
     minsky.clearAllMaps(true);
     expect(await minsky.undo(0)).toBe(1);
     expect(await minsky.doPushHistory(false)).toBe(false);
     expect(await minsky.doPushHistory()).toBe(false);
     minsky.canvas.addOperation("time");
     minsky.canvas.addOperation("time");
     expect(await minsky.model.items.size()).toBe(2);
     expect(await minsky.undo(1)).toBe(1);
     expect(await minsky.model.items.size()).toBe(2);
     expect(await minsky.doPushHistory(true)).toBe(true);
     expect(await minsky.doPushHistory()).toBe(true);
     minsky.canvas.addOperation("time");
     expect(await minsky.undo(0)).toBe(2);
     minsky.canvas.addOperation("time");
     expect(await minsky.model.items.size()).toBe(4);
     expect(await minsky.undo(0)).toBe(3);
     expect(await minsky.undo(1)).toBe(2);
     expect(await minsky.model.items.size()).toBe(3);
   });
   test('clearHistory',async ()=>{
     minsky.canvas.addOperation("time");
     minsky.canvas.addOperation("time");
     minsky.canvas.addOperation("time");
     expect(await minsky.undo(0)).toBeGreaterThan(1);
     minsky.clearHistory();
     expect(await minsky.undo(0)).toBe(1);
   });
   test('dimensional analysis',async ()=>{
     minsky.deleteAllUnits();
     expect(await minsky.dimensionalAnalysis()).toStrictEqual({});
   });
 
   test('addGodley',async ()=>{
     minsky.canvas.addGodley();
     minsky.canvas.getItemAt(0,0);
     expect(await minsky.canvas.item.classType()).toBe("GodleyIcon");
     let godley=new GodleyIcon(minsky.canvas.item);
     godley.table.resize(3,2);
     godley.setCell(0,1,"stock");
     godley.setCell(2,1,"flow");
     godley.update();
     minsky.canvas.copyAllFlowVars();
     minsky.canvas.copyAllStockVars();
   });
   test('addPlot',async ()=>{
     minsky.canvas.addPlot();
     minsky.canvas.getItemAt(0,0);
     expect(await minsky.canvas.item.classType()).toBe("PlotWidget");
     let plot=new PlotWidget(minsky.canvas.item);
     expect(await plot.plotType()).toBe("automatic");
     expect(await plot.plotType("bar")).toBe("bar");
     minsky.canvas.copyItem();
     expect(await minsky.model.items.size()).toBe(2);
     expect(await minsky.model.items.elem(1).classType()).toBe("PlotWidget");
   });
 
   test('defaultRotation',async ()=>{
     expect(await minsky.canvas.defaultRotation(90)).toBe(90);
     expect(await minsky.canvas.defaultRotation()).toBe(90);
     expect(await minsky.canvas.defaultRotation(0)).toBe(0);
     expect(await minsky.canvas.defaultRotation()).toBe(0);
   });
   test('deleteItem',async ()=>{
     minsky.canvas.addPlot();
     expect(await minsky.model.items.size()).toBe(1);
     minsky.canvas.getItemAt(0,0);
     minsky.canvas.deleteItem();
     expect(await minsky.model.items.size()).toBe(0);
   });
   test('add/delete wire',async ()=>{
     minsky.canvas.addOperation("time");
     minsky.canvas.mouseUp(100,100); // insert op at 100,100
     minsky.canvas.addOperation("add");
     minsky.canvas.mouseUp(200,200); 
     minsky.canvas.getItemAt(100,100);
     let src=[await minsky.canvas.item.portX(0), await minsky.canvas.item.portY(0)];
     minsky.canvas.getItemAt(200,200);
     minsky.canvas.mouseDown(src[0],src[1]);
     minsky.canvas.mouseUp(await minsky.canvas.item.portX(1), await minsky.canvas.item.portY(1)); // should add a wire
     expect(await minsky.model.wires.size()).toBe(1);
     minsky.canvas.getWireAt(150,150);
     expect(await minsky.canvas.wire.$properties()).not.toBe({});
     minsky.canvas.deleteWire();
     expect(await minsky.model.wires.size()).toBe(0);
   });
});
 
