"use strict";
const ids = require('./ids_dom_elements');
const kanbanRPC = require('./kanbango/rpc');
const fabcoinInitialization = require('./fabcoin/initialization');
const miscellaneousFrontEnd = require('./miscellaneous_frontend');
const storageKanban = require('./storage').storageKanban;
const themes = require('./themes');
const login = require('./login');

function Page() {

  this.pages = {
    fabcoinInitialization: {
      idPage: ids.defaults.pages.fabcoin.initialization,
    },
    //fabcoinInitializationOLD: {
    //  updateFunction: fabcoinInitialization.updateFabcoinInitializationPage,      
    //},
    fabcoinSmartContract: {
      idPage: ids.defaults.pages.fabcoin.smartContract,
    },
    kanbanGOSendReceive:{
      idPage: ids.defaults.pages.kanbanGOSendReceive
    },
    kanbanGOTransfer: {
      idPage: ids.defaults.pages.kanbanGOTransfer
    },
    sandbox: {
      idPage: ids.defaults.pages.sandbox
    },
    myLocalKanbanNodes: {
      idPage: ids.defaults.pages.kanbanMyLocalNodes,
      updateFunction: kanbanRPC.theKBNodes.getNodeInformation.bind(kanbanRPC.theKBNodes)
    },
    myNodes: {
      idPage: ids.defaults.pages.myNodes,
    },
    kanbanJS: {
      idPage: ids.defaults.pages.kanbanJS,
    },
    fabcoinCrypto: {
      idPage: ids.defaults.pages.fabcoin.crypto,
      updateFunction: null
    },
    kanbanGO: {
      idPage: ids.defaults.pages.kanbanGO,
      updateFunction: null
    },
    themes: {
      idPage: ids.defaults.pages.themes
    },
    demoPage: {
      idPage: ids.defaults.pages.demo
    },
    privacyPolicy: {
      idPage: ids.defaults.pages.privacyPolicy
    },
    loginPage: {
      idPage: ids.defaults.pages.login
    },
    serverStatus: {
      idPage: ids.defaults.pages.serverStatus,
      updateFunction: fabcoinInitialization.initializer.getServerInformation.bind(fabcoinInitialization.initializer),
    }
  };
  this.checkboxBindingsWithId = [[
      storageKanban.variables.autostartFabcoindAfterKanbanGO,
      ids.defaults.kanbanGO.checkboxFabcoindAutostartAfterKanbanGO,
    ], [
      storageKanban.variables.connectKanbansInALine,
      ids.defaults.kanbanGO.checkboxConnectKanbansInALine,
    ], [
      storageKanban.variables.includeFabcoinContractCallsInTransactionOutputs,
      ids.defaults.fabcoin.checkboxes.transactions.contractCallsInOutputs,
    ], [
      storageKanban.variables.includeKanbanGoContractCallsInTransactionOutputs,
      ids.defaults.kanbanGO.checkboxes.transactions.contractCallsInOutputs,
    ], [
      storageKanban.variables.contractCoversFeesFabcoin,
      ids.defaults.fabcoin.checkboxes.transactions.contractCoversFees,
    ], [
      storageKanban.variables.contractCoversFeesKanban,
      ids.defaults.kanbanGO.checkboxes.transactions.contractCoversFees,
    ], [
      storageKanban.variables.noAncestorTransactionFabcoin,
      ids.defaults.fabcoin.checkboxes.transactions.noAncestorTransaction,
    ], [
      storageKanban.variables.noAncestorTransactionKanban,
      ids.defaults.kanbanGO.checkboxes.transactions.noAncestorTransaction,
    ], [
      storageKanban.variables.secretSignsPublicKeyWithoutHashFabcoin,
      ids.defaults.fabcoin.checkboxes.transactions.secretSignsPubkeyNoHash,
    ], [
      storageKanban.variables.secretSignsPublicKeyWithoutHashKanban,
      ids.defaults.kanbanGO.checkboxes.transactions.secretSignsPubkeyNoHash,
    ], 
  ];
}

Page.prototype.initializeCheckBoxes = function() {
  storageKanban.variables.theme.changeValueHandler = themes.setTheme;
  for (var i = 0; i < this.checkboxBindingsWithId.length; i ++) {
    this.checkboxBindingsWithId[i][0].changeValueHandler = miscellaneousFrontEnd.setCheckbox.bind(
      null,
      this.checkboxBindingsWithId[i][1] 
    );
  }
}

