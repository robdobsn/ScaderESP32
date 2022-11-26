        ////////////////////////////////////////////////////////////////////////////
        // Cat Deterrent (cat.js)
        ////////////////////////////////////////////////////////////////////////////

        class CatDeterrent {
            constructor() {
                this.scaderName = "ScaderCat";
                this.friendlyName = "Cat Deterrent";
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
                actionsDiv.innerHTML =
                    `<button class='button-bar' onmousedown="${elAccess}.squirtStart();" onmouseup="${elAccess}.squirtStop()">Squirt</button>` +
                    `<button class='button-bar' onclick="${elAccess}.lightOn();">Light On</button>` +
                    `<button class='button-bar' onclick="${elAccess}.lightOff();">Light Off</button>`;

                parentDocElem.appendChild(actionsDiv);
            }

            enableChangeCB(checkbox, elIdx) {
                this.configObj.enable = checkbox.checked;
                console.log("Enable change: " + this.configObj.enable);
            }

            squirtStart(event) {
                console.log(`${this.scaderName} Squirt start`);
                ajaxGet(urlAPIPrefix() + "/cat/squirt/on/2000");
            }

            squirtStop(event) {
                console.log(`${this.scaderName} Squirt stop`);
                ajaxGet(urlAPIPrefix() + "/cat/squirt/off");
            }

            lightOn(event) {
                console.log(`${this.scaderName} Light on`);
                ajaxGet(urlAPIPrefix() + "/cat/light/on");
            }

            lightOff(event) {
                console.log(`${this.scaderName} Light off`);
                ajaxGet(urlAPIPrefix() + "/cat/light/off");
            }

            getLatestData() {
            }

        }
