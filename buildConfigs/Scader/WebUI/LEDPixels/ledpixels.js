        ////////////////////////////////////////////////////////////////////////////
        // LED Pixels (ledpixels.js)
        ////////////////////////////////////////////////////////////////////////////

        class ScaderLEDPixels {
            constructor() {
                this.scaderName = "ScaderLEDPix";
                this.friendlyName = "LED Pixels";
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


                } else {

                }
            }

            enableChangeCB(checkbox, elIdx) {
                // Store change
                this.configObj.enable = checkbox.checked;
                console.log("Enable change: " + this.configObj.enable);

                // Update the UI
                this.updateUI();
            }

            getLatestData() {
            }

        }
