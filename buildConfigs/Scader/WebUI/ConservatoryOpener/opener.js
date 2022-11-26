        ////////////////////////////////////////////////////////////////////////////
        // Conservatory Opener (opener.js)
        ////////////////////////////////////////////////////////////////////////////

        class ConservatoryOpener {
            constructor() {
                this.scaderName = "ScaderOpener";
                this.friendlyName = "Conservatory Opener";
                this.configObj = {};
            }
            init() {
                if (!(this.scaderName in window.appState.configObj)) {
                    window.appState.configObj[this.scaderName] = this.defaultConfig();
                }
                this.configObj = window.appState.configObj[this.scaderName]
            }

            getId() {
                return this.scaderName;
            }

            defaultConfig() {
                return {
                    enable: false
                }
            }

            updateUI() {
                // Get or create the documentElement
                const mainDocElem = commonGetOrCreateDocElem(this.scaderName);

                // Enables
                this.updateUIEnables(mainDocElem);

                // Generate the div for the main UI
                const mainDiv = document.createElement("div");
                mainDiv.id = this.scaderName + "-main";
                mainDocElem.appendChild(mainDiv);

                // Populate the main div
                this.updateMainDiv(mainDocElem);
            }

            updateUIEnables(parentDocElem) {
                // UI for enablement
                if (window.appState.uiInSetupMode) {
                    // Form the enable checkbox
                    commonGenCheckbox(parentDocElem, this.scaderName, 0, 'enable', 
                                'Enable ' + this.friendlyName, this.configObj.enable);

                }
            }

            updateMainDiv(parentDocElem) {
                // Check if not enabled or setup mode
                if (!this.configObj.enable || window.appState.uiInSetupMode) {
                    return;
                }
                // Form the main div
                const actionsDiv = document.createElement("div");
                actionsDiv.id = this.scaderName + "-actions";
                const elAccess = `window.appState.elems['${this.scaderName}']`;

                // Generate the mode dropdown
                let actionsHTML = this.genModeDropdown(elAccess);

                // Generate the actions
                actionsHTML += 
                    `<button class='button-bar' onclick="${elAccess}.doorOpen();">Open Door</button>` +
                    `<button class='button-bar' onclick="${elAccess}.doorClose();">Close Door</button>` +
                    `<button class='button-bar' onclick="${elAccess}.doorStop();">Stop Door</button>`;

                actionsDiv.innerHTML = actionsHTML;
                parentDocElem.appendChild(actionsDiv);
            }

            // Function to generate the "mode" dropdown
            genModeDropdown(accessElement) {
                let modeOptions = {
                    "out": "Out Only",
                    "in": "In Only",
                    "both": "In and Out",
                    "none": "Disable"
                };

                // Populate dropdown
                let dropDownHTML = "<div class='common-config'>";
                dropDownHTML += 'Opener Mode: ';
                dropDownHTML += `<select class="common-num-edit" onchange="${accessElement}.setOpenerMode(this.value)">`;
                for (const key in modeOptions) {
                    const isSelected = (key == this.configObj.mode) ? "selected" : "";
                    dropDownHTML += `<option value="${key}" ${isSelected}>${modeOptions[key]}</option>`;
                }
                dropDownHTML += "</select>";
                dropDownHTML += "</div>";
                return dropDownHTML;
            }

            enableChangeCB(checkbox, elIdx) {
                this.configObj.enable = checkbox.checked;
                console.log("Enable change: " + this.configObj.enable);
            }

            setOpenerMode(mode) {
                console.log(`Opener mode ${mode}`);
                ajaxGet(urlAPIPrefix() + "/opener/mode/" + mode);
            }

            doorOpen(event) {
                console.log("Opener open door");
                ajaxGet(urlAPIPrefix() + "/opener/open");
            }

            doorClose(event) {
                console.log("Opener close door");
                ajaxGet(urlAPIPrefix() + "/opener/close");
            }
            doorStop(event) {
                console.log("Opener stop door");
                ajaxGet(urlAPIPrefix() + "/opener/stop");
            }

            getLatestData() {
            }

        }




// ////////////////////////////////////////////////////////////////////////////
//         // Opener (opener.js)
//         ////////////////////////////////////////////////////////////////////////////

//         // Called on page load
//         function openerBodyIsLoaded() {
//             window.openerPageState = {
//                 name: "DoorOpener",
//                 doorState: false,
//                 refreshTimer: null,
//                 lastMsgRx: new Date(),
//                 sysVers: ""
//             };
//             // requestState();
//             // // EventSource
//             // if (!!window.EventSource) {
//             //     connectEventSource();
//             // }
//             // // Timer updates
//             // window.openerPageState.connCheckTimer = setInterval(checkConn, 1000);
//             // window.openerPageState.refreshTimer = setInterval(requestState, 5000);
//         }

//         function connectEventSource() {
//             window.openerPageState.eventSource = new EventSource('/events');
//             window.openerPageState.eventSource.addEventListener('open', function (e) {
//                 console.log("Events Connected");
//                 window.openerPageState.lastMsgRx = new Date();
//             }, false);
//             window.openerPageState.eventSource.addEventListener('error', function (e) {
//                 if (e.target.readyState != EventSource.OPEN) {
//                     console.log("Events Disconnected");
//                 }
//             }, false);
//             window.openerPageState.eventSource.addEventListener('message', function (e) {
//                 console.log("message", e.data);
//                 window.openerPageState.lastMsgRx = new Date();
//                 if (window.openerPageState.isEditMode) {
//                     let logText = document.getElementById("log-text-area");
//                     logText.value += e.data + "\n";
//                     if (logText.value.length > 30000) {
//                         logText.value = logText.value.slice(-30000);
//                     }
//                 }
//             }, false);
//             window.openerPageState.eventSource.addEventListener('status', function (e) {
//                 console.log("status", e.data);
//                 window.openerPageState.lastMsgRx = new Date();
//                 stateInfoCallback(e.data);
//                 if (window.openerPageState.isEditMode) {
//                     jsonData = JSON.parse(e.data);
//                     window.openerPageState.sysVers = jsonData.version;
//                     let versionText = document.getElementById("version-text");
//                     versionText.innerHTML = "Version: " + window.openerPageState.sysVers;
//                 }
//             }, false);
//         }

