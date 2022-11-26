        ////////////////////////////////////////////////////////////////////////////
        // Plant Waterer (cat.js)
        ////////////////////////////////////////////////////////////////////////////

        class PlantWaterer {
            constructor() {
                this.scaderName = "ScaderWaterer";
                this.friendlyName = "Plant Waterer";
                this.configObj = {};
                this.MAX_PLANTS = 4;
                this.moistureGauges = [];
                this.moistureLabels = [];
                for (let i = 0; i < this.MAX_PLANTS; i++) {
                    this.moistureGauges.push(null);
                    this.moistureLabels.push(null);
                }
            }
            init() {
                if (!(this.scaderName in window.appState.configObj)) {
                    window.appState.configObj[this.scaderName] = this.defaultConfig();
                }
                this.configObj = window.appState.configObj[this.scaderName]

                // Check there is a plants element in the config
                if (!('plants' in this.configObj)) {
                    this.configObj.plants = [];
                }
                if (this.configObj.plants.length == 0) {
                    // Add plants to the config
                    for (let i = 0; i < this.MAX_PLANTS; i++) {
                        this.configObj.plants.push(this.formPlantObj(i));
                    }
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

            formPlantObj(idx) {
                return {
                    idx: idx,
                    name: "Plant " + (idx + 1),
                    waterLevel: 20,
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
                const mainDiv = document.createElement("div");
                mainDiv.id = this.scaderName + "-plants";
                parentDocElem.appendChild(mainDiv);
                // Create a column inside the div for each plant
                for (let i = 0; i < this.configObj.plants.length; i++) {
                    const plant = this.configObj.plants[i];
                    const plantDiv = document.createElement("div");
                    plantDiv.id = this.scaderName + "-plant-" + i;
                    plantDiv.className = "plant-div";
                    mainDiv.appendChild(plantDiv);
                    // Add the plant name
                    const plantName = document.createElement("div");
                    plantName.id = this.scaderName + "-plant-name-" + i;
                    plantName.className = "plant-name";
                    plantName.innerHTML = plant.name;
                    plantDiv.appendChild(plantName);
                    // Add the plant enable checkbox
                    commonGenCheckbox(plantDiv, this.scaderName, i, 'enablePlant', 'Enable', plant.enable);
                    // Add the plant enable checkbox change callback
                    // const enablePlantCheckbox = document.getElementById(this.scaderName + "-enablePlant-" + i);
                    // enablePlantCheckbox.addEventListener("change", this.enablePlantChangeCB.bind(this));
                    // Add the plant enable checkbox change callback
                    // // Add the plant enable button
                    // const plantEnable = document.createElement("button");
                    // plantEnable.id = this.scaderName + "-plant-enable-" + i;
                    // plantEnable.className = "plant-enable";
                    // plantEnable.innerHTML = "Enable";
                    // plantEnable.onclick = this.enablePlantChangeCB.bind(this);
                    // plantDiv.appendChild(plantEnable);

                    // Add a div for the gauge and the gauge value
                    const gaugeDiv = document.createElement("div");
                    gaugeDiv.id = this.scaderName + "-plant-gauge-" + i;
                    gaugeDiv.className = "plant-gauge";
                    plantDiv.appendChild(gaugeDiv);

                    // Add meter showing plant water level
                    const plantMeter = document.createElement("meter");
                    plantMeter.id = this.scaderName + "-plant-meter-" + i;
                    plantMeter.className = "plant-meter";
                    plantMeter.min = 0;
                    plantMeter.max = 100;
                    plantMeter.value = 20;
                    gaugeDiv.appendChild(plantMeter);
                    this.moistureGauges[i] = plantMeter;

                    // Add the value for the water level
                    const plantValue = document.createElement("div");
                    plantValue.id = this.scaderName + "-plant-value-" + i;
                    plantValue.className = "plant-value";
                    plantValue.innerHTML = plant.waterLevel + " %";
                    gaugeDiv.appendChild(plantValue);
                    this.moistureLabels[i] = plantValue;

                    // Add a label for the plant water level input
                    const plantInputId = this.scaderName + "-plant-input-" + i;
                    const plantInputLabel = document.createElement("label");
                    plantInputLabel.id = plantInputId + "-label";
                    plantInputLabel.className = "plant-input-label";
                    plantInputLabel.innerHTML = "Req. %";
                    plantInputLabel.htmlFor = plantInputId;
                    plantDiv.appendChild(plantInputLabel);

                    // Input box for setting plant water level
                    const plantInput = document.createElement("input");
                    plantInput.id = plantInputId;
                    plantInput.className = "plant-input";
                    plantInput.type = "number";
                    plantInput.min = 0;
                    plantInput.max = 100;
                    plantInput.value = 20;
                    plantInput.dataset.idx = i;
                    plantDiv.appendChild(plantInput);
                    // Add the plant input change callback
                    plantInput.addEventListener("change", this.plantInputChangeCB.bind(this));
                    
                    
                    // Add the plant water button
                    // const plantWaterButton = document.createElement("button");
                    // plantWaterButton.id = this.scaderName + "-plant-water-button-" + i;
                    // plantWaterButton.className = "plant-water-button";
                    // plantWaterButton.innerHTML = "Water now";
                    // plantWaterButton.addEventListener("click", this.squirtStart.bind(this));
                    // plantDiv.appendChild(plantWaterButton);
                    // // Add the plant light button
                    // const plantLightButton = document.createElement("button");
                    // plantLightButton.id = this.scaderName + "-plant-light-button-" + i;
                    // plantLightButton.className = "plant-light-button";
                    // plantLightButton.innerHTML = "Light";
                    // plantLightButton.addEventListener("click", this.lightOn.bind(this));
                    // plantDiv.appendChild(plantLightButton);
                }

                // const actionsDiv = document.createElement("div");
                // actionsDiv.id = this.scaderName + "-actions";
                // const elAccess = `window.appState.elems['${this.scaderName}']`;
                // actionsDiv.innerHTML =
                //     `<button class='button-bar' onmousedown="${elAccess}.squirtStart();" onmouseup="${elAccess}.squirtStop()">Squirt</button>` +
                //     `<button class='button-bar' onclick="${elAccess}.lightOn();">Light On</button>` +
                //     `<button class='button-bar' onclick="${elAccess}.lightOff();">Light Off</button>`;

                // parentDocElem.appendChild(actionsDiv);
            }

            enableChangeCB(checkbox, elemIdx) {
                this.configObj.enable = checkbox.checked;
                console.log("Enable change: " + this.configObj.enable);
            }

            enablePlantChangeCB(checkbox, elemIdx) {
                this.configObj.plants[elemIdx].enable = checkbox.checked;
                console.log(`Enable change plant ${elemIdx} to ${this.configObj.plants[elemIdx].enable}`);
            }

            plantInputChangeCB(ev) {
                const elemIdx = ev.target.dataset.idx;
                this.configObj.plants[elemIdx].waterLevel = ev.target.value;
                console.log(`Water level change plant ${elemIdx} to ${this.configObj.plants[elemIdx].waterLevel}`);
            }

            handleWSMessage(msgData) {
                let isChanged = false;
                if ("moisture" in msgData) {
                    for (let i = 0; i < msgData.moisture.length; i++) {
                        if (i >= this.moistureGauges.length) {
                            break;
                        }
                        const meterVal = msgData.moisture[i];
                        this.moistureGauges[i].value = meterVal;
                        this.moistureLabels[i].innerHTML = meterVal + " %";
                        // const newState = msgData.relays[i].state === "on";
                        // if (this.relayOnStates[i] != newState) {
                        //     isChanged = true;
                        // }
                        // this.relayOnStates[i] = newState;
                    }
                }
                if (isChanged) {
                    this.updateUI();
                }
            }

            getLatestData() {
            }
        }
