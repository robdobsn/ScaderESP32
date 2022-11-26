        ////////////////////////////////////////////////////////////////////////////
        // Relays (relays.js)
        ////////////////////////////////////////////////////////////////////////////

        class ScaderRelays {
            constructor() {
                this.scaderName = "ScaderRelays";
                this.friendlyName = "Relays";
                this.configObj = {};
                this.MAX_RELAYS = 24;
                this.relayOnStates = [];
                for (let i = 0; i < this.MAX_RELAYS; i++) {
                    this.relayOnStates[i] = false;
                }
            }
            init() {
                if (!(this.scaderName in window.appState.configObj)) {
                    window.appState.configObj[this.scaderName] = this.defaultConfig();
                }
                this.configObj = window.appState.configObj[this.scaderName]

                // Check there is a relays element in the config
                if (!('relays' in this.configObj)) {
                    this.configObj.relays = [];
                }
                if (this.configObj.relays.length == 0) {
                    this.configObj.relays.push(this.formRelayObj(0));
                }
            }

            getId() {
                return this.scaderName;
            }

            defaultConfig() {
                return {
                    enable: false
                }
            }

            formRelayObj(relayIdx) {
                return {
                    idx: relayIdx,
                    name: "Relay" + (relayIdx + 1),
                }
            }

            updateUI() {
                // Get or create the documentElement
                const mainDocElem = commonGetOrCreateDocElem(this.scaderName);
                mainDocElem.innerHTML = "";

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
                // Check if not enabled
                if (!this.configObj.enable) {
                    return;
                }

                // Check mode
                if (window.appState.uiInSetupMode) {

                    // Handle setup mode
                    const setupDiv = document.createElement("div");
                    setupDiv.id = this.scaderName + "-setup";
                    setupDiv.className = "common-config";

                    // Generate lable for select
                    const label = document.createElement("label");
                    label.className = "common-config-label";
                    label.innerHTML = "Num Relays:";
                    setupDiv.appendChild(label);
                    
                    // Generate the dropdown for the number of relays
                    const numRelaysDropdown = document.createElement("select");
                    numRelaysDropdown.id = this.scaderName + "-numRelays";
                    numRelaysDropdown.className = "common-num-edit";

                    // Add the options
                    for (let i = 0; i < this.MAX_RELAYS; i++) {
                        const option = document.createElement("option");
                        option.value = i + 1;
                        option.innerHTML = i + 1;
                        // Show selection option
                        if (i + 1 === this.configObj.relays.length) {
                            option.selected = true;
                        }
                        numRelaysDropdown.appendChild(option);
                    }

                    // Handle num relays selection
                    numRelaysDropdown.onchange = () => {
                        // Store change
                        this.configObj.relays = [];
                        for (let i = 0; i < numRelaysDropdown.value; i++) {
                            this.configObj.relays.push(this.formRelayObj(i));
                        }
                        console.log("Num relays change: " + numRelaysDropdown.value);

                        // Update the UI
                        this.updateUI();
                    }
                    setupDiv.appendChild(numRelaysDropdown);
                    parentDocElem.appendChild(setupDiv);

                    // Create div for editing relay names
                    const relaysEditDiv = document.createElement("div");
                    relaysEditDiv.id = this.scaderName + "-relays-edit";
                    relaysEditDiv.className = "common-config-line";

                    // Handle edit boxes for names of the relays
                    for (let i = 0; i < this.configObj.relays.length; i++) {
                        const relayDiv = document.createElement("div");
                        relayDiv.id = this.scaderName + "-relay" + (i + 1);
                        relayDiv.className = "common-config-line";
                        
                        // Create label for name of relay
                        const label = document.createElement("label");
                        label.innerHTML = "Relay " + (i + 1) + ":";
                        label.className = "common-config-label";
                        relayDiv.appendChild(label);

                        // Create edit-box for name of relay
                        const nameEdit = document.createElement("input");
                        nameEdit.id = this.scaderName + "-relay" + (i + 1) + "-name";
                        nameEdit.className = "common-name-edit";
                        nameEdit.value = this.configObj.relays[i].name;
                        nameEdit.onchange = () => {
                            // Store change
                            this.configObj.relays[i].name = nameEdit.value;
                            console.log("Relay " + (i + 1) + " name change: " + nameEdit.value);
                        }
                        relayDiv.appendChild(nameEdit);
                        relaysEditDiv.appendChild(relayDiv);
                    }
                    parentDocElem.appendChild(relaysEditDiv);

                } else {

                    // Main div
                    const actionsDiv = document.createElement("div");
                    actionsDiv.id = this.scaderName + "-actions";
                    const elAccess = `window.appState.elems['${this.scaderName}']`;

                    // Create grid for relay buttons
                    const relaysDiv = document.createElement("ul");
                    relaysDiv.id = "relays-grid";
                    relaysDiv.className = "relays-grid";

                    // Create buttons for each relay
                    for (let i = 0; i < this.configObj.relays.length; i++) {
                        const relayDiv = document.createElement("li");
                        relayDiv.id = this.scaderName + "-relay-button" + (i + 1);
                        relayDiv.className = "relays-item";

                        // Generate button
                        const relayButton = this.generateRelayButton(i, this.configObj.relays[i].name, this.relayOnStates[i]);
                        relayDiv.appendChild(relayButton);
                        relaysDiv.appendChild(relayDiv);
                    }

                    parentDocElem.appendChild(relaysDiv);
                }
            }

            enableChangeCB(checkbox, elemIdx) {
                // Store change
                this.configObj.enable = checkbox.checked;
                console.log("Enable change: " + this.configObj.enable);

                // Update the UI
                this.updateUI();
            }

            generateRelayButton(relayIdx, relayName, isOn) {
                const buttonDiv = document.createElement("div");
                buttonDiv.tag = relayIdx;
                buttonDiv.className = "relay-info " + (isOn ? "state-on" : "")

                // Create anchor button
                const anchor = document.createElement("a");
                anchor.href = "#";

                // Button name
                const name = document.createElement("div");
                name.className = "relay-name";
                name.innerHTML = relayName;
                anchor.appendChild(name);

                // Button state icon
                const state = document.createElement("div");
                state.className = "relay-state";
                state.innerHTML = this.getRelayIconHtml(isOn)
                anchor.appendChild(state);

                // Handle button click
                anchor.onclick = (e) => {
                    e.preventDefault();

                    // Send toggle command
                    const currentlyOn = this.relayOnStates[relayIdx];
                    const cmd = urlAPIPrefix() + "/relay/" + (relayIdx + 1) + "/" + (currentlyOn ? "off" : "on") + "/";
                    ajaxGet(cmd, (resp) => {

                        // Update state
                        this.relayOnStates[relayIdx] = !currentlyOn;

                        // Update the UI
                        this.updateUI();
                    });
                }

                buttonDiv.appendChild(anchor);
                return buttonDiv;
            }

            getRelayIconHtml(isOn) {
                const iconName = isOn ? "on-icon" : "off-icon";
                return `<svg width='70' height='70' class='button-icon' fill=${isOn?"white":"black"}><use xlink:href='#${iconName}'></use></svg>\r\n`;
            }

            handleWSMessage(msgData) {
                let isChanged = false;
                if ("relays" in msgData) {
                    for (let i = 0; i < msgData.relays.length; i++) {
                        if (i >= this.relayOnStates.length) {
                            break;
                        }
                        const newState = typeof msgData.relays[i].state === "string" ? msgData.relays[i].state === "on" : msgData.relays[i].state !== 0;
                        if (this.relayOnStates[i] != newState) {
                            isChanged = true;
                        }
                        this.relayOnStates[i] = newState;
                    }
                }
                if (isChanged) {
                    this.updateUI();
                }
            }

            getLatestData() {
                const cmd = urlAPIPrefix() + "/sysmodinfo/" + this.scaderName + "/";
                ajaxGet(cmd, (resp) => {
                    try {
                        const data = JSON.parse(resp);
                        this.handleWSMessage(data);
                    } catch (e) {
                        console.log("ScaderRelays: getLatestData() error parsing JSON: " + e);
                    }
                });
            }
        }