Page.prototype.initialize = function() {
  this.initializeInputPlaceholders();
  this.initializeCheckBoxes();
  this.initializePanels();
  storageKanban.loadAll();
  storageKanban.variables.currentPage.changeValueHandler = this.initializeCurrentPage.bind(this);
  this.initializeCurrentPage();
  window.onhashchange = storageKanban.onWindowHashChange.bind(storageKanban);
  if (window.kanban.ace.editor === null) {
    window.kanban.ace.editor = window.kanban.ace.ace.edit('aceEditor');
    window.kanban.ace.editor.getSession().setMode('ace/mode/solidity');
    window.kanban.ace.editor.$blockScrolling = Infinity;
  }
  miscellaneousFrontEnd.hookUpHexWithStringInput(
    ids.defaults.fabcoin.inputCrypto.inputAggregateSignature.message, 
    ids.defaults.fabcoin.inputCrypto.inputAggregateSignature.messageHex
  );
  miscellaneousFrontEnd.hookUpHexWithStringInput(
    ids.defaults.kanbanGO.inputSendReceive.messageVote,
    ids.defaults.kanbanGO.inputSendReceive.messageVoteHex,
  );
  miscellaneousFrontEnd.hookUpHexWithStringInput(
    ids.defaults.demo.inputs.corporationName,
    ids.defaults.demo.inputs.corporationNameHex,
  );
  miscellaneousFrontEnd.hookUpHexWithStringInput(
    ids.defaults.kanbanGO.inputSchnorr.message,
    ids.defaults.kanbanGO.inputSchnorr.messageHex,
  );
  miscellaneousFrontEnd.hookUpHexWithStringInput(
    ids.defaults.fabcoin.inputCrypto.inputSchnorrSignature.messageToSha3,
    ids.defaults.fabcoin.inputCrypto.inputSchnorrSignature.messageToSha3Hex,
  );
  miscellaneousFrontEnd.hookUpHexWithStringInput(
    ids.defaults.kanbanGO.inputAggregateSignature.message,
    ids.defaults.kanbanGO.inputAggregateSignature.messageHex,
  );
  //Load google login:
  gapi.load('auth2', login.login.gapiLoadCallback.bind(login.login));
  kanbanRPC.theKBNodes.getNodeInformation()
}

Page.prototype.initializeInputPlaceholder = function (idInput) {
  var oldInput = document.getElementById(idInput);
  if (oldInput === null) {
    throw(`Input id: ${idInput} not found. `);
  }
  var theInput = oldInput.cloneNode();
  var theParent = oldInput.parentElement;
  var groupContainer = document.createElement("SPAN");
  var label = document.createElement("LABEL");
  label.textContent = theInput.placeholder;
  label.className = "form-control-placeholder";
  groupContainer.className = "form-group";
  theInput.classList.add("form-control");
  groupContainer.appendChild(theInput);
  groupContainer.appendChild(label);
  theParent.replaceChild(groupContainer, oldInput);
  theInput.addEventListener('change', storageKanban.storeInputChange.bind(storageKanban, theInput));
  theInput.addEventListener('keydown', storageKanban.storeInputChange.bind(storageKanban, theInput));
}

Page.prototype.initializePanels = function() {
  var standardPanels = document.getElementsByClassName("panelStandard");
  for (var i = 0; i < standardPanels.length; i ++) {
    var currentPanel = standardPanels[i];
    miscellaneousFrontEnd.makePanel(currentPanel);
    var currentId = currentPanel.id; 
    if (currentId in storageKanban.variables) {
      //variable is registered more than once.
      console.log (`Id ${currentId} already registered. This may or may not be an error`);
      continue;
    }
    storageKanban.variables[currentId] = {
      name: currentId,
      nameLocalStorage: currentId,
      value: null,
      changeValueHandler: miscellaneousFrontEnd.standardExpandButtonHandler.bind(null, currentId)  
    };
  }
}

Page.prototype.initializeInputPlaceholders = function() {
  var collectionsToPlaceholderify = [
    ids.defaults.kanbanGO.inputSchnorr,
    ids.defaults.kanbanGO.inputAggregateSignature,
    ids.defaults.fabcoin.inputCrypto.inputSchnorrSignature, 
    ids.defaults.fabcoin.inputCrypto.inputAggregateSignature,
    ids.defaults.kanbanJS.inputSchnorr,
    ids.defaults.kanbanGO.inputSendReceive,
    ids.defaults.kanbanGO.inputInitialization,
    ids.defaults.fabcoin.inputInitialization,
    ids.defaults.fabcoin.inputBlockInfo,
    ids.defaults.demo.inputs,
    ids.defaults.kanbanGO.inputBenchmarkParameters,
    ids.defaults.myNodes.inputSSH,
    ids.defaults.kanbanGO.inputTransfers,
  ];
  var alreadyLoaded = {};
  for (var collectionCounter = 0; collectionCounter < collectionsToPlaceholderify.length; collectionCounter ++) {
    var currentCollection = collectionsToPlaceholderify[collectionCounter];
    for (var label in currentCollection) {
      var currentId = currentCollection[label];
      if (currentId in alreadyLoaded) {
        continue;
      } 
      alreadyLoaded[currentId] = true;
      this.initializeInputPlaceholder(currentId);
      storageKanban.registerInputBox(currentId);
    }
  }
}

Page.prototype.initializeCurrentPage = function() {
  for (var label in this.pages) {
    var pageId = this.pages[label].idPage;
    document.getElementById(pageId).style.display = "none";
  }
  var currentPageLabel = storageKanban.getVariable(storageKanban.variables.currentPage);
  if (currentPageLabel in this.pages) {
    var pageId = this.pages[currentPageLabel].idPage;
    document.getElementById(pageId).style.display = "";
    var currentPage = this.pages[currentPageLabel];
    if (currentPage.updateFunction !== null && currentPage.updateFunction !== undefined) {
      currentPage.updateFunction();
    }
  }
}

Page.prototype.selectPage = function(pageLabel) {
  storageKanban.setVariable(storageKanban.variables.currentPage, pageLabel, false);
}

function getPage() {
  if (window.kanban.thePage === null || window.kanban.thePage === undefined) {
    window.kanban.thePage = new Page();
  }
  return window.kanban.thePage;
}

module.exports = {
  getPage
}
