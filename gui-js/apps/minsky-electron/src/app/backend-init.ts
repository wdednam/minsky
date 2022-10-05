import {CppClass, importCSVerrorMessage, Utility, version } from '@minsky/shared';
import { WindowManager } from './managers/WindowManager';
import { dialog, shell } from 'electron';
import * as JSON5 from 'json5';
import * as elog from 'electron-log';

const log=elog? elog: console;
if (!Utility.isDevelopmentMode()) { //clobber logging in production
  log.info=function(...args: any[]){};
}

const addonDir = Utility.isPackaged()
      ? '../node-addons'
      : '../../node-addons';
/** REST Service addon */
export var restService = null;
try {
  restService = require('bindings')(addonDir+'/minskyRESTService.node');
} catch (error) {
  log.error(error);
}

/** returns true if RESTService call is logged in development mode */
function logFilter(c: string) {
  const logFilter=["mouseMove$", "requestRedraw$"];
  for (var i in logFilter)
    if (c.match(logFilter[i])) return false;
  return true;
}

/** core function to call into C++ object heirarachy */
CppClass.backend=function backend(command: string, ...args: any[]) {
  if (!command) {
    log.error('backend called without any command');
    return {};
  }
  if (!restService) {
    log.error('Rest Service not ready');
    return {};
  }
  try {
    var arg='';
    if (args.length>0)
      arg=JSON5.stringify(args);
    const response = restService.call(command, arg);
    if (logFilter(command))
      log.info('Rest API: ',command,arg,"=>",response);
    return JSON5.parse(response);
  } catch (error) {
    log.error('Rest API: ',command,arg,'=>Exception caught: ' + error?.message);
    if (!dialog) throw error; // rethrow to force error in jest environment
    if (command === '/minsky/canvas/item/importFromCSV') {
      return importCSVerrorMessage;
    } else {
      if (error?.message)
        dialog.showMessageBoxSync(WindowManager.getMainWindow(),{
          message: error.message,
          type: 'error',
        });
      return error?.message;
    }
  }
}

if ("JEST_WORKER_ID" in process.env) {
  restService.setMessageCallback(function (msg: string, buttons: string[]) {
    log.info(msg);
  });
  restService.setBusyCursorCallback(function (busy: boolean) {});
} else {
  restService.setMessageCallback(function (msg: string, buttons: string[]) {
    if (msg && dialog)
      return dialog.showMessageBoxSync(WindowManager.getMainWindow(),{
        message: msg,
        type: 'info',
        buttons: buttons,
      });
    return 0;
  });

  restService.setBusyCursorCallback(function (busy: boolean) {
    WindowManager.getMainWindow()?.webContents?.insertCSS(
      busy ? 'html body {cursor: wait}' : 'html body {cursor: default}'
    );
  });
}

// Sanity checks before we get started
if (CppClass.backend("/minsky/minskyVersion")!=version)
  setTimeout(()=>{
    dialog.showMessageBoxSync({
      message: "Mismatch of front end and back end versions",
      type: 'warning',
    });
  },1000);

if (CppClass.backend("/minsky/ravelExpired"))
  setTimeout(()=>{
    const button=dialog.showMessageBoxSync({
      message: "Your Ravel license has expired",
      type: 'warning',
      buttons: ["OK","Upgrade"],
    });
    if (button==1)
      shell.openExternal("https://ravelation.hpcoders.com.au");
  },1000);

