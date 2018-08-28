"use strict";
const submitRequests = require('./submit_requests');
const pathnames = require('../pathnames');

const ids = require('./ids_dom_elements');
const jsonToHtml = require('./json_to_html');
//const Block = require('../bitcoinjs_src/block');
const globals = require('./globals');
const miscellaneous = require('../miscellaneous');
const miscellaneousFrontEnd = require('./miscellaneous_frontend');
const kanbanGO = require('../resources_kanban_go');

function TestKanbanGO() {
  var inputSchnorr = ids.defaults.kanbanGO.inputSchnorr;
  this.theFunctions  = {
    testSha3 : {
      rpcCall: kanbanGO.rpcCalls.testSha3.rpcCall, 
      //<- must equal the label of the rpc call in the kanbanGO.rpcCalls data structure.
      //Setting rpcCall to null or undefined is allowed:
      //if that happens, in this.correctFunctions() 
      //we set rpcCall to the natural default - the function label.
      //If that default is not an rpc call, an error will 
      //conveniently be thrown to let you know of the matter.
      inputs: {
        message: inputSchnorr.message
      }
    },
    versionGO: {
    },
    testPrivateKeyGeneration: {
      outputs: {
        privateKeyBase58Check: inputSchnorr.privateKey
      }
    },
    testPublicKeyFromPrivate: {
      inputs: {
        privateKey: inputSchnorr.privateKey
      },
      outputs: {
        publicKeyHex: inputSchnorr.publicKey
      }
    },
    testSchnorrSignature: {
      inputs: {
        privateKey: inputSchnorr.privateKey
      },
      inputsBase64: {
        messageBase64: inputSchnorr.message
      },
      outputs: {
        signatureBase58: inputSchnorr.signature
      }
    },
    testSchnorrVerification: {
      inputs: {
        publicKey: inputSchnorr.publicKey,
        signature: inputSchnorr.signature
      },
      inputsBase64: {
        messageBase64: inputSchnorr.message
      }, 
      callback: this.callbackSchnorrVerification
    },
    
  };
  this.correctFunctions();
}

TestKanbanGO.prototype.callbackSchnorrVerification = function(functionLabel, input, output) {
  this.callbackStandard(functionLabel, input, output);
  var outputRaw = jsonToHtml.getHtmlFromArrayOfObjects(input, optionsForKanbanGOStandard);
  var parsedInput = JSON.parse(input);
  if (parsedInput.result !== undefined) {
    if (parsedInput.result === true || parsedInput.result === "true") {
      outputRaw = "<b style = 'color:green'>Verified</b><br>" + outputRaw;
    } else {
      outputRaw = "<b style = 'color:red'>Failed</b><br>" + outputRaw;
    }
  }
  if (typeof output === "string") {
    output = document.getElementById(output);
  }
  output.innerHTML = outputRaw;
}

TestKanbanGO.prototype.correctFunctions = function() {  
  for (var label in this.theFunctions) {
    var currentCall = this.theFunctions[label];
    if (currentCall.rpcCall === null || currentCall.rpcCall === undefined) {
      currentCall.rpcCall = label; 
      if (label !== kanbanGO.rpcCalls[label].rpcCall) {
        throw(`Fatal error: kanbanGO rpc label ${label} doesn't equal the expecte value ${kanbanGO.rpcCalls[label].rpcCall}.`);
      }
    }
  }
}

var optionsForKanbanGOStandard = {};

TestKanbanGO.prototype.callbackStandard = function(functionLabel, input, output) {
  jsonToHtml.writeJSONtoDOMComponent(input, output, optionsForKanbanGOStandard);
  var theFunction = this.theFunctions[functionLabel];
  if (theFunction.outputs === null || theFunction.outputs === undefined) {
    return;
  }
  var parsedInput = JSON.parse(input);
  for (var label in theFunction.outputs) {
    submitRequests.updateValue(theFunction.outputs[label], parsedInput[label]);
  }
}

TestKanbanGO.prototype.testClear = function() {
  var inputAggregate = ids.defaults.kanbanGO.inputAggregateSignature;
  submitRequests.updateValue(inputAggregate.numberOfPrivateKeysToGenerate, '5');
  submitRequests.updateValue(inputAggregate.privateKeys, '');
  submitRequests.updateValue(inputAggregate.nonces, '');
  submitRequests.updateValue(inputAggregate.publicKeys, '');
  submitRequests.updateValue(inputAggregate.committedSignersBitmap, '01111');
  submitRequests.updateValue(inputAggregate.commitments, '');
  submitRequests.updateValue(inputAggregate.challenge, '');
  submitRequests.updateValue(inputAggregate.aggregateCommitment, '');
  submitRequests.updateValue(inputAggregate.aggregatePublickey, '');
  submitRequests.updateValue(inputAggregate.solutions, '');
  submitRequests.updateValue(inputAggregate.aggregateSignature, '');
}

TestKanbanGO.prototype.run = function(functionLabel) {
  var theFunction = this.theFunctions[functionLabel];
  if (theFunction === null || theFunction === undefined) {
    throw (`Unknown function call label: ${functionLabel}`);
  }
  var theArguments = {};
  var currentInputs = theFunction.inputs;
  for (var inputLabel in currentInputs) {
    theArguments[inputLabel] = document.getElementById(currentInputs[inputLabel]).value;
  }
  var currentInputsBase64 = theFunction.inputsBase64;
  if (currentInputsBase64 !== null && currentInputsBase64 !== undefined) {
    for (var inputLabel in currentInputsBase64) {
      var theValue =  document.getElementById(currentInputsBase64[inputLabel]).value;
      theArguments[inputLabel] = Buffer.from(theValue).toString('base64');
    }
  }
  var messageBody = pathnames.getPOSTBodyFromKanbanGORPCLabel(theFunction.rpcCall, theArguments);
  var theURL = `${pathnames.url.known.goKanbanRPC}`;
  var currentResult = ids.defaults.kanbanGO.outputSchnorr;
  var currentProgress = globals.spanProgress();
  var usePOST = window.kanban.rpc.forceRPCPOST;
  if (!usePOST) {
    if (messageBody.length > 1000) {
      usePOST = true;
    }
  }
  var callbackCurrent = this.callbackStandard;
  if (theFunction.callback !== undefined && theFunction.callback !== null) {
    callbackCurrent = theFunction.callback;
  }  
  callbackCurrent = callbackCurrent.bind(this, functionLabel);

  if (usePOST) {
    submitRequests.submitPOST({
      url: theURL,
      messageBody: messageBody,
      progress: currentProgress,
      callback: callbackCurrent,
      result: currentResult
    });
  } else {
    theURL += `?command=${messageBody}`;
    submitRequests.submitGET({
      url: theURL,
      progress: currentProgress,
      callback: callbackCurrent,
      result: currentResult
    });
  }
}

var testFunctions = new TestKanbanGO();

module.exports = {
  testFunctions
}