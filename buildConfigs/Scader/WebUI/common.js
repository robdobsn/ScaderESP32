        ////////////////////////////////////////////////////////////////////////////
        // Common (common.js)
        ////////////////////////////////////////////////////////////////////////////

        function urlAPIPrefix() {
            return "/api";
        }

        ////////////////////////////////////////////////////////////////////////////
        // bodyIsLoaded
        ////////////////////////////////////////////////////////////////////////////

        // Function called when body is loaded
        function bodyIsLoaded() {
            // Setup common state
            window.appState = {
                uiInSetupMode: false,
                configObj: {},
                spinner: new Spinner()
            };
            window.appState.elems = {};

            // Add relays
            const scaderRelays = new ScaderRelays();
            window.appState.elems[scaderRelays.getId()] = scaderRelays;

            // Add the Cat Deterrent
            const catDeterrent = new CatDeterrent();
            window.appState.elems[catDeterrent.getId()] = catDeterrent;

            // Add the Conservatory Opener
            const conservatoryOpener = new ConservatoryOpener();
            window.appState.elems[conservatoryOpener.getId()] = conservatoryOpener;

            // Add LED Pixels
            const ledpixels = new ScaderLEDPixels();
            window.appState.elems[ledpixels.getId()] = ledpixels;

            // Add the Plant Waterer
            const plantWaterer = new PlantWaterer();
            window.appState.elems[plantWaterer.getId()] = plantWaterer;
            
            // Get settings and update UI
            getAppSettingsAndUpdateUI();

            // Connect to the websocket
            connectWebsocket();
        }

        ////////////////////////////////////////////////////////////////////////////
        // Get and post the app settings from the server
        ////////////////////////////////////////////////////////////////////////////

        // Get application settings and update UI
        async function getAppSettingsAndUpdateUI()
        {
            const getSettingsResponse = await fetch(urlAPIPrefix() + "/getsettings/nv");
            if (getSettingsResponse.ok) {
                const settings = await getSettingsResponse.json();
                if ("nv" in settings) {
                    // Extract non-volatile settings
                    window.appState.configObj = settings.nv;

                    // Common setup defaults
                    commonSetupDefaults();

                    // Get latest data
                    for (const [key, elem] of Object.entries(window.appState.elems)) {
                        elem.getLatestData();
                    }

                    // Update UI
                    commonUIUpdate();
                } else {
                    alert("getAppSettings settings missing nv section");
                }
            } else {
                alert("getAppSettings failed");
            }
        }

        // Post applicaton settings
        function postAppSettings(okCallback, failCallback) {

            // Set the name and hostname from the common settings
            const commonScaderName = document.getElementById("common-name-field");
            window.appState.configObj.ScaderCommon.name = commonScaderName.value;
            const commonScaderHostname = document.getElementById("common-hostname-field");
            window.appState.configObj.ScaderCommon.hostname = commonScaderHostname.value;
            let jsonStr = JSON.stringify(window.appState.configObj);
            jsonStr = jsonStr.replace("\n", "\\n")
            console.log("postAppSettings " + jsonStr);
            ajaxPost(urlAPIPrefix() + "/postsettings", jsonStr, okCallback, failCallback);
        }

        ////////////////////////////////////////////////////////////////////////////
        // UI Update
        ////////////////////////////////////////////////////////////////////////////

        function commonUIUpdate() {

            // Clear the UI
            document.getElementById("elements-area").innerHTML = "";

            // Generate heading
            commmonGenHeading();

            // Show mode
            commonShowSetupMode();

            // Init element UI
            for (const [key, elem] of Object.entries(window.appState.elems)) {
                elem.updateUI();
            }
        }

        ////////////////////////////////////////////////////////////////////////////
        // Setup mode indication and toggle
        ////////////////////////////////////////////////////////////////////////////

        // Show setup mode info (or not)
        function commonShowSetupMode() {
            let setupStatus = window.appState.uiInSetupMode ? "SETUP MODE" : "";
            const setupModeSpace = document.getElementById("setup-mode-space");
            setupModeSpace.innerHTML = '<p class="setup-mode-ind">' + setupStatus + '</p>';
            const controlButtons = document.getElementById("save-load-settings-buttons");
            const commonSettings = document.getElementById("common-settings-bar");
            if (window.appState.uiInSetupMode) {
                controlButtons.innerHTML =
                    "<button onclick='commonSaveAndRestart();'>Save and Restart Device</button>" +
                    "<button onclick='commonRestartOnly();'>Restart Device</button>" +
                    "<button onclick='commonRestoreDefaults();'>Restore Default Settings</button>";

                // Show the common name field editable
                commonSettings.innerHTML =
                    "<div class='common-settings-bar-item'>" +
                    "<label for='common-name-field' class='common-config-label'>Scader Name</label>" +
                    "<input type='text' id='common-name-field' class='common-name-edit' value='" + window.appState.configObj.ScaderCommon.name + "'>" +
                    "</div>" +
                    "<div class='common-settings-bar-item'>" +
                    "<label for='common-hostname-field' class='common-config-label'>Scader hostname</label>" +
                    "<input type='text' id='common-hostname-field' class='common-name-edit' value='" + window.appState.configObj.ScaderCommon.hostname + "'>" +
                    "</div>";

            } else {
                controlButtons.innerHTML = "";
                commonSettings.innerHTML = "";
            }
        }

        // Toggle setup mode
        function commonToggleSetupMode() {

            // Check if we're entering setup mode
            if (!window.appState.uiInSetupMode) {
                // Copy state
                window.appState.lastSavedConfigJson = JSON.stringify(window.appState.configObj)
            } else {
                if (window.appState.lastSavedConfigJson !== JSON.stringify(window.appState.configObj)) {
                    // Save config
                    commonSaveAndRestart();
                }
            }

            // Toggle mode
            window.appState.uiInSetupMode = !window.appState.uiInSetupMode;

            // Update UI
            commonUIUpdate();
        }

        ////////////////////////////////////////////////////////////////////////////
        // Setup default values
        ////////////////////////////////////////////////////////////////////////////

        function commonSetupDefaults() {
            // Initialize the elements
            for (const [key, elem] of Object.entries(window.appState.elems)) {
                elem.init();
            }

            // Ensure there is a title for the main screen
            if (!("ScaderCommon" in window.appState.configObj)) {
                window.appState.configObj.ScaderCommon = {
                    name: "Scader"
                }
            }
        }

        ////////////////////////////////////////////////////////////////////////////
        // Save and restart
        ////////////////////////////////////////////////////////////////////////////

        // Reset functions
        function commonSaveAndRestart() {
            // Save settings
            postAppSettings(
                () => {
                    console.log("Post settings ok");
                    commonRestartOnly();
                },
                () => {
                    console.log("Post settings failed");
                    alert("Save settings failed")
                },
            );
        }

        function commonRestartOnly() {
            // Reset
            ajaxGet(urlAPIPrefix() + "/reset");
            window.appState.isUIInSetupMode = false;

            // Update UI after a time delay
            const controlButtons = document.getElementById("save-load-settings-buttons");
            window.appState.spinner.spin(controlButtons);
            setTimeout(() => {
                window.appState.uiInSetupMode = false;
                commonUIUpdate();
                window.appState.spinner.stop();
            }, 10000);
        }

        function commonRestoreDefaults() {
            // Restore defaults
            window.appState.configObj = {};
            commonSetupDefaults();
            commonSaveAndRestart();
        }

        ////////////////////////////////////////////////////////////////////////////
        // UI Generation
        ////////////////////////////////////////////////////////////////////////////

        function commmonGenHeading() {
            let headerDiv = document.getElementById("main-header");
            if (window.appState.isUIInSetupMode) {
                headerDiv.innerHTML = "<input id='scader-name' onchange='commonNameChange(this.value)' class='common-name-edit' type='text' name='commonNameVal' value=\"" +
                    window.appState.configObj.ScaderCommon.name + "\">";
            } else {
                headerDiv.innerHTML = window.appState.configObj.ScaderCommon.name;
            }
        }

        function commonGetOrCreateDocElem(elemName) {
            // Get existing
            let elemDocElem = document.getElementById(elemName);
            if (elemDocElem)
                return elemDocElem;
            const elementsArea = document.getElementById("elements-area");
            if (!elementsArea)
                return null;
            elemDocElem = document.createElement("div");
            elemDocElem.id = elemName;
            elementsArea.appendChild(elemDocElem);
            return elemDocElem;
        }

        function commonGenCheckbox(parentEl, elName, elIdx, cbName, label, checked) {
            let elStr = `<div class='common-config'>`;
                elStr += `<input type="checkbox" ${checked ? "checked" : ""} class="common-checkbox" ` +
                        `id="${elName}-${elIdx}-${cbName}-checkbox" name="${elName}-${elIdx}-${cbName}-checkbox" ` +
                        `onclick="window.appState.elems['${elName}'].${cbName}ChangeCB(this, ${elIdx});"'>`;
                elStr += `<label for="${elName}-${elIdx}-${cbName}-checkbox">${label}</label>`;
                elStr += `</div>`;
            parentEl.innerHTML += elStr;
        }

        ////////////////////////////////////////////////////////////////////////////
        // Name change
        ////////////////////////////////////////////////////////////////////////////

        function commonNameChange(newName) {
            window.appState.configObj.ScaderCommon.name = newName;
        }

        ////////////////////////////////////////////////////////////////////////////
        // AJAX
        ////////////////////////////////////////////////////////////////////////////

        // Basic AJAX GET
        function ajaxGet(url, callback) {
            // Handle normally
            let xmlhttp = new XMLHttpRequest();
            xmlhttp.onreadystatechange = function() {
                if (xmlhttp.readyState == 4 && xmlhttp.status == 200) {
                    if (callback)
                        callback(xmlhttp.responseText);
                }
            }
            xmlhttp.open("GET", url, true);
            xmlhttp.send();
        }

        // Basic AJAX POST
        function ajaxPost(url, jsonStrToPos, okCallback, failCallback) {
            const xmlhttp = new XMLHttpRequest();
            xmlhttp.onreadystatechange = function() {
                if (xmlhttp.readyState === 4) {
                    if (xmlhttp.status === 200) {
                        if ((okCallback !== undefined) && (typeof okCallback !== 'undefined'))
                            okCallback(xmlhttp.responseText);
                    } else {
                        if ((failCallback !== undefined) && (typeof failCallback !== 'undefined'))
                            failCallback(xmlhttp);
                    }
                }
            };
            xmlhttp.open("POST", url);
            xmlhttp.setRequestHeader("Content-Type", "application/json");
            xmlhttp.send(jsonStrToPos);
        }

        ////////////////////////////////////////////////////////////////////////////
        // Connect Websocket
        ////////////////////////////////////////////////////////////////////////////

        function connectWebsocket()
        {
            // Setup webSocket
            if (!window.webSocket) 
            {
                console.log('Attempting WebSocket connection');
                window.webSocket = new WebSocket('ws://' + location.host + '/scader');
                window.webSocket.binaryType = "arraybuffer";

                window.webSocket.onopen = function(evt) 
                {
                    console.log('WebSocket connection opened');
                    // Subscribe to scader messages
                    for (const [key, elem] of Object.entries(window.appState.elems)) {
                        const subscribeName = elem.getId();
                        window.webSocket.send(JSON.stringify({
                            cmdName: "subscription",
                            action: "update",
                            pubRecs: [
                                {name: subscribeName, msgID: subscribeName, rateHz: 0.1},
                            ]
                        }));
                    }
                }
                
                window.webSocket.onmessage = function(evt) 
                {
                    let msg = "";
                    if (evt.data instanceof ArrayBuffer) {
                        msg = new TextDecoder().decode(evt.data);
                    } else {
                        msg = evt.data;
                    }
                    // console.log(`WebSocket message received: ${msg}`);
                    if (msg.length > 0) {
                        const msgObj = JSON.parse(msg);
                        // console.log(msgObj);
                        for (const [key, elem] of Object.entries(window.appState.elems)) {
                            if (elem.handleWSMessage) {
                                elem.handleWSMessage(msgObj);
                            }
                        }
                    }
                }
                
                window.webSocket.onclose = function(evt) 
                {
                    console.log('Websocket connection closed - attempting reconnect');
                    window.webSocket = null;
                    setTimeout(connectWebsocket, 2000);
                }
                
                window.webSocket.onerror = function(evt) 
                {
                    console.log('Websocket error: ' + evt);
                }
            }
        }