//         function checkConn() {
//             if (!window.openerPageState.runInTestMode) {
//                 let msElapsed = new Date() - window.openerPageState.lastMsgRx;
//                 if (msElapsed > 10000) {
//                     if (window.openerPageState.eventSource)
//                         window.openerPageState.eventSource.close();
//                     connectEventSource();
//                     console.log("Reconnecting event source elapsedMs " + msElapsed);
//                 }
//             }
//         }

//         // Called to request state info
//         function requestStateInfo() {
//             ajaxGet("/q", stateInfoCallback);
//         }

//         // Called with data from the /q command
//         function stateInfoCallback(jsonResp) {

//             // Extract info
//             if (extractStateInfo(jsonResp)) {
//                 regenerateHtmlPage();
//             } else {
//                 updateStatus();
//             }
//         }

//         // Extract information from the JSON provided
//         function extractStateInfo(jsonResp) {
//             let infoChanged = false;
//             jsonData = JSON.parse(jsonResp);
//             infoChanged = window.openerPageState.doorState != jsonData.doorState;
//             return infoChanged;
//         }

//         // Regenerate the HTML for the page with updated data
//         function regenerateHtmlPage() {
//             genHeadings();
//             genGrid();
//             genLogText();
//         }

//         // Callback to refresh display after status update, etc
//         function updateStatus() {

//             // // Update elements
//             // let relayInfo = document.getElementsByClassName("relay-info");
//             // for (let i = 0; i < relayInfo.length; i++) {
//             // 	let relayIdx = parseInt(relayInfo.item(i).getAttribute("tag"));
//             // 	let exStr = "off";
//             //     let stateOn = false;
//             //     if ("state" in window.openerPageState.relays[relayIdx])
//             //     {
//             //         exStr = window.openerPageState.relays[relayIdx].state == "on" ? "on" : "off";
//             //         stateOn = window.openerPageState.relays[relayIdx].state == "on";
//             //     }
//             //     if (stateOn)
//             //         relayInfo.item(i).classList.add("state-on");
//             //     else
//             //         relayInfo.item(i).classList.remove("state-on");
//             //     let relayStateElem = relayInfo.item(i).getElementsByClassName("relay-state")
//             //     relayStateElem[0].innerHTML = getDeviceIconHtml(stateOn);
//             // }
//         }

//         // Generate headings
//         function genHeadings() {
//             // let relaysData = window.openerPageState;
//             // let headerDiv = document.getElementById("devices-header");
//             // let headStr = "";
//             // if (!window.openerPageState.isEditMode) {
//             // 	headStr += relaysData.name;
//             // }
//             // else {
//             // 	headStr += "<input id='devices-header-name' class='window-name-edit' type='text' name='windowNameVal' value='" + relaysData.name + "'></div>\r\n";
//             // }
//             // headerDiv.innerHTML = headStr;
//         }

//         // Generate the grid
//         function genGrid() {
//             // let isEditMode = window.openerPageState.isEditMode;
//             // let relaysData = window.openerPageState;
//             // let relaysDiv = document.getElementById("devices-grid");
//             // let listStr = "";
//             // for (let relayIdx = 0; relayIdx < relaysData.numRelays; relayIdx++) {
//             // 	listStr += "<li class='relays-item'>\r\n";
//             // 	let relayInfo = relaysData.relays[relayIdx];
//             // 	listStr += getDeviceItemHtml(isEditMode, relayInfo, relayIdx);
//             // 	listStr += '</li>\r\n';
//             // }
//             // relaysDiv.innerHTML = listStr;
//         }

//         function genLogText() {
//             let logText = document.getElementById("log-text");
//             let logTextStr = "";
//             if (window.openerPageState.isEditMode) {
//                 logTextStr = '<textarea id="log-text-area"></textarea>';
//             }
//             logText.innerHTML = logTextStr;
//         }

//         function buttonToggleCallback(relayIdx, isOn) {
//             // window.openerPageState.relays[relayIdx].state = ((isOn == "on") ? "off" : "on");
//             // updateRelaysStatus();
//         }

//         // function buttonToggle(relayIdx) {
//         //     let relayInfo = window.openerPageState.relays[relayIdx];
//         //     let relayNum = relayInfo.num;
//         //     let commandStr = (relayInfo.state == "on") ? "off" : "on";
//         //     if (!window.openerPageState.runInTestMode)
//         //     {
//         //         ajaxGet("/relay/" + relayNum + "/" + commandStr + "/", function() { buttonToggleCallback(relayIdx, relayInfo.state); });
//         //     }
//         //     else
//         //     {
//         //         window.openerPageState.testData.relays[relayIdx].state = commandStr;
//         //         buttonToggleCallback(relayIdx, window.openerPageState.testData.relays[relayIdx].state);
//         //     }
//         // }

//         function getDeviceIconHtml(isOn) {
//             let iconName = isOn ? "on-icon" : "off-icon";
//             return "<svg width='70' height='70' class='button-icon'><use xlink:href = '#" + iconName + "'></use></svg>\r\n";
//         }

